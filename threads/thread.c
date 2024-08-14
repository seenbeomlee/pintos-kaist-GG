#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/fixed_point.h" // for project 1: advanced scheduler
#include "intrinsic.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* Random value for basic thread
   Do not modify this value. */
#define THREAD_BASIC 0xd42df210

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;

/* ********** ********** ********** project 1 : alarm clock ********** ********** ********** */
/* list for sleep */
static struct list sleep_list; /* block된 스레드를 저장할 공간 */
static int64_t next_tick_to_awake;

/* ********** ********** ********** project 1 : advanced_scheduler (mlfqs) ********** ********** ********** */
/** List of all processes.  Processes are added to this list
   when they are first scheduled and removed when they exit. */
static struct list all_list;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Thread destruction requests */
static struct list destruction_req;

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

/* ********** ********** ********** project 1 : advanced_scheduler (mlfqs) ********** ********** ********** */
int load_avg;

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static void do_schedule(int status);
static void schedule (void);
static tid_t allocate_tid (void);

/* Returns true if T appears to point to a valid thread. */
#define is_thread(t) ((t) != NULL && (t)->magic == THREAD_MAGIC)

/* Returns the running thread.
 * Read the CPU's stack pointer `rsp', and then round that
 * down to the start of a page.  Since `struct thread' is
 * always at the beginning of a page and the stack pointer is
 * somewhere in the middle, this locates the curent thread. */
#define running_thread() ((struct thread *) (pg_round_down (rrsp ())))


