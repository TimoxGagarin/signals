#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <dirent.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <ncurses.h>
#include <errno.h>
#include <time.h>
#include <sys/select.h>
#include <curses.h>

#include "headers/parent_utils.h"
#include "headers/general_utils.h"
#include <bits/types/siginfo_t.h>

struct sigaction printSignal, intSignal;

pid_t **get_children()
{
    pid_t ppid = getppid();
    pid_t **ret = (pid_t **)malloc(sizeof(pid_t *));
    int count = 0;
    ret[count] = (pid_t *)malloc(sizeof(pid_t));
    DIR *dir;
    struct dirent *entry;

    // Открываем директорию /proc
    dir = opendir("/proc");
    if (dir)
    {
        perror("Ошибка при открытии /proc");
        exit(EXIT_FAILURE);
    }
    // Читаем содержимое директории /proc
    while (entry = readdir(dir))
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        if (!atoi(entry->d_name) || atoi(entry->d_name) && atoi(entry->d_name) < ppid)
            continue;
        char full_path[264];
        snprintf(full_path, 264, "/proc/%s", entry->d_name);
        struct stat fileStat;
        if (lstat(full_path, &fileStat) == -1)
            continue;

        if (S_ISDIR(fileStat.st_mode))
        {
            // Проверяем, является ли имя директории числом (это может быть pid процесса)
            char *endptr;
            pid_t pid = strtol(entry->d_name, &endptr, 10);

            if (*endptr == '\0')
            {
                // Это числовое имя, проверим, является ли это дочерним процессом
                char proc_path[256];
                snprintf(proc_path, sizeof(proc_path), "/proc/%d/stat", pid);

                FILE *file = fopen(proc_path, "r");
                if (file)
                {
                    // Читаем информацию из файла /proc/<pid>/stat
                    long ppid = 0;
                    fscanf(file, "%*d %*s %*c %ld", &ppid);
                    fclose(file);

                    // Если ppid соответствует pid родительского процесса, то это дочерний процесс
                    if (ppid == getpid())
                    {
                        count++;
                        ret = (pid_t **)realloc(ret, (count + 1) * sizeof(pid_t *));
                        ret[count] = (pid_t *)malloc(sizeof(pid_t));
                        *(ret[count - 1]) = pid;
                    }
                }
            }
        }
    }
    closedir(dir);
    ret[count] = NULL;
    return ret;
}

void setPrint(int sig, siginfo_t *info, void *context)
{
    char can_print[256];
    snprintf(can_print, 256, "%s_CAN_PRINT", name_by_pid(info->si_pid));
    if (atoi(getenv(can_print)))
    {
        usleep(150000);
        kill(info->si_pid, SIGUSR1);
    }
}

void handleExit(int sig)
{
    close_child_processes();
    exit(EXIT_SUCCESS);
}

void initSignalHandlers()
{
    printSignal.sa_sigaction = setPrint; // ссылка на функцию setPrint
    printSignal.sa_flags = SA_SIGINFO | SA_RESTART;
    sigaction(SIGUSR1, &printSignal, NULL); // устанавливает printSignal в пользвл сигнал SIGUSR1

    intSignal.sa_handler = handleExit;
    sigaction(SIGINT, &intSignal, NULL);
}

/**
 * @brief Обрабатывает опции, вводимые пользователем, и выполняет соответствующие действия.
 */
void choose_options(const char *child_path)
{
    printf("Вход в цикл обработки нажатий клавиши...\n");
    char *option = (char *)malloc(16);
    while (1)
    {
        if (fgets(option, 16, stdin) == NULL)
        {
            perror("fgets");
        }
        else if (option[0] == '+')
        {
            // Родительский процесс порождает дочерний процесс (C_k) и сообщает об этом.
            createChildProcess(child_path);
        }
        else if (option[0] == '-')
        {
            // Родительский процесс удаляет последний порожденный C_k, сообщает об этом и о количестве оставшихся.
            close_last_child_processes();
        }
        else if (option[0] == 'l')
        {
            // Родительский процесс выводит перечень родительских и дочерних процессов.
            list_child_processes();
        }
        else if (option[0] == 'k')
        {
            // Родительский процесс удаляет все C_k и сообщает об этом.
            close_child_processes();
        }
        else if (option[0] == 's')
        {
            // Если передано в виде "s<num>", родительский процесс запрещает C_<num> выводить статистику.
            // Иначе, запрещает всем C_k выводить статистику
            if (option[1] == '\n')
            {
                allow_children_print(false);
            }
            else
            {
                allow_child_print(atoi(option + 1), false);
            }
        }
        else if (option[0] == 'g')
        {
            // Если передано в виде "g<num>", родительский процесс разрешает C_<num> выводить статистику.
            // Иначе, разрешает всем C_k выводить статистику.
            if (option[1] == '\n')
            {
                allow_children_print(true);
            }
            else
            {
                allow_child_print(atoi(option + 1), true);
            }
        }
        else if (option[0] == 'p')
        {
            // Родительский процесс запрещает всем C_k вывод и запрашивает C_<num> вывести свою статистику.
            // По истечению заданного времени (5 с, например), если не введен символ «g»,
            // разрешает всем C_k снова выводить статистику.
            allow_children_print(false);
            allow_child_print(atoi(option + 1), true);
        }
        else if (option[0] == 'q')
        {
            // Родительский процесс удаляет все C_k, сообщает об этом и завершается.
            close_child_processes();
            break;
        }
        else
        {
            // Вывод ошибки, если введена неизвестная команда
            printf("Неизвестная команда: %s\n", option);
        }
    }
}

