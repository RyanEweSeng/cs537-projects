#include "types.h"
#include "stat.h"
#include "user.h"

struct iostat {
    int readcount;
    int writecount;
};

int main(int argc, char *argv[]) {
    int m, n;
    m = atoi(argv[1]);
    n = atoi(argv[2]);

    struct iostat *iostat_ptr = malloc(sizeof(struct iostat));
    
    for (int i = 0; i < m; i++) { // call m number of reads
       read(-1, "", 1); 
    }

    for (int i = 0; i < n; i++) { // call n number of writes
        write(-1, "", 1);
    }
    
    int o = getiocounts(iostat_ptr);
    printf(1, "%d %d %d\n", o, m, n);
    printf(1, "readcount: %d\n", iostat_ptr->readcount);
    printf(1, "writecount: %d\n", iostat_ptr->writecount);
    free(iostat_ptr);

    exit();
}
