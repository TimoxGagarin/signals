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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *name_by_pid(pid_t pid)
{
    char proc_path[256];
    int path_length = snprintf(proc_path, sizeof(proc_path), "/proc/%d/comm", pid);

    if (path_length < 0 || path_length >= sizeof(proc_path))
    {
        perror("Error forming file path");
        exit(EXIT_FAILURE);
    }

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

    if (fgets(name, 256, file) == NULL)
    {
        if (feof(file))
        {
            fprintf(stderr, "End of file reached\n");
        }
        else
        {
            fprintf(stderr, "Error reading file\n");
        }

        free(name);
        fclose(file);
        exit(EXIT_FAILURE);
    }

    name = (char *)realloc(name, strlen(name) + 1);
    if (!name)
    {
        perror("Error reallocating memory");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    name[strlen(name) - 1] = '\0';

    if (fclose(file) != 0)
    {
        perror("Error closing file");
        free(name);
        exit(EXIT_FAILURE);
    }

    return name;
}

#include <dirent.h>

pid_t pid_by_name(const char *process_name)
{
    DIR *dir = opendir("/proc");
    if (!dir)
    {
        perror("Error opening /proc directory");
        return -1;
    }

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

    closedir(dir);
    return -1;
}
void free_children(pid_t **children)
{
    for (int i = 0; children[i]; i++)
        free(children[i]);
    free(children);
}