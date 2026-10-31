#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>     
#include <sys/types.h>
#include <unistd.h>

/* Универсальный макрос: печатает адрес объекта как uintptr_t */
#define SHW_ADR(ID, I) \
    (printf("ID %-8s \t is at virtual address: 0x%016" PRIXPTR "\n", \
            ID, (uintptr_t)&(I)))

extern int etext, edata, end;   /* маркеры границ сегментов (от линкера) */

char *cptr = "This message is output by the function showit()\n"; /*Литерал лежит в .rodata, а сам указатель cptr в data, потому что инициализирован.*/
char buffer1[25]; /*неинициализированный попадает в BSS*/

int showit(char *p);            /* прототип с явным типом аргумента */

int main(void) {
    int i = 0; /* стек: локальная переменная main */

    printf("\nAddress etext: 0x%016" PRIXPTR "\n", (uintptr_t)&etext);                   
    printf("Address edata: 0x%016" PRIXPTR "\n", (uintptr_t)&edata); /* &edata конец .data инициализированные глобальные*/ 
    printf("Address end  : 0x%016" PRIXPTR "\n", (uintptr_t)&end); /* &end конец .bss, начало кучи */
                                                          

    SHW_ADR("main",    main); /* .text: адрес функции, код */
    SHW_ADR("showit",  showit); /* .text: адрес функции, код */
    SHW_ADR("cptr",    cptr); /* .data: сама переменная инициализирована но значение cptr указывает в .rodata (литерал) */
    SHW_ADR("buffer1", buffer1); /* .bss: глобальный массив без инициализации */
    SHW_ADR("i",       i); /* стек: локальная переменная main */

    strcpy(buffer1, "A demonstration\n"); /* пишем в .bss; литерал-источник лежит в .rodata */
    write(1, buffer1, strlen(buffer1) + 1);
    showit(cptr); 
    return 0;
}

int showit(char *p) {
    char *buffer2; /* стек: локальная переменная*/
    SHW_ADR("buffer2", buffer2); /* стек: печатается &buffer2, а не значение */
    if ((buffer2 = (char *)malloc((unsigned)(strlen(p) + 1))) != NULL) { /* malloc выделяет память в куче*/
        printf("Allocated memory at 0x%016" PRIXPTR "\n", (uintptr_t)buffer2); 
        strcpy(buffer2, p); /* копия строки теперь в куче */
        printf("%s", buffer2); /* читаем из кучи */
        free(buffer2); /* освобождаем кучу */
    } else {
        printf("Allocation error\n");
        exit(1);
    }
    return 0;
}