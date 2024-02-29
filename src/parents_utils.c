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
#include <errno.h>
#include <time.h>
#include <sys/select.h>

#include "headers/parent_utils.h"
#include "headers/general_utils.h"
#include <bits/types/siginfo_t.h>

struct sigaction printSignal;
struct sigaction intSignal;
struct sigaction alarmSignal;

/**
 * @brief Получает массив дочерних процессов (C_k) для текущего родительского процесса.
 *
 * @return Массив дочерних процессов (завершенный нулевым указателем).
 */
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
    if (!dir)
    {
        perror("Ошибка при открытии /proc");
        exit(EXIT_FAILURE);
    }
    // Читаем содержимое директории /proc
    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        if (!atoi(entry->d_name) || (atoi(entry->d_name) && atoi(entry->d_name) < ppid))
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

/**
 * @brief Устанавливает обработчик сигнала для вывода статистики.
 *
 * @param sig Номер сигнала.
 * @param info Структура с информацией о сигнале.
 * @param context Контекст выполнения.
 */
void setPrint(int sig, siginfo_t *info, void *context)
{
    if (info->si_pid == getpid())
        return;

    char can_print[256];
    snprintf(can_print, 256, "%s_CAN_PRINT", name_by_pid(info->si_pid));

    if (atoi(getenv(can_print)))
        kill(info->si_pid, SIGUSR1);
}

/**
 * @brief Обработчик сигнала завершения (SIGINT).
 *
 * @param sig Номер сигнала.
 */
void handleExit(int sig)
{
    close_child_processes();
    exit(EXIT_SUCCESS);
}

/**
 * @brief Обработчик сигнала таймера (SIGALRM).
 *
 * @param sig Номер сигнала.
 */
void alarmHandler(int sig)
{
    if (sig == SIGALRM)
        allow_children_print(true);
}

/**
 * @brief Инициализирует обработчики сигналов.
 */
void initSignalHandlers()
{
    printSignal.sa_sigaction = setPrint; // Устанавливает обработчик сигнала для вывода статистики
    printSignal.sa_flags = SA_SIGINFO | SA_RESTART;
    sigaction(SIGUSR1, &printSignal, NULL); // Устанавливает printSignal в пользу сигнала SIGUSR1

    intSignal.sa_handler = handleExit;
    sigaction(SIGINT, &intSignal, NULL);

    alarmSignal.sa_handler = alarmHandler;
    alarmSignal.sa_flags = SA_SIGINFO;
    sigaction(SIGALRM, &alarmSignal, NULL);
}

/**
 * @brief Обрабатывает опции, вводимые пользователем, и выполняет соответствующие действия.
 *
 * @param child_path Путь к программе, которую необходимо выполнить в дочернем процессе.
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
            char name[18];
            snprintf(name, 18, "C_%s", option + 1);
            name[strlen(name) - 1] = '\0';
            pid_t pid = pid_by_name(name);
            if (pid == -1)
            {
                puts("Process not found");
                continue;
            }
            else
                kill(pid, SIGUSR1);
            alarm(5);
        }
        else if (option[0] == 'q')
        {
            // Родительский процесс удаляет все C_k, сообщает об этом и завершается.
            close_child_processes();
            free(option);
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
        usleep(1500);
        kill(pid, SIGUSR1);
    }
}

/**
 * @brief Выводит список родительских и дочерних процессов.
 */
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

/**
 * @brief Завершает все дочерние процессы.
 */
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
        waitpid(*(children[i]), NULL, 0);
    }
    free_children(children);
}

/**
 * @brief Завершает последний дочерний процесс.
 */
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
    waitpid(*(children[count - 1]), NULL, 0);

    free_children(children);
}

/**
 * @brief Разрешает или запрещает вывод статистики для всех дочерних процессов.
 *
 * @param can Флаг, разрешающий (true) или запрещающий (false) вывод статистики.
 */
void allow_children_print(bool can)
{
    pid_t **children = get_children();
    for (int i = 0; children[i]; i++)
    {
        char can_print[256];
        snprintf(can_print, 256, "%s_CAN_PRINT", name_by_pid(*(children[i])));
        char val[16];
        sprintf(val, "%i", can);
        setenv(can_print, val, 1);
        if (can)
        {
            usleep(1000000);
            kill(*(children[i]), SIGUSR1);
        }
    }
    free_children(children);
}

/**
 * @brief Разрешает или запрещает вывод статистики для конкретного дочернего процесса.
 *
 * @param pnum Номер дочернего процесса.
 * @param can Флаг, разрешающий (true) или запрещающий (false) вывод статистики.
 */
void allow_child_print(int pnum, bool can)
{
    DIR *dir;
    struct dirent *entry;

    char name[16];
    char can_print[256];
    snprintf(name, 256, "C_%i", pnum);
    snprintf(can_print, 256, "%s_CAN_PRINT", name);

    char val[16];
    sprintf(val, "%i", can);
    setenv(can_print, val, 1);
    kill(pid_by_name(name), SIGUSR1);
}