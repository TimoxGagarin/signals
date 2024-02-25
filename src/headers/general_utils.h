#pragma once

char *name_by_pid(pid_t pid);
pid_t pid_by_name(const char *process_name);
void free_children(pid_t **children);