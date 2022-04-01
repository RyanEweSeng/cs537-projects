#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int does_not_contain(char buffer[], char substring[]) {
    int flag = 1;
    char *ptr;
    for (ptr  = substring; *ptr != '\0'; ptr++) {
        if(strchr(buffer, *ptr)) flag = 0;
    }
    return flag;
} 

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("wordle: invalid number of args\n");
        exit(1);
    }
    
    FILE *fp = fopen(argv[1], "r");
    if (fp == NULL) {
        printf("wordle: cannot open file\n");
        exit(1);
    }
   
    int bufferLength = 255;
    char buffer[bufferLength];
    while (fgets(buffer, bufferLength, fp)) {
        if (strlen(buffer) - 1 == 5 && does_not_contain(buffer, argv[2])) printf("%s", buffer);
    }
    fclose(fp);    

    return 0;
}

