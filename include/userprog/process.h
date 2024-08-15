#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_create_initd (const char *file_name);
tid_t process_fork (const char *name, struct intr_frame *if_);
int process_exec (void *f_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (struct thread *next);

/* ********** ********** ********** project 2 : argument parsing ********** ********** ********** */
void argument_stack(char **argv, int argc, struct intr_frame *if_);

/* ********** ********** ********** project 2 : File I/O ********** ********** ********** */
int process_add_file(struct file *f);
struct file *process_get_file(int fd);
int process_close_file(int fd);

#endif /* userprog/process.h */
