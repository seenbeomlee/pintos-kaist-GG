#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/interrupt.h"
#include "threads/synch.h"
#ifdef VM
#include "vm/vm.h"
#endif

/* ********** ********** ********** project 2 : Hierarchical Process Structure ********** ********** ********** */
#define USERPROG

/* States in a thread's life cycle. */
enum thread_status {
	THREAD_RUNNING,     /* Running thread. */
	THREAD_READY,       /* Not running but ready to run. */
	THREAD_BLOCKED,     /* Waiting for an event to trigger. */
	THREAD_DYING        /* About to be destroyed. */
};

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */

/* ********** ********** ********** project 1 : advanced_scheduler (mlfqs) ********** ********** ********** */          
#define NICE_DEFAULT 0
#define RECENT_CPU_DEFAULT 0
#define LOAD_AVG_DEFAULT 0

/* ********** ********** ********** project 2 : File I/O ********** ********** ********** */
#define FDT_PAGES 3 // test for 'multi-oom'
/**
 * 엔트리는 1 << 9 == 2의 9승으로 인해 512개가 된다.
 * 페이지 크기는 4kb이고(2의 12승)), 파일 포인터가 8byte(2의 3승)이기 때문이다.
 * 따라서, 2의 12승 / 2의 3승 == 2의 9승 만큼의 페이지 크기가 된다.
 */
#define FDCOUNT_LIMIT FDT_PAGES * (1 << 9) 

/* A kernel thread or user process.
 *
 * Each thread structure is stored in its own 4 kB page.  The
 * thread structure itself sits at the very bottom of the page
 * (at offset 0). 이를 TCB라고 부른다.
 * The rest of the page is reserved for the
 * thread's kernel stack, which grows downward from the top of
 * the page (at offset 4 kB).  Here's an illustration:
 *
 *      4 kB +---------------------------------+
 *           |          kernel stack           |
 *           |                |                |
 *           |                |                |
 *           |                V                |
 *           |         grows downward          |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           +---------------------------------+
 *           |              magic              |
 *           |            intr_frame           |
 *           |                :                |
 *           |                :                |
 *           |               name              |
 *           |              status             |
 *      0 kB +---------------------------------+
 *
 * The upshot of this is twofold:
 *
 *    1. First, `struct thread' must not be allowed to grow too
 *       big.  If it does, then there will not be enough room for
 *       the kernel stack.  Our base `struct thread' is only a
 *       few bytes in size.  It probably should stay well under 1
 *       kB.
 *
 *    2. Second, kernel stacks must not be allowed to grow too
 *       large.  If a stack overflows, it will corrupt the thread
 *       state.  Thus, kernel functions should not allocate large
 *       structures or arrays as non-static local variables.  Use
 *       dynamic allocation with malloc() or palloc_get_page()
 *       instead.
 *
 * The first symptom of either of these problems will probably be
 * an assertion failure in thread_current(), which checks that
 * the `magic' member of the running thread's `struct thread' is
 * set to THREAD_MAGIC.  Stack overflow will normally change this
 * value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
 * the run queue (thread.c), or it can be an element in a
 * semaphore wait list (synch.c).  It can be used these two ways
 * only because they are mutually exclusive: only a thread in the
 * ready state is on the run queue, whereas only a thread in the
 * blocked state is on a semaphore wait list. */