// Global descriptor table for the thread_start.
// Because the gdt will be setup after the thread_init, we should
// setup temporal gdt first.
static uint64_t gdt[3] = { 0, 0x00af9a000000ffff, 0x00cf92000000ffff };

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
void
thread_init (void) {
	ASSERT (intr_get_level () == INTR_OFF);

	/* Reload the temporal gdt for the kernel
	 * This gdt does not include the user context.
	 * The kernel will rebuild the gdt with user context, in gdt_init (). */
	struct desc_ptr gdt_ds = {
		.size = sizeof (gdt) - 1,
		.address = (uint64_t) gdt
	};
	lgdt (&gdt_ds);

	/* Init the globla thread context */
	lock_init (&tid_lock);
	list_init (&ready_list);
	list_init (&destruction_req);
/* ********** ********** ********** project 1 : alarm clock ********** ********** ********** */
	list_init (&sleep_list); /* sleep_list를 사용할 수 있도록 sleep_list를 초기화하는 코드 */
/* ********** ********** ********** project 1 : advanced_scheduler (mlfqs) ********** ********** ********** */
	list_init (&all_list); /* all_list 사용할 수 있도록 all_list 초기화하는 코드 */

	/* Set up a thread structure for the running thread. */
	initial_thread = running_thread ();
	init_thread (initial_thread, "main", PRI_DEFAULT);

// /* ********** ********** ********** project 1 : advanced_scheduler (mlfqs) ********** ********** ********** */
 	if (thread_mlfqs) {
 		list_push_back(&all_list, &(initial_thread->all_elem));
	}
	initial_thread->status = THREAD_RUNNING;
	initial_thread->tid = allocate_tid ();
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void) {
	/* Create the idle thread. */
	struct semaphore idle_started;
	sema_init (&idle_started, 0);
	thread_create ("idle", PRI_MIN, idle, &idle_started);

/* ********** ********** ********** project 1 : advanced_scheduler (mlfqs) ********** ********** ********** */
  load_avg = LOAD_AVG_DEFAULT;
  // 새롭게 추가한 변수를 초기화 하는 과정

	/* Start preemptive thread scheduling. */
	intr_enable ();

	/* Wait for the idle thread to initialize idle_thread. */
	sema_down (&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void
thread_tick (void) {
	struct thread *t = thread_current ();

	/* Update statistics. */
	if (t == idle_thread)
		idle_ticks++;
#ifdef USERPROG
	else if (t->pml4 != NULL)
		user_ticks++;
#endif
	else
		kernel_ticks++;

	/* Enforce preemption. */
	if (++thread_ticks >= TIME_SLICE)
		intr_yield_on_return ();
}

/* Prints thread statistics. */
void
thread_print_stats (void) {
	printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
			idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
tid_t
thread_create (const char *name, int priority,
		thread_func *function, void *aux) {
	struct thread *t;
	tid_t tid;

	ASSERT (function != NULL);

	/* Allocate thread. */
	t = palloc_get_page (PAL_ZERO);
	if (t == NULL)
		return TID_ERROR;

	/* Initialize thread. */
	init_thread (t, name, priority);
	tid = t->tid = allocate_tid ();

	/* Call the kernel_thread if it scheduled.
	 * Note) rdi is 1st argument, and rsi is 2nd argument. */
	t->tf.rip = (uintptr_t) kernel_thread;
	t->tf.R.rdi = (uint64_t) function;
	t->tf.R.rsi = (uint64_t) aux;
	t->tf.ds = SEL_KDSEG;
	t->tf.es = SEL_KDSEG;
	t->tf.ss = SEL_KDSEG;
	t->tf.cs = SEL_KCSEG;
	t->tf.eflags = FLAG_IF;

	/* Add to run queue. */
	thread_unblock (t);
/* ********** ********** ********** project 1 : priority scheduleing(1) ********** ********** ********** */
  thread_test_preemption ();
	return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void
thread_block (void) {
	ASSERT (!intr_context ());
	ASSERT (intr_get_level () == INTR_OFF);
	thread_current ()->status = THREAD_BLOCKED;
	schedule ();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void
thread_unblock (struct thread *t) {
  enum intr_level old_level;

  ASSERT (is_thread (t));

  old_level = intr_disable ();
  ASSERT (t->status == THREAD_BLOCKED);
  // list_push_back (&ready_list, &t->elem); // list push back 함수는 round-robin 방식에서 elem을 list의 맨 뒤에 push 하는 함수이다.
  list_insert_ordered (&ready_list, &t->elem, thread_compare_priority, 0);
  t->status = THREAD_READY;
  intr_set_level (old_level);
}

/* Returns the name of the running thread. */
const char *
thread_name (void) {
	return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void) {
	struct thread *t = running_thread ();

	/* Make sure T is really a thread.
	   If either of these assertions fire, then your thread may
	   have overflowed its stack.  Each thread has less than 4 kB
	   of stack, so a few big automatic arrays or moderate
	   recursion can cause stack overflow. */
	ASSERT (is_thread (t));
	ASSERT (t->status == THREAD_RUNNING);

	return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void) {
	return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void) {
	ASSERT (!intr_context ());

#ifdef USERPROG
	process_exit ();
#endif
/* ********** ********** ********** project 1 : advanced_scheduler (mlfqs) ********** ********** ********** */
	if (thread_mlfqs)
    list_remove(&thread_current()->all_elem);
/* ********** ********** ********** project 1 : advanced_scheduler (mlfqs) ********** ********** ********** */

	/* Just set our status to dying and schedule another process.
	   We will be destroyed during the call to schedule_tail(). */
	intr_disable ();
	do_schedule (THREAD_DYING);
	NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void) {
  struct thread *cur = thread_current ();
  enum intr_level old_level;
  
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  if (cur != idle_thread) {
    	// list_push_back (&ready_list, &cur->elem); 이는 round-robin 방식에 사용되는 단순 list_push_back() 함수이다.
    	list_insert_ordered (&ready_list, &cur->elem, thread_compare_priority, 0);
	}
	do_schedule (THREAD_READY);
	intr_set_level (old_level);
}

/* Sets the current thread's priority to NEW_PRIORITY. */
void
thread_set_priority (int new_priority) {
/* ********** ********** ********** project 1 : advanced_scheduler (mlfqs) ********** ********** ********** */
  	// mlfqs scheduler 에서는 priority를 임의로 변경할 수 없기 때문에, thread_set_priority() 함수 역시 비활성화 시켜야 한다.
  	if (thread_mlfqs) {
    	return;
  	}

/* ********** ********** ********** project 1 : priority inversion(donation) ********** ********** ********** */
	// 만약, 현재 진행중인 running thread의 priority 변경이 일어났을 때,
	// donations list들에 있는 thread들보다 priority가 높아지는 경우가 생길 수 있다.
	// 이 경우, priority는 donations list 중 가장 높은 priority가 아니라, 새로 바뀐 priority가 적용될 수 있게 해야 한다.
	// 이는 thread_set_priority에 refresh_priority() 함수를 추가하는 것으로 간단하게 가능하다.
	// thread_current ()->priority = new_priority; 원래 존재하던 코드 priority -> init_priority로 변경.
	thread_current ()->init_priority = new_priority;
  refresh_priority ();

/* ********** ********** ********** project 1 : priority scheduleing(1) ********** ********** ********** */
	thread_test_preemption ();
}

/* Returns the current thread's priority. */
int
thread_get_priority (void) {
	return thread_current ()->priority;
}

/* ********** ********** ********** project 1 : advanced_scheduler (mlfqs) ********** ********** ********** */
/** Sets the current thread's nice value to NICE. */
void
thread_set_nice (int nice UNUSED) 
{ // 현재 thread의 nice 값을 새 값으로 설정한다.
  enum intr_level old_level = intr_disable (); // 각 값들을 변경할 시에는 interrupt를 비활성화 해야한다.
  thread_current ()->nice = nice;
  mlfqs_calculate_priority (thread_current ());
  thread_test_preemption ();
  intr_set_level (old_level); // 다시 interrupt를 활성화 해준다.
}

/** Returns the current thread's nice value. */
int
thread_get_nice (void) 
{ // 현재 thread의 nice 값을 반환한다.
  enum intr_level old_level = intr_disable ();
  int nice = thread_current ()-> nice;
  intr_set_level (old_level);
  return nice;
}

/** Returns 100 times the system load average. */
int
thread_get_load_avg (void) 
{ // 현재 system의 load_avg * 100 값을 반환한다.
  enum intr_level old_level = intr_disable ();
  // pintos document의 지시대로 100을 곱한 후 정수형으로 만들고 반올림하여 반환한다.
  // 정수형 반환값에서 소수점 둘째 자리까지의 값을 확인할 수 있도록 하는 용도이다.
  int load_avg_value = fp_to_int_round (mult_mixed (load_avg, 100));
  intr_set_level (old_level);
  return load_avg_value;
}

/** Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void) 
{ // 현재 thread의 recent_cpu * 100 값을 반환한다.
  enum intr_level old_level = intr_disable ();
  // pintos document의 지시대로 100을 곱한 후 정수형으로 만들고 반올림하여 반환한다.
  // 정수형 반환값에서 소수점 둘째 자리까지의 값을 확인할 수 있도록 하는 용도이다.
  int recent_cpu= fp_to_int_round (mult_mixed (thread_current ()->recent_cpu, 100));
  intr_set_level (old_level);
  return recent_cpu;
}
/* ********** ********** ********** project 1 : advanced_scheduler (mlfqs) ********** ********** ********** */


/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED) {
	struct semaphore *idle_started = idle_started_;

	idle_thread = thread_current ();
	sema_up (idle_started);

	for (;;) {
		/* Let someone else run. */
		intr_disable ();
		thread_block ();

		/* Re-enable interrupts and wait for the next one.

		   The `sti' instruction disables interrupts until the
		   completion of the next instruction, so these two
		   instructions are executed atomically.  This atomicity is
		   important; otherwise, an interrupt could be handled
		   between re-enabling interrupts and waiting for the next
		   one to occur, wasting as much as one clock tick worth of
		   time.

		   See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
		   7.11.1 "HLT Instruction". */
		asm volatile ("sti; hlt" : : : "memory");
	}
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux) {
	ASSERT (function != NULL);

	intr_enable ();       /* The scheduler runs with interrupts off. */
	function (aux);       /* Execute the thread function. */
	thread_exit ();       /* If function() returns, kill the thread. */
}


/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread (struct thread *t, const char *name, int priority) {
	ASSERT (t != NULL);
	ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
	ASSERT (name != NULL);

	memset (t, 0, sizeof *t);
	t->status = THREAD_BLOCKED;
	strlcpy (t->name, name, sizeof t->name);
	t->tf.rsp = (uint64_t) t + PGSIZE - sizeof (void *);

	if (thread_mlfqs) {
		mlfqs_calculate_priority(t);
		list_push_back (&all_list, &t->all_elem);
	} else {
		t->priority = priority;
	}

/* ********** ********** ********** project 1 : priority inversion(donation) ********** ********** ********** */
	// 새롭게 추가한 요소를 초기화 하는 과정
  t->init_priority = priority;
	t->wait_on_lock = NULL;
  list_init (&t->donations);

	t->magic = THREAD_MAGIC;

/* ********** ********** ********** project 1 : advanced_scheduler (mlfqs) ********** ********** ********** */
	// 새롭게 추가한 요소를 초기화 하는 과정
	t->nice = NICE_DEFAULT;
  t->recent_cpu = RECENT_CPU_DEFAULT;
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *
next_thread_to_run (void) {
	if (list_empty (&ready_list))
		return idle_thread;
	else
		return list_entry (list_pop_front (&ready_list), struct thread, elem);
}

/* Use iretq to launch the thread */
void
do_iret (struct intr_frame *tf) {
	__asm __volatile(
			"movq %0, %%rsp\n"
			"movq 0(%%rsp),%%r15\n"
			"movq 8(%%rsp),%%r14\n"
			"movq 16(%%rsp),%%r13\n"
			"movq 24(%%rsp),%%r12\n"
			"movq 32(%%rsp),%%r11\n"
			"movq 40(%%rsp),%%r10\n"
			"movq 48(%%rsp),%%r9\n"
			"movq 56(%%rsp),%%r8\n"
			"movq 64(%%rsp),%%rsi\n"
			"movq 72(%%rsp),%%rdi\n"
			"movq 80(%%rsp),%%rbp\n"
			"movq 88(%%rsp),%%rdx\n"
			"movq 96(%%rsp),%%rcx\n"
			"movq 104(%%rsp),%%rbx\n"
			"movq 112(%%rsp),%%rax\n"
			"addq $120,%%rsp\n"
			"movw 8(%%rsp),%%ds\n"
			"movw (%%rsp),%%es\n"
			"addq $32, %%rsp\n"
			"iretq"
			: : "g" ((uint64_t) tf) : "memory");
}

/* Switching the thread by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function. */
static void
thread_launch (struct thread *th) {
	uint64_t tf_cur = (uint64_t) &running_thread ()->tf;
	uint64_t tf = (uint64_t) &th->tf;
	ASSERT (intr_get_level () == INTR_OFF);

	/* The main switching logic.
	 * We first restore the whole execution context into the intr_frame
	 * and then switching to the next thread by calling do_iret.
	 * Note that, we SHOULD NOT use any stack from here
	 * until switching is done. */
	__asm __volatile (
			/* Store registers that will be used. */
			"push %%rax\n"
			"push %%rbx\n"
			"push %%rcx\n"
			/* Fetch input once */
			"movq %0, %%rax\n"
			"movq %1, %%rcx\n"
			"movq %%r15, 0(%%rax)\n"
			"movq %%r14, 8(%%rax)\n"
			"movq %%r13, 16(%%rax)\n"
			"movq %%r12, 24(%%rax)\n"
			"movq %%r11, 32(%%rax)\n"
			"movq %%r10, 40(%%rax)\n"
			"movq %%r9, 48(%%rax)\n"
			"movq %%r8, 56(%%rax)\n"
			"movq %%rsi, 64(%%rax)\n"
			"movq %%rdi, 72(%%rax)\n"
			"movq %%rbp, 80(%%rax)\n"
			"movq %%rdx, 88(%%rax)\n"
			"pop %%rbx\n"              // Saved rcx
			"movq %%rbx, 96(%%rax)\n"
			"pop %%rbx\n"              // Saved rbx
			"movq %%rbx, 104(%%rax)\n"
			"pop %%rbx\n"              // Saved rax
			"movq %%rbx, 112(%%rax)\n"
			"addq $120, %%rax\n"
			"movw %%es, (%%rax)\n"
			"movw %%ds, 8(%%rax)\n"
			"addq $32, %%rax\n"
			"call __next\n"         // read the current rip.
			"__next:\n"
			"pop %%rbx\n"
			"addq $(out_iret -  __next), %%rbx\n"
			"movq %%rbx, 0(%%rax)\n" // rip
			"movw %%cs, 8(%%rax)\n"  // cs
			"pushfq\n"
			"popq %%rbx\n"
			"mov %%rbx, 16(%%rax)\n" // eflags
			"mov %%rsp, 24(%%rax)\n" // rsp
			"movw %%ss, 32(%%rax)\n"
			"mov %%rcx, %%rdi\n"
			"call do_iret\n"
			"out_iret:\n"
			: : "g"(tf_cur), "g" (tf) : "memory"
			);
}

/* Schedules a new process. At entry, interrupts must be off.
 * This function modify current thread's status to status and then
 * finds another thread to run and switches to it.
 * It's not safe to call printf() in the schedule(). */
static void
do_schedule(int status) {
	ASSERT (intr_get_level () == INTR_OFF);
	ASSERT (thread_current()->status == THREAD_RUNNING);
	while (!list_empty (&destruction_req)) {
		struct thread *victim =
			list_entry (list_pop_front (&destruction_req), struct thread, elem);
		palloc_free_page(victim);
	}
	thread_current ()->status = status;
	schedule ();
}

static void
schedule (void) {
	struct thread *curr = running_thread ();
	struct thread *next = next_thread_to_run ();

	ASSERT (intr_get_level () == INTR_OFF);
	ASSERT (curr->status != THREAD_RUNNING);
	ASSERT (is_thread (next));
	/* Mark us as running. */
	next->status = THREAD_RUNNING;

	/* Start new time slice. */
	thread_ticks = 0;

#ifdef USERPROG
	/* Activate the new address space. */
	process_activate (next);
#endif

	if (curr != next) {
		/* If the thread we switched from is dying, destroy its struct
		   thread. This must happen late so that thread_exit() doesn't
		   pull out the rug under itself.
		   We just queuing the page free reqeust here because the page is
		   currently used by the stack.
		   The real destruction logic will be called at the beginning of the
		   schedule(). */
		if (curr && curr->status == THREAD_DYING && curr != initial_thread) {
			ASSERT (curr != next);
			list_push_back (&destruction_req, &curr->elem);
		}

		/* Before switching the thread, we first save the information
		 * of current running. */
		thread_launch (next);
	}
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void) {
	static tid_t next_tid = 1;
	tid_t tid;

	lock_acquire (&tid_lock);
	tid = next_tid++;
	lock_release (&tid_lock);

	return tid;
}

/* ********** ********** ********** new functions below ********** ********** ********** */
/* ********** ********** ********** project 1 : alarm clock ********** ********** ********** */
/* thread를 재우는 작업 */
/* 일어날 시간을 저장한 다음에 재워야 할 thread를 sleep_list에 추가한다. */
/* thread의 상태를 block state로 만든다. */
void
thread_sleep (int64_t wakeup_ticks)
{
  struct thread *cur;
  enum intr_level old_level;

	old_level = intr_disable (); // interrupt off & get previous interrupt state(INTR_ON maybe)
	cur = thread_current ();

  ASSERT (cur != idle_thread); // CPU가 항상 실행 상태를 유지하게 하기 위해 idel thread는 sleep되지 않아야 한다.

  cur->wakeup_time = wakeup_ticks; // 현재 running 중인 thread A가 일어날 시간을 저장
  list_push_back (&sleep_list, &cur->elem); // sleep_list 에 추가한다.
  thread_block (); //thread A를 block 상태로 변경한다.
   
  intr_set_level (old_level); // interrupt on
}

/* block된 thread들이 일어날 시간이 되었을 때 깨우는 함수 */
void
thread_awake (int64_t ticks)
{
  struct list_elem *e = list_begin (&sleep_list);

  while (e != list_end (&sleep_list)){
    struct thread *t = list_entry (e, struct thread, elem);
    if (t->wakeup_time <= ticks){	// 스레드가 일어날 시간이 되었는지 확인
      e = list_remove (e);	// sleep list 에서 제거
      thread_unblock (t);	// 스레드 unblock
    }
    else 
      e = list_next (e);
  }
}

/* ********** ********** ********** project 1 : alarm clock ********** ********** ********** */

/* ********** ********** ********** project 1 : priority schedulder(1) ********** ********** ********** */
// list_insert_ordered(struct list *list, struct list_elem *elem, list_less_func *less, void *aux)
// list_insert(struct list_elem *before, struct list_elem *elem) 해당 함수는 elem을 e의 '앞에' 삽입한다.
// 우리는 ready_list에서 thread를 pop할 때 가장 앞에서 꺼내기로 하였다.
// 따라서, 가장 앞에 priority가 가장 높은 thread가 와야 한다.
// 즉, ready_list는 내림차순으로 정렬되어야 한다.
// if(less (elem, e, aux))가 elem > e인 순간에 break; 를 해주어야 한다.
// 즉, 우리가 만들어야 하는 order 비교함수 less(elem, e, aux)는 elem > e일 때 true를 반환하는 함수이다.
bool 
thread_compare_priority (const struct list_elem *l, const struct list_elem *s, void *aux UNUSED)
{
    return list_entry (l, struct thread, elem)->priority
         > list_entry (s, struct thread, elem)->priority;
}

// 현재 실행중인 running thread의 priority가 바뀌는 순간이 있다.
// 이때 바뀐 priority가 ready_list의 가장 높은 priority보다 낮다면 CPU 점유를 넘겨주어야 한다.
// 현재 실행중인 thread의 priority가 바뀌는 순간은 두 가지 경우이다.
// (1) thread_create() -- thread가 새로 생성되어서 ready_list에 추가된다.
// (2) thread_set_priority() -- 현재 실행중인 thread의 우선순위가 재조정된다.
// 두 경우의 마지막에 running thread와 ready_list의 가장 앞의 thread의 priority를 비교하는 코드를 넣어주어야 한다.
// running thread와 ready_list의 가장 앞 thread의 priority를 비교하고,
// 만약 ready_list의 thread가 더 높은 priority를 가진다면 thread_yield()를 실행하여 CPU의 점유권을 넘겨준다.
// 이 함수를 (1), (2)에 추가한다.
void 
thread_test_preemption (void)
{
    if (!list_empty (&ready_list) && 
		!intr_context() &&
		// priority1 < priority2 라면, priority2의 우선순위가 더 높음을 의미한다. 또한 이것이 list의 맨 앞에 추가된다.
    thread_current ()->priority < 
    list_entry (list_front (&ready_list), struct thread, elem)->priority)
        thread_yield ();
}

/* ********** ********** ********** project 1 : priority inversion(donation) ********** ********** ********** */
// 위의 thread_compare_priority과 동일하다. 다만, elem이 아니라, donation elem을 대상으로 진행한다는 점이 다르다.
bool
thread_compare_donate_priority (const struct list_elem *l, const struct list_elem *s, void *aux UNUSED)
{
	return list_entry (l, struct thread, donation_elem)->priority
		 > list_entry (s, struct thread, donation_elem)->priority;
}

// 자신의 priority를 필요한 lock을 점유하고 있는 thread에게 빌려주는 함수이다.
// 주의할 점은, nested donation을 위해 하위에 연결된 모든 thread에 donation이 일어나야 한다는 것이다.
void
donate_priority (void) {
  int depth;
  struct thread *cur = thread_current ();

  for (depth = 0; depth < 8; depth++){ // max_depth == 8
    if (!cur->wait_on_lock) break; // thread의 wait_on_lock이 NULL이라면 더이상 donation을 진행할 필요가 없으므로 멈춘다.
    struct thread *holder = cur->wait_on_lock->holder;
    holder->priority = cur->priority;
    cur = holder;
  }
}

void
remove_with_lock (struct lock *lock)
{
	struct list_elem *e;
  struct thread *cur = thread_current ();

  for (e = list_begin (&cur->donations); e != list_end (&cur->donations); e = list_next (e)){
    struct thread *t = list_entry (e, struct thread, donation_elem);
    if (t->wait_on_lock == lock) // wait_on_lock이 이번에 release하는 lock이라면, 해당 thread를 donation list에서 지운다.
      list_remove (&t->donation_elem); // 모든 donations list와 관련된 작업에서는 elem이 아니라, donation_elem을 사용한다.
  }
}

void
refresh_priority (void)
{
	struct thread *cur = thread_current ();

  cur->priority = cur->init_priority; // donations list가 비어있을 때는 cur thread에 init_priority를 삽입해준다.
  
	  // 허나, donations list에 thread가 남아있다면 thread 중에서 가장 높은 priority를 가져와서 삽입해야한다.
  if (!list_empty (&cur->donations)) {
    list_sort (&cur->donations, thread_compare_donate_priority, 0); // donations list를 내림차순으로 정렬한다.

    // 그 후, 맨 앞의 thread(우선순위가 가장 큰)를 가져온다.
    struct thread *front = list_entry (list_front (&cur->donations), struct thread, donation_elem);
		// init_priority와 비교하여 더 큰 priority를 삽입한다.
    if (front->priority > cur->priority)
      cur->priority = front->priority;
  }
}

/* ********** ********** ********** project 1 : advanced_scheduler (mlfqs) ********** ********** ********** */
// mlfqs_caculate_priority 함수는 priority를 계산한다.
// idel_thread의 priority는 고정이므로 제외하고, fixed_point.h에서 만든 fp 연산 함수를 사용하여 priority를 구한다.
// 계산 결과의 소수점 부분은 버림하고, 정수의 priority로 설정한다.
void
mlfqs_calculate_priority (struct thread *t)
{
  if (t == idle_thread) 
    return ;
  t->priority = fp_to_int (add_mixed (div_mixed (t->recent_cpu, -4), PRI_MAX - t->nice * 2));
}

// mlfqs_calculate_recent_cpu 함수는 특정 thread의 priority를 계산하는 함수이다.
void
mlfqs_calculate_recent_cpu (struct thread *t)
{
  if (t == idle_thread)
    return ;
  t->recent_cpu = add_mixed (mult_fp (div_fp (mult_mixed (load_avg, 2), add_mixed (mult_mixed (load_avg, 2), 1)), t->recent_cpu), t->nice);
}

// mlfqs_calculate_load_avg 함수는 load_avg 값을 계산하는 함수이다.
// load_avg 값은 thread 고유의 값이 아니라, system wide 값이기 때문에, idle_thread가 실행되는 경우에도 계산하여 준다.
void 
mlfqs_calculate_load_avg (void) 
{
  int ready_threads;
  
	// ready_threads는 현재 시점에서 실행 가능한 thread의 수를 나타내므로,
	// ready_list에 들어있는 thread의 숫자에 현재 running thread 1개를 더한다.
	// idle thread는 실행 가능한 thread에 포함시키지 않는다.
  if (thread_current () == idle_thread)
    ready_threads = list_size (&ready_list);
  else
    ready_threads = list_size (&ready_list) + 1;

  load_avg = add_fp (mult_fp (div_fp (int_to_fp (59), int_to_fp (60)), load_avg), 
                     mult_mixed (div_fp (int_to_fp (1), int_to_fp (60)), ready_threads));
}

// 각 값들이 변하는 시점에 수행할 함수를 만든다. 값들이 변화하는 시점은 3가지가 있다.
// (1) 1 tick마다 running thread의 recent_cpu 값 + 1
// (2) 4 tick마다 모든 thread의 priority 값 재계산
// (3) 1 초마다 모든 thread의 recent_cpu값과 load_avg 값 재계산 
// 아래의 mlfqs_increment_recent_cpu, mlfqs_recalculate_recent_cpu, mlfqs_recalculate_priority 함수를
// 해당하는 시간 주기마다 실행되도록 timer_interrupt 함수를 바꾸어주면 된다. -> <devices/timer.c> -> timer_interrupt ()

// 현재 thread의 recent_cpu 값을 1 증가시키는 함수이다.
void
mlfqs_increment_recent_cpu (void)
{
  if (thread_current () != idle_thread)
    thread_current ()->recent_cpu = add_mixed (thread_current ()->recent_cpu, 1);
}

// 모든 thread의 recent_cpu를 재계산하는 함수이다.
void
mlfqs_recalculate_recent_cpu (void)
{
  struct list_elem *e;

  for (e = list_begin (&all_list); e != list_end (&all_list); e = list_next (e)) {
    struct thread *t = list_entry (e, struct thread, all_elem);
    mlfqs_calculate_recent_cpu (t);
  }
}

// 모든 thread의 priority를 재계산하는 함수이다.
void
mlfqs_recalculate_priority (void)
{
  struct list_elem *e;

  for (e = list_begin (&all_list); e != list_end (&all_list); e = list_next (e)) {
    struct thread *t = list_entry (e, struct thread, all_elem);
    mlfqs_calculate_priority (t);
  }
}