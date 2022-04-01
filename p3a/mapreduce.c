#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include "mapreduce.h"
#include "hashmap.h"

// a struct to store the key-value pair
struct kv {
    char* key;
    char* value;
};

// a struct that acts as a bucket: 
//      * elements          all key-value pairs
//      * num_elements      the number of pairs
//      * size              the max size of elements
//      * counter           for iterating using get_func()
//      * partition_lock    used to ensure adding a pair to a partition is atomic
struct bucket {
    struct kv** elements;
    size_t num_elements;
    size_t size;
    int counter;
    pthread_mutex_t partition_lock;
};

// global variables:
//      * data              contains the mapped key-value pairs
//      * num_partitions    the number of paritions in the data
//      * file_lock         lock used to ensure file assignment to thread is atomic
//      * curr_file         keeps track of the file to assign next
//      * total_files       the total number of files
//      * files             array of all file names provided
struct bucket* data;
int num_partitions;
pthread_mutex_t file_lock;
int curr_file;
int total_files;
char** files;

Mapper m;
Reducer r;
Partitioner p;

// initializes the data structure and the buckets within it
void init(size_t list_size, int partitions) {
    data = (struct bucket*) malloc(partitions * sizeof(struct bucket));

    for (int i = 0; i < partitions; i++) {
        data[i].elements = (struct kv**) malloc(list_size * sizeof(struct kv*));
        data[i].num_elements = 0;
        data[i].size = list_size;
        data[i].counter = 0;
        pthread_mutex_init(&(data[i].partition_lock), NULL);
    }     
}

// key compare function for qsort
int cmp(const void* a, const void* b) {
    char* str1 = (*(struct kv**) a)->key;
    char* str2 = (*(struct kv**) b)->key;
    return strcmp(str1, str2);
}

// creates key-value pair and adds it to the data structure
void MR_Emit(char* key, char* value) {
    struct kv* new = (struct kv*) malloc(sizeof(struct kv));
    
    if (new == NULL) {
        printf("Malloc error\n");
        exit(1);
    }

    new->key = strdup(key);
    new->value = strdup(value);

    int idx = (*p) (new->key, num_partitions);

    pthread_mutex_lock(&(data[idx].partition_lock));
    if (data[idx].num_elements == data[idx].size) {
        data[idx].size *= 2;
        data[idx].elements = realloc(data[idx].elements, data[idx].size * sizeof(struct kv*));
    }
    
    data[idx].elements[data[idx].num_elements++] = new;
    pthread_mutex_unlock(&(data[idx].partition_lock));

    return;
}

// default partitioner function
unsigned long MR_DefaultHashPartition(char* key, int num_partitions) {
    unsigned long hash = 5381;
    int c;
    while ((c = *key++) != '\0')
        hash = hash * 33 + c;
    return hash % num_partitions;
}

// gets the value associated with the given key
char* get_func(char* key, int idx) {
    if (data[idx].counter == data[idx].num_elements) {
        return NULL;
    }

    struct kv* curr_kv = data[idx].elements[data[idx].counter];
    if (!strcmp(curr_kv->key, key)) {
        data[idx].counter++;
        return curr_kv->value;
    }

    return NULL;
}

// map thread routine (no argument) to assign a file to a thread and run the map function
void* map_routine(void* arg) {
    while (1) {
        pthread_mutex_lock(&file_lock);
        if (curr_file > total_files) {
            pthread_mutex_unlock(&file_lock);
            break;
        }

        char* filename = files[curr_file];
        curr_file++; 
        pthread_mutex_unlock(&file_lock);

        (*m) (filename);
    }

    return NULL;
}


// reduce thread routine (takes partition number as argument) to run the reduce function
void* reduce_routine(void* arg) {
    int* idx = (int*) arg;
    
    while (data[*idx].counter < data[*idx].num_elements) {
        (*r) ((data[*idx].elements[data[*idx].counter])->key, get_func, *idx);
    }
    //for (int i = 0; i < data[*idx].num_elements; i++) {    
    //    (*r) ((data[*idx].elements[i])->key, get_func, *idx);
    //}

    return NULL;
}

void MR_Run(int argc, char* argv[], Mapper map, int num_mappers, Reducer reduce, int num_reducers, Partitioner partition) {
    // initialization
    num_partitions = num_reducers;
    curr_file = 1;
    total_files = argc - 1;
    files = argv;
    init(10, num_partitions);

    m = map;
    r = reduce;
    p = partition;
    
    pthread_t map_threads[num_mappers];
    pthread_t reduce_threads[num_reducers];

    pthread_mutex_init(&file_lock, NULL);

    // create map threads and wait for them to complete
    for (int i = 0; i < num_mappers; i++) {
        pthread_create(&map_threads[i], NULL, map_routine, NULL);
    }

    for (int i = 0; i < num_mappers; i++) {
        pthread_join(map_threads[i], NULL);
    }

    // sort partitions in ascending order
    for (int i = 0; i < num_partitions; i++) {
        qsort(data[i].elements, data[i].num_elements, sizeof(struct kv*), cmp);
    }

    // create reduce threads and wait for them to complete
    for (int i = 0; i < num_reducers; i++) {
        void* partition_num = malloc(sizeof(void*));
        *(int*) partition_num = i;
        pthread_create(&reduce_threads[i], NULL, reduce_routine, partition_num);
    }

    for (int i = 0; i < num_reducers; i++) {
        pthread_join(reduce_threads[i], NULL);
    }
    
    // TODO: lock cleanup
    // TODO: memory cleanup
    
    return;
}

