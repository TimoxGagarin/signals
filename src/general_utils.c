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

char *name_by_pid(pid_t pid)
{
    char proc_path[256];
    snprintf(proc_path, sizeof(proc_path), "/proc/%d/comm", pid);

    FILE *file = fopen(proc_path, "r");
    if (!file)
    {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    char *name = (char *)malloc(256);
    if (!name)
    {
        perror("Error allocating memory");
        exit(EXIT_FAILURE);
    }

    if (!fgets(name, 256, file))
    {
        fprintf(stderr, "Error reading file\n");
        free(name); // Освободите память, если чтение не удалось
        exit(EXIT_FAILURE);
    }

    name = (char *)realloc(name, strlen(name) + 1);
    if (!name)
    {
        perror("Error reallocating memory");
        exit(EXIT_FAILURE);
    }

    name[strlen(name) - 1] = '\0';
    fclose(file);
    return name;
}

pid_t pid_by_name(const char *process_name)
{
    DIR *dir = opendir("/proc");
    struct dirent *entry;
    int pid = -1;

    entry = readdir(dir);
    while (dir && entry)
    {
        entry = readdir(dir);
        pid_t current_pid = atoi(entry->d_name);
        if (current_pid > 0 && !strcmp(name_by_pid(current_pid), process_name))
        {
            pid = current_pid;
            break;
        }
    }
    closedir(dir);

    return pid;
}

void free_children(pid_t **children)
{
    for (int i = 0; children[i]; i++)
        free(children[i]);
    free(children);
}