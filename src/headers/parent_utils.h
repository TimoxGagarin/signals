#pragma once

void initSignalHandlers();
void choose_options(const char *child_path);
void createChildProcess(const char *child_path);
void list_child_processes();
void close_child_processes();
void close_last_child_processes();