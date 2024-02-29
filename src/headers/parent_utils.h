#pragma once
#include <stdbool.h>

void initSignalHandlers();
void choose_options(const char *child_path);
void createChildProcess(const char *child_path);
void list_child_processes();
void close_child_processes();
void close_last_child_processes();
void allow_children_print(bool can);
void allow_child_print(int pnum, bool can);