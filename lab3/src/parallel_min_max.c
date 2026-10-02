#include <ctype.h>                  
#include <limits.h>                 
#include <stdbool.h>                
#include <stdio.h>                  
#include <stdlib.h>              
#include <string.h>               
#include <unistd.h>                
#include <sys/time.h>                
#include <sys/types.h>              
#include <sys/wait.h>               
#include <getopt.h>                 

#include "find_min_max.h"           
#include "utils.h"              

int main(int argc, char **argv) {
  int seed = -1;                    // Seed генератора случайных чисел
  int array_size = -1;              // Размер массива
  int pnum = -1;                    // Количество дочерних процессов
  bool with_files = false;          // true — использовать файлы, false — pipe

  while (true) {
    int current_optind = optind ? optind : 1; // Текущая позиция аргумента

    static struct option options[] = {
        {"seed", required_argument, 0, 0},       
        {"array_size", required_argument, 0, 0}, 
        {"pnum", required_argument, 0, 0},        
        {"by_files", no_argument, 0, 'f'},       
        {0, 0, 0, 0}
    };

    int option_index = 0;            // Индекс найденной опции
    int c = getopt_long(argc, argv, "f", options, &option_index); // Читаем аргумент

    if (c == -1) break;              // Если аргументы закончились — выходим из цикла

    switch (c) {
      case 0:
        switch (option_index) {
          case 0:
            seed = atoi(optarg);     // Получаем seed
            if (seed <= 0) {        
              printf("seed must be positive\n");
              return 1;
            }
            break;

          case 1:
            array_size = atoi(optarg); // Получаем размер массива
            if (array_size <= 0) {    
              printf("array_size must be positive\n");
              return 1;
            }
            break;

          case 2:
            pnum = atoi(optarg);     // Получаем количество процессов
            if (pnum <= 0) {         
              printf("pnum must be positive\n");
              return 1;
            }
            break;

          case 3:
            with_files = true;      // Включаем режим работы через файлы
            break;

          default:
            printf("Index %d is out of options\n", option_index);
        }
        break;

      case 'f':
        with_files = true;           // Короткая форма -f включает файлы
        break;

      case '?':
        return 1;                    // Неизвестный аргумент

      default:
        printf("getopt returned character code 0%o?\n", c);
        return 1;
    }
  }

  if (optind < argc) {               // Проверяем наличие лишних аргументов
    printf("Has at least one no option argument\n");
    return 1;
  }

  if (seed == -1 || array_size == -1 || pnum == -1) { // Проверяем обязательные параметры
    printf("Usage: %s --seed \"num\" --array_size \"num\" --pnum \"num\" [--by_files]\n", argv[0]);
    return 1;
  }

  if (pnum > array_size) {           // Не создаём больше процессов, чем элементов
    printf("pnum must not be greater than array_size\n");
    return 1;
  }

  int *array = malloc(sizeof(int) * array_size); // Выделяем память под массив
  if (array == NULL) {               
    printf("Memory allocation failed\n");
    return 1;
  }

  GenerateArray(array, array_size, seed); // Заполняем массив

  int active_child_processes = 0;     // Количество созданных процессов

  int (*pipes)[2] = malloc(sizeof(int[2]) * pnum); // Создаём pipe для каждого процесса
  pid_t *child_pids = malloc(sizeof(pid_t) * pnum); // Массив PID дочерних процессов

  if (pipes == NULL || child_pids == NULL) { // Проверяем выделение памяти
    printf("Memory allocation failed\n");
    free(array);
    free(pipes);
    free(child_pids);
    return 1;
  }

  if (!with_files) {                 // Pipe нужны только если не используем файлы
    for (int i = 0; i < pnum; i++) {
      if (pipe(pipes[i]) == -1) {    // Создаём pipe для i-го процесса
        printf("Pipe failed!\n");
        free(array);
        free(pipes);
        free(child_pids);
        return 1;
      }
    }
  }

  struct timeval start_time;         // Время начала работы
  gettimeofday(&start_time, NULL);   // Запоминаем время

  for (int i = 0; i < pnum; i++) {
    pid_t child_pid = fork();        // Создаём дочерний процесс

    if (child_pid >= 0) {
      active_child_processes += 1;   // Увеличиваем количество процессов

      if (child_pid == 0) {          // Этот код выполняет дочерний процесс
        unsigned int begin = (unsigned int)((long long)i * array_size / pnum); // Начало части
        unsigned int end = (unsigned int)((long long)(i + 1) * array_size / pnum); // Конец части

        struct MinMax result = GetMinMax(array, begin, end); // Ищем min и max своей части

        if (with_files) {            // Если выбран режим файлов
          char filename[64];         
          sprintf(filename, "min_max_%d.txt", i); // Создаём уникальное имя

          FILE *file = fopen(filename, "w"); // Открываем файл для записи
          if (file == NULL) {         // Проверяем открытие файла
            printf("Cannot open file\n");
            free(array);
            free(pipes);
            free(child_pids);
            exit(1);
          }

          fprintf(file, "%d %d\n", result.min, result.max); // Записываем min и max
          fclose(file);              // Закрываем файл
        } else {
          close(pipes[i][0]);        // Ребёнку не нужна сторона чтения своего pipe

          write(pipes[i][1], &result, sizeof(struct MinMax)); // Передаём результат родителю

          close(pipes[i][1]);        // Закрываем сторону записи
        }

        free(array);                 // Освобождаем память ребёнка
        free(pipes);                 // Освобождаем массив pipe
        free(child_pids);             // Освобождаем массив PID
        exit(0);                     // Завершаем дочерний процесс
      }

      child_pids[i] = child_pid;     // Сохраняем PID дочернего процесса

    } else {
      printf("Fork failed!\n");      
      free(array);
      free(pipes);
      free(child_pids);
      return 1;
    }
  }

  if (!with_files) {                 // Родителю нужны только стороны чтения
    for (int i = 0; i < pnum; i++) {
      close(pipes[i][1]);            // Закрываем стороны записи в родителе
    }
  }

  while (active_child_processes > 0) {
    wait(NULL);                      
    active_child_processes -= 1;     
  }

  struct MinMax min_max;             // Общий результат
  min_max.min = INT_MAX;             // Начальное значение общего минимума
  min_max.max = INT_MIN;             // Начальное значение общего максимума

  for (int i = 0; i < pnum; i++) {
    int min = INT_MAX;               // Минимум i-й части
    int max = INT_MIN;               // Максимум i-й части

    if (with_files) {                // Получаем результат из файла
      char filename[64];             
      sprintf(filename, "min_max_%d.txt", i); // Формируем имя файла

      FILE *file = fopen(filename, "r"); // Открываем файл для чтения
      if (file == NULL) {            // Проверяем открытие
        printf("Cannot open file\n");
        free(array);
        free(pipes);
        free(child_pids);
        return 1;
      }

      fscanf(file, "%d %d", &min, &max); // Читаем min и max
      fclose(file);                 
      remove(filename);             // Удаляем временный файл
    } else {
      struct MinMax result;          // Структура для результата дочернего процесса

      read(pipes[i][0], &result, sizeof(struct MinMax)); // Читаем результат из pipe
      close(pipes[i][0]);             // Закрываем pipe после чтения

      min = result.min;              // Получаем минимум
      max = result.max;              // Получаем максимум
    }

    if (min < min_max.min) min_max.min = min; // Обновляем общий минимум
    if (max > min_max.max) min_max.max = max; // Обновляем общий максимум
  }

  struct timeval finish_time;        // Время окончания работы
  gettimeofday(&finish_time, NULL);  // Получаем текущее время

  double elapsed_time = (finish_time.tv_sec - start_time.tv_sec) * 1000.0; // Секунды в миллисекунды
  elapsed_time += (finish_time.tv_usec - start_time.tv_usec) / 1000.0; // Добавляем микросекунды

  free(array);                       // Освобождаем массив
  free(pipes);                       // Освобождаем массив pipe
  free(child_pids);                  // Освобождаем массив PID

  printf("Min: %d\n", min_max.min);  // Выводим общий минимум
  printf("Max: %d\n", min_max.max);  // Выводим общий максимум
  printf("Elapsed time: %fms\n", elapsed_time); // Выводим время выполнения
  fflush(NULL);                      // Очищаем буферы вывода

  return 0;                          
}

