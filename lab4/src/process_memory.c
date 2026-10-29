#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>     /* для PRIXPTR */
#include <sys/types.h>
#include <unistd.h>

/* Универсальный макрос: печатает адрес объекта как uintptr_t */
#define SHW_ADR(ID, I) \
    (printf("ID %-8s \t is at virtual address: 0x%016" PRIXPTR "\n", \
            ID, (uintptr_t)&(I)))

extern int etext, edata, end;   /* маркеры границ сегментов (от линкера) */

char *cptr = "This message is output by the function showit()\n";
char buffer1[25];

int showit(char *p);            /* прототип с явным типом аргумента */

int main(void) {
    int i = 0;

    printf("\nAddress etext: 0x%016" PRIXPTR "\n", (uintptr_t)&etext);
    printf("Address edata: 0x%016" PRIXPTR "\n", (uintptr_t)&edata);
    printf("Address end  : 0x%016" PRIXPTR "\n", (uintptr_t)&end);

    SHW_ADR("main",    main);
    SHW_ADR("showit",  showit);
    SHW_ADR("cptr",    cptr);
    SHW_ADR("buffer1", buffer1);
    SHW_ADR("i",       i);

    strcpy(buffer1, "A demonstration\n");
    write(1, buffer1, strlen(buffer1) + 1);   /* системный вызов */
    showit(cptr);

    return 0;
}

int showit(char *p) {
    char *buffer2;
    SHW_ADR("buffer2", buffer2);
    if ((buffer2 = (char *)malloc((unsigned)(strlen(p) + 1))) != NULL) {
        printf("Allocated memory at 0x%016" PRIXPTR "\n", (uintptr_t)buffer2);
        strcpy(buffer2, p);
        printf("%s", buffer2);
        free(buffer2);
    } else {
        printf("Allocation error\n");
        exit(1);
    }
    return 0;
}