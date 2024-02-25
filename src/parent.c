#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include "headers/parent_utils.h"
#include <dirent.h>
#include <sys/prctl.h>
#include <signal.h>

int main(int argc, char *argv[])
{
    initSignalHandlers();

    if (argc != 2)
    {
        printf("Неверное количество аргументов. Правильное использование программы:\n");
        printf("./parent <путь/к/файлу>\n");
        exit(EXIT_FAILURE);
    }

    if (prctl(PR_SET_NAME, "P", 0, 0, 0) != NULL)
    {
        perror("prctl PR_SET_NAME");
        exit(EXIT_FAILURE);
    }

    // Обработка опций и выполнение соответствующих действий
    choose_options(argv[1]);
    exit(EXIT_SUCCESS);
}