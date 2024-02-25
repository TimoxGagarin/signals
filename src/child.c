#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <time.h>
#include <signal.h>
#include <sys/prctl.h>
#include <ncurses.h>

typedef struct stat // структура статистики доч процесса
{
    int res00;
    int res01;
    int res10;
    int res11;
} stat_t;

typedef struct pair // структура таймера
{
    int first;
    int second;

} pair_t;

bool EndAlarm, Print = true;
pair_t pair = {0, 0};
stat_t all_stat = {0, 0};
struct sigaction printSignal, alarmSignal, sigsegvSignal;

void setPrint(int sign) // printSignal
{
    Print = true;
}

void firealarm(int sign)
{
    exit(1);
}

void setAlarm(int sign)
{
    EndAlarm = true;
    if (pair.first == pair.second) // расчеты для статистики
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

void initSignalHandlers()
{
    printSignal.sa_handler = setPrint;      // printSignal отвечает за сигнал SIGUSR1,
    sigaction(SIGUSR1, &printSignal, NULL); // который будет послан родительским процессом для печати статистики работы дочернего процесса

    alarmSignal.sa_handler = setAlarm;
    sigaction(SIGALRM, &alarmSignal, NULL);
}

int main(int argc, char *argv[])
{
    char can_print_var[256];

    if (prctl(PR_SET_NAME, argv[0], 0, 0, 0) != NULL)
    {
        perror("prctl PR_SET_NAME");
        exit(EXIT_FAILURE);
    }

    printf("New child process pid: %d, ppid: %d  \n", getpid(), getppid()); // вывод параметров
    initSignalHandlers();

    char buf[256]; // буфер для вывода статистики
    int count = 100;
    while (count--)
    {
        bool fl;
        EndAlarm = false;
        ualarm(10000, 0); // таймер на 1 с
        while (!EndAlarm)
        {
            pair.first = fl ? 1 : 0;  // если true то 1, иначе 0
            pair.second = fl ? 1 : 0; // если true то 1, иначе 0
            fl = fl ? false : true;
        }
    }
    // загрузка статистики в буфер
    snprintf(buf, 256, "%s 00: %d, 01: %d, 10: %d, 11: %d", argv[0], all_stat.res00, all_stat.res01, all_stat.res10, all_stat.res11);

    while (true)
    {
        if (Print)
        {
            usleep(1000);
            Print = false;
            puts(buf);
            usleep(300);
            kill(getppid(), SIGUSR1); // род процессу отпр сигнал SIGUSR1
        }
        if (strcmp(name_by_pid(getppid()), "systemd") == 0)
            exit(EXIT_SUCCESS);
    }
    exit(EXIT_SUCCESS);
}