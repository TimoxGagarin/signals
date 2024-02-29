#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <time.h>
#include <signal.h>
#include <sys/prctl.h>

typedef struct stat // Структура статистики дочернего процесса
{
    int res00;
    int res01;
    int res10;
    int res11;
} stat_t;

typedef struct pair // Структура для таймера
{
    int first;
    int second;
} pair_t;

volatile sig_atomic_t EndAlarm = false;
volatile sig_atomic_t Print = false;
pair_t pair = {0, 0};
stat_t all_stat = {0, 0, 0, 0};
struct sigaction setPrintSignal;
struct sigaction alarmSignal;

/**
 * @brief Устанавливает обработчик сигнала для вывода статистики.
 *
 * @param sign Номер сигнала.
 */
void setPrint(int sign) // Обработчик сигнала для вывода статистики
{
    if (sign == SIGUSR1)
        Print = true;
}

/**
 * @brief Устанавливает обработчик сигнала для таймера и расчета статистики.
 *
 * @param sign Номер сигнала.
 */
void setAlarm(int sign)
{
    if (sign != SIGALRM)
        return;

    EndAlarm = true;

    // Расчет статистики
    if (pair.first == pair.second)
    {
        if (pair.first == 1)
        {
            all_stat.res11++;
        }
        else
        {
            all_stat.res00++;
        }
    }
    else
    {
        if (pair.first == 0)
        {
            all_stat.res01++;
        }
        else
        {
            all_stat.res10++;
        }
    }
}

/**
 * @brief Инициализирует обработчики сигналов.
 */
void initSignalHandlers()
{
    setPrintSignal.sa_handler = setPrint; // Обработчик сигнала для вывода статистики
    sigaction(SIGUSR1, &setPrintSignal, NULL);

    alarmSignal.sa_handler = setAlarm; // Обработчик сигнала для таймера и расчета статистики
    sigaction(SIGALRM, &alarmSignal, NULL);
}

int main(int argc, char *argv[])
{
    pid_t default_ppid = getppid();
    if (prctl(PR_SET_NAME, argv[0], 0, 0, 0))
    {
        perror("prctl PR_SET_NAME");
        exit(EXIT_FAILURE);
    }

    printf("New child process pid: %d, ppid: %d  \n", getpid(), getppid()); // Вывод информации о процессе
    initSignalHandlers();

    while (true)
    {
        char buf[256]; // Буфер для вывода статистики
        int count = 100;

        bool fl = false;
        while (count--)
        {
            EndAlarm = false;
            ualarm(10000, 0);
            while (!EndAlarm)
            {
                pair.first = fl ? 1 : 0;  // Если true, то 1, иначе 0
                pair.second = fl ? 1 : 0; // Если true, то 1, иначе 0
                fl = fl ? false : true;
            }
        }

        // Загрузка статистики в буфер
        snprintf(buf, 256, "%s 00: %d, 01: %d, 10: %d, 11: %d", argv[0], all_stat.res00, all_stat.res01, all_stat.res10, all_stat.res11);

        if (Print)
        {
            Print = false;
            puts(buf);
            sleep(2);
            kill(default_ppid, SIGUSR1); // Отправка сигнала SIGUSR1 родительскому процессу для вывода статистики
        }
        if (getppid() != default_ppid)
            exit(EXIT_SUCCESS);
    }
}