/**
 * @brief Создает дочерний процесс и выполняет указанную программу с определенными аргументами и окружением.
 *
 * @param child_path Путь к программе, которую необходимо выполнить в дочернем процессе.
 */
void createChildProcess(const char *child_path)
{
    static int child_number = 0;
    // Установка имени программы (argv[0]) как "C_k"
    char child_name[50];
    snprintf(child_name, sizeof(child_name), "C_%d", child_number);
    char can_print[256];
    snprintf(can_print, 256, "%s_CAN_PRINT", child_name);

    // Инкрементация номера для следующего запуска
    child_number++;

    // Запуск дочернего процесса
    pid_t pid = fork();

    if (pid == -1)
    {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    else if (pid == 0)
    {
        // Замена текущего процесса кодом нового процесса
        char full_child_path[256];
        snprintf(full_child_path, strlen(child_path) + 1, "%s/child", child_path);

        execl(full_child_path, child_name, NULL);

        // В случае ошибки при выполнении execve
        perror("execve");
        exit(EXIT_FAILURE);
    }
    else
    {
        setenv(can_print, "1", 1);
        // Ждем завершения дочернего процесса
        waitpid(pid, NULL, WNOHANG);
    }
}

void list_child_processes()
{
    char proccess_name[256];
    prctl(PR_GET_NAME, proccess_name, 0, 0, 0);
    printf("Родительский процесс\nName: %s; PID: %d\n", proccess_name, getpid());
    printf("Дочерние процессы:\n");

    pid_t **children = get_children();
    for (int i = 0; children[i]; i++)
        printf("Name: %s; PID: %d\n", name_by_pid(*(children[i])), *(children[i]));
    free_children(children);
}

void close_child_processes()
{
    pid_t **children = get_children();
    for (int i = 0; children[i]; i++)
    {
        char *pname = name_by_pid(*(children[i]));
        if (kill(*(children[i]), SIGKILL) != 0)
        {
            perror("Ошибка при завершении процесса");
            continue;
        }
        printf("Процесс %s(%d) был удален\n", pname, *(children[i]));
        char can_print[256];
        snprintf(can_print, 256, "%s_CAN_PRINT", pname);
        unsetenv(can_print);
        waitpid(*(children[i]), NULL, NULL);
    }
    free_children(children);
}

void close_last_child_processes()
{
    pid_t **children = get_children();
    int count = 0;
    for (; children[count]; count++)
        ;

    if (count == 0)
    {
        puts("Дочерние процессы не найдены.");
        free_children(children);
        return;
    }

    char *pname = name_by_pid(*(children[count - 1]));
    if (kill(*(children[count - 1]), SIGKILL) != 0)
    {
        perror("Ошибка при завершении процесса");
        free_children(children);
        return;
    }
    printf("Процесс %s(%d) был удален. Осталось %d процессов\n", pname, *(children[count - 1]), count - 1);

    char can_print[256];
    snprintf(can_print, 256, "%s_CAN_PRINT", pname);
    unsetenv(can_print);
    waitpid(*(children[count - 1]), NULL, NULL);

    free_children(children);
}

void allow_children_print(bool can)
{
    pid_t **children = get_children();
    for (int i = 0; children[i]; i++)
    {
        char can_print[256];
        snprintf(can_print, 256, "%s_CAN_PRINT", name_by_pid(*(children[i])));
        char val[16];
        sprintf(val, 16, can);
        setenv(can_print, val, 1);
    }
    free_children(children);
    raise(SIGUSR1);
}

void allow_child_print(int pnum, bool can)
{
    DIR *dir;
    struct dirent *entry;

    char can_print[256];
    sprintf(can_print, 256, "C_%d_CAN_PRINT", pnum);

    char val[16];
    sprintf(val, 16, can);
    setenv(can_print, val, 1);
    raise(SIGUSR1);
}