struct thread /* TCB 영역의 구성 */
  {
    /* Owned by thread.c. */
    tid_t tid;                          /**< Thread identifier. */
    enum thread_status status;          /**< Thread state. */
    char name[16];                      /**< Name (for debugging purposes). */
    uint8_t *stack;                     /**< Saved stack pointer. */
   /* *stack 포인터가 중요하다. running중인 thread A에서 scheduling이 일어나면, 현재 CPU가 실행중이던 상태,
   즉 현재 CPU의 Register들에 저장된 값들을? 값들의 포인터를 아닌가? 이 *stack에 저장한다. 후에 다시 thread A가
   실행되는 순간에 이 stack이 다시 CPU의 Register로 로드되어 실행을 이어갈 수 있게 된다. 
   */
    int priority;                       /**< Priority. */
    struct list_elem all_elem;           /**< List element for all threads list. */

    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /**< List element. */

#ifdef USERPROG
	/* Owned by userprog/process.c. */
	uint64_t *pml4;                     /* Page map level 4 */
/* ********** ********** ********** project 2 : argument parsing ********** ********** ********** */
    struct file *runn_file;  // 실행중인 파일

/* ********** ********** ********** project 2 : system call ********** ********** ********** */
    // exit(), wait() 구현 때 사용될 exit_status를 추가하고 초기화 한다.
    int exit_status;

/* ********** ********** ********** project 2 : File I/O ********** ********** ********** */
    /** File Descriptor 
     * 유닉스 시스템에서는 많은 것들을 파일로 관리한다.
     * 정규 파일들뿐만 아니라 디렉터리, 소켓, 기타 입출력 장치들 모두 파일의 형태로 취급하는데,
     * 이 파일들을 구분하기 위해 파일 디스크립터라는 개념을 사용한다.
     * 우선 각각의 프로세스들이 실행될 때, file descriptor table도 함께 생성된다.
     * file desciptor table은 배열 형태로 관리가 되며, 각 element는 Inode table의 특정 위치 또는 연결된 소켓을 가리킨다.
     * 
     * 따라서, 우리가 open을 통해 파일을 열거나, socekt 함수를 통해 소켓을 생성하면, int 형태의 파일 디스크립터가 실행된다.
     * 이때, 이 파일 디스크립터는 file descriptor table 상에서 해당 파일 또는 소켓이 할당받은 위치의 인덱스이다.
     * 따라서, 파일 디스크립터는 항상 양수 값을 가지며, 파일을 정상적으로 열지 못하거나 소켓을 연결하지 못한 경우에 -1을 예외적으로 리턴한다.
     * 
     * 프로세스에서 소켓을 열거나, 파일을 열 때 할당되는 파일 디스크립터는 항상 3부터 시작한다. 0, 1, 2는 시스템 상 먼저 예약되어 있기 때문이다.
     * 0 == standard input
     * 1 == standard output
     * 2 == standard error
     * 
     * file table은 각 process마다 따로 가지고 있으므로, 서로 다른 프로세스에서 같은 파일을 참조한다고 하여도
     * 각각 다른 파일 디스크립터를 가질 수 있고, 다른 프로세스에서 같은 파일 디스크립터를 가지고 있어도 다른 파일을 참조할 수 있다.
     * 다만, file descriptor table은 프로세스 당 하나씩 존재하지만, 
     * 이것이 가리키는 open file table은 (파일의 위치, 파일이 참조된 횟수를 나타내는 ref_cnt 변수 포함한 파일 구조체)
     * 모든 프로세스가 공유한다.
    */
    int fd_idx; // 파일 디스크립터 인덱스, 새로 할당한 fd의 인덱스를 나타낸다. == next_fd와 같다.
    struct file **fdt; // 파일 디스크립터 테이블
    /** 파일 테이블은 struct file *fdt[128]과 같이 포인터 배열 형태로 선언하면 struct thread의 크기가 너무 커진다.
     * 이를 방지하기 위해 thread에는 파일 테이블의 시작 주소만 가지고 있게끔 **fdt 형태로 선언해주고,
     * thread가 생성될 때 파일 테이블을 동적으로 할당받게끔 한다. by (palloc)
    */

/* ********** ********** ********** project 2 : Hierarchical Process Structure ********** ********** ********** */
    struct intr_frame parent_if; // parent process의 if
    struct list child_list; // parent가 가진 child_list
    struct list_elem child_elem;

// for process_fork() & __do_fork()
    struct semaphore fork_sema; // fork가 완료될 때 signal
// for process_wait()
    struct semaphore exit_sema; // child process 종료 signal
    struct semaphore wait_sema; // exit_sema를 기다릴 때 사용한다.
#endif
#ifdef VM
	/* Table for whole virtual memory owned by thread. */
	struct supplemental_page_table spt;
#endif

	/* Owned by thread.c. */
	struct intr_frame tf;               /* Information for switching */
	unsigned magic;                     /* Detects stack overflow. */

/* ********** ********** ********** new functions below ********** ********** ********** */
/* ********** ********** ********** project 1 : alarm clock ********** ********** ********** */
    int64_t wakeup_time; /* block된 스레드가 꺠어나야 할 tick을 저장한 변수 추가 */

/* ********** ********** ********** project 1 : priority inversion(donation) ********** ********** ********** */
	// thread가 priority를 양도받았다가 다시 반납할 때 원래의 priority를 복원할 수 있도록 고유의 값 저장하는 변수
    int init_priority;

    // thread가 현재 얻기 위해 기다리고 있는 lock으로, thread는 이 lock이 release 되기를 기다린다.
    // 즉, thread B가 얻기 위해 기다리는 lock을 현재 보유한 thread A에게 자신의 priority를 주는 것이므로, 이를 받은 thread A의 donations에 thread B가 기록된다.
    struct lock *wait_on_lock; 

	// 자신에게 priority를 나누어진 thread들의 list. 왜 thread들이냐면, Multiple donation 때문이다.
    struct list donations;
	// 이 list를 관리하기 위한 element로, thread 구조체의 elem과 구분하여 사용한다.
    struct list_elem donation_elem; 

/* ********** ********** ********** project 1 : advanced_scheduler (mlfqs) ********** ********** ********** */
 	int nice;
   	int recent_cpu;
};

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs; // for lab4 Advanced Scheduler

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

void do_iret (struct intr_frame *tf);

#endif /* threads/thread.h */

/* ********** ********** ********** new functions below ********** ********** ********** */
/* ********** ********** ********** project 1 : alarm clock ********** ********** ********** */
void thread_sleep(int64_t ticks);
void thread_awake(int64_t ticks);

/* ********** ********** ********** project 1 : priority scheduleing(1) ********** ********** ********** */
bool thread_compare_priority (const struct list_elem *l, const struct list_elem *s, void *aux UNUSED);
void thread_test_preemption (void);

/* ********** ********** ********** project 1 : priority inversion(donation) ********** ********** ********** */
bool thread_compare_donate_priority (const struct list_elem *l, const struct list_elem *s, void *aux UNUSED);
void donate_priority (void);
void remove_with_lock (struct lock *lock);
void refresh_priority (void);

/* ********** ********** ********** project 1 : advanced_scheduler (mlfqs) ********** ********** ********** */
void mlfqs_calculate_priority (struct thread *t);
void mlfqs_calculate_recent_cpu (struct thread *t);
void mlfqs_calculate_load_avg (void);
void mlfqs_increment_recent_cpu (void);
void mlfqs_recalculate_recent_cpu (void);
void mlfqs_recalculate_priority (void);
