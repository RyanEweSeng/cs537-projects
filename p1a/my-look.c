#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

void clean_string(char *str) {
    int i = 0;
    int j = 0;
    char ch;

    while ((ch = str[i++]) != '\0') {
        if ( (ch >= 48 && ch <= 57) || (ch >= 65 && ch <= 90) || (ch >= 97 && ch <= 122) ) str[j++] = ch;
    }
    str[j] = '\0';
}

void print_words(FILE *fp, char keyword[]) {
    if (fp == NULL) {
        printf("my-look: cannot open file\n");
        exit(1);
    }

    int bufferLen = 255;
    char buffer[bufferLen];
    char temp[bufferLen];
    while (fgets(buffer, bufferLen, fp)) {
        strcpy(temp, buffer);
        clean_string(temp); // remove non-alpha numeric
        if (strncasecmp(temp, keyword, strlen(keyword)) == 0) printf("%s", buffer);
    }
}

int main(int argc, char *argv[]) {
    opterr = 0;
    int opt;
    FILE *fp;
    while ((opt = getopt(argc, argv, "Vhf:")) != -1) {
        switch (opt) {
            case 'V':
                printf("my-look from CS537 Spring 2022\n");
                exit(0);
            case 'h':
                printf("Usage: my-look -V -h -f <filename> <keyword>\n");
                exit(0);
            case 'f':
                fp = fopen(optarg, "r");
                print_words(fp, argv[argc - 1]);
                fclose(fp);
                exit(0);
            case '?':
                printf("my-look: invalid command line\n");    
                exit(1);
        }
    }

    print_words(stdin, argv[argc - 1]);
    return 0;
}

