#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

/* ********** ********** ********** project 2 : Extend File Descriptor ********** ********** ********** */
#define STDIN 1
#define STDOUT 2
#define STDERR 3

/**
 * #define STDIN 0
 * #define STDOUT 1
 * #define STDERR 2
 * 여야 맞는 것 같은데.. 일단 수정하지 말고 넘어가자.
 */
tid_t process_create_initd (const char *file_name);
tid_t process_fork (const char *name, struct intr_frame *if_);
int process_exec (void *f_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (struct thread *next);

/* ********** ********** ********** project 2 : argument parsing ********** ********** ********** */
void argument_stack(char **argv, int argc, struct intr_frame *if_);

/* ********** ********** ********** project 2 : File I/O ********** ********** ********** */
/**
 * 현재 thread의 fdt에 현재 file을 추가한다.
 * thread에 저장되어 있는 next_fd부터 탐색을 시작하여 LIMIT 전까지 탐색해서 빈 자리에 할당한다.
 * 할당에 성공했으면 fd를, 실패했으면 -1을 return 한다.
 */
int process_add_file(struct file *f);
struct file *process_get_file(int fd);
int process_close_file(int fd);

/* ********** ********** ********** project 2 : Hierarchical Process Structure ********** ********** ********** */
struct thread *get_child_process(int pid);

/* ********** ********** ********** project 2 : Extend File Descriptor ********** ********** ********** */
process_insert_file(int fd, struct file *f);

#endif /* userprog/process.h */
