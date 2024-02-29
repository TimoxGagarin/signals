#include <stdio.h>
#include <stdlib.h>
#include "headers/parent_utils.h"
#include <sys/prctl.h>

// Главная функция программы
int main(int argc, char *argv[])
{
    // Инициализация обработчиков сигналов
    initSignalHandlers();

    // Проверка корректного количества аргументов командной строки
    if (argc != 2)
    {
        printf("Неверное количество аргументов. Правильное использование программы:\n");
        printf("./parent <путь/к/файлу>\n");
        exit(EXIT_FAILURE);
    }

    // Установка имени процесса с использованием prctl
    if (prctl(PR_SET_NAME, "P", 0, 0, 0))
    {
        perror("prctl PR_SET_NAME");
        exit(EXIT_FAILURE);
    }

    // Обработка опций и выполнение соответствующих действий
    choose_options(argv[1]);

    // Завершение программы с успешным кодом завершения
    exit(EXIT_SUCCESS);
}