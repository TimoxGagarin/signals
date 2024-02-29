#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/prctl.h>

/**
 * @brief Получает имя процесса по его PID.
 *
 * @param pid Идентификатор процесса (PID).
 * @return Указатель на строку с именем процесса.
 */
char *name_by_pid(pid_t pid)
{
    char proc_path[256];
    int path_length = snprintf(proc_path, sizeof(proc_path), "/proc/%d/comm", pid);

    // Проверка на ошибки при формировании пути к файлу
    if (path_length < 0 || path_length >= (int)sizeof(proc_path))
    {
        perror("Error forming file path");
        exit(EXIT_FAILURE);
    }

    // Открытие файла с именем процесса
    FILE *file = fopen(proc_path, "r");
    if (!file)
    {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    // Выделение памяти для имени процесса
    char *name = (char *)malloc(256);
    if (!name)
    {
        perror("Error allocating memory");
        exit(EXIT_FAILURE);
    }

    // Чтение имени процесса из файла
    if (!fgets(name, 256, file))
    {
        if (feof(file))
            fprintf(stderr, "End of file reached\n");
        else
            fprintf(stderr, "Error reading file\n");

        free(name);
        fclose(file);
        exit(EXIT_FAILURE);
    }

    // Убедимся, что строка завершается символом '\0'
    name[strcspn(name, "\n")] = '\0';

    // Переаллокация памяти под точное количество символов
    char *temp = (char *)realloc(name, strlen(name) + 1);
    if (!temp)
    {
        perror("Error reallocating memory");
        free(name);
        fclose(file);
        exit(EXIT_FAILURE);
    }

    name = temp;

    // Закрытие файла
    if (fclose(file))
    {
        perror("Error closing file");
        free(name);
        exit(EXIT_FAILURE);
    }

    // Возвращение указателя на строку с именем процесса
    return name;
}

/**
 * @brief Получает PID процесса по его имени.
 *
 * @param process_name Имя процесса.
 * @return PID процесса, -1 в случае ошибки или отсутствия процесса с заданным именем.
 */
pid_t pid_by_name(const char *process_name)
{
    // Открытие директории /proc
    DIR *dir = opendir("/proc");
    if (!dir)
    {
        perror("Error opening /proc directory");
        return -1;
    }

    // Чтение содержимого директории /proc
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL)
    {
        pid_t current_pid = atoi(entry->d_name);
        if (current_pid > 0)
        {
            char *name = name_by_pid(current_pid);
            if (name && !strcmp(name, process_name))
            {
                free(name);
                closedir(dir);
                return current_pid;
            }
            free(name);
        }
    }

    // Закрытие директории /proc
    closedir(dir);
    return -1;
}

/**
 * @brief Освобождает память, выделенную для массива дочерних процессов.
 *
 * @param children Указатель на массив дочерних процессов (PID), завершенный нулевым указателем.
 */
void free_children(pid_t **children)
{
    for (int i = 0; children[i]; i++)
        free(children[i]);
    free(children);
}