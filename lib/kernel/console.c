#include <console.h>
#include <stdarg.h>
#include <stdio.h>
#include "devices/serial.h"
#include "devices/vga.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/synch.h"

static void vprintf_helper (char, void *);
static void putchar_have_lock (uint8_t c);

/* The console lock.
   Both the vga and serial layers do their own locking, so it's
   safe to call them at any time.
   But this lock is useful to prevent simultaneous printf() calls
   from mixing their output, which looks confusing. */
static struct lock console_lock;

/* True in ordinary circumstances: we want to use the console
   lock to avoid mixing output between threads, as explained
   above.

   False in early boot before the point that locks are functional
   or the console lock has been initialized, or after a kernel
   panics.  In the former case, taking the lock would cause an
   assertion failure, which in turn would cause a panic, turning
   it into the latter case.  In the latter case, if it is a buggy
   lock_acquire() implementation that caused the panic, we'll
   likely just recurse. */
static bool use_console_lock;

/* It's possible, if you add enough debug output to Pintos, to
   try to recursively grab console_lock from a single thread.  As
   a real example, I added a printf() call to palloc_free().
   Here's a real backtrace that resulted:

   lock_console()
   vprintf()
   printf()             - palloc() tries to grab the lock again
   palloc_free()        
   schedule_tail()      - another thread dying as we switch threads
   schedule()
   thread_yield()
   intr_handler()       - timer interrupt
   intr_set_level()
   serial_putc()
   putchar_have_lock()
   putbuf()
   sys_write()          - one process writing to the console
   syscall_handler()
   intr_handler()

   This kind of thing is very difficult to debug, so we avoid the
   problem by simulating a recursive lock with a depth
   counter. */
static int console_lock_depth;

/* Number of characters written to console. */
static int64_t write_cnt;

/* Enable console locking. */
void
console_init (void) {
	lock_init (&console_lock);
	use_console_lock = true;
}

/* Notifies the console that a kernel panic is underway,
   which warns it to avoid trying to take the console lock from
   now on. */
void
console_panic (void) {
	use_console_lock = false;
}

/* Prints console statistics. */
void
console_print_stats (void) {
	printf ("Console: %lld characters output\n", write_cnt);
}

/* Acquires the console lock. */
	static void
acquire_console (void) {
	if (!intr_context () && use_console_lock) {
		if (lock_held_by_current_thread (&console_lock)) 
			console_lock_depth++; 
		else
			lock_acquire (&console_lock); 
	}
}

/* Releases the console lock. */
static void
release_console (void) {
	if (!intr_context () && use_console_lock) {
		if (console_lock_depth > 0)
			console_lock_depth--;
		else
			lock_release (&console_lock); 
	}
}

/* Returns true if the current thread has the console lock,
   false otherwise. */
static bool
console_locked_by_current_thread (void) {
	return (intr_context ()
			|| !use_console_lock
			|| lock_held_by_current_thread (&console_lock));
}

/* The standard vprintf() function,
   which is like printf() but uses a va_list.
   Writes its output to both vga display and serial port. */
int
vprintf (const char *format, va_list args) {
	int char_cnt = 0;

	acquire_console ();
	__vprintf (format, args, vprintf_helper, &char_cnt);
	release_console ();

	return char_cnt;
}

/* Writes string S to the console, followed by a new-line
   character. */
int
puts (const char *s) {
	acquire_console ();
	while (*s != '\0')
		putchar_have_lock (*s++);
	putchar_have_lock ('\n');
	release_console ();

	return 0;
}

/* Writes the N characters in BUFFER to the console. */
/** write(int fd, void *buffer, unsigned int size)
 * read와 마찬가지로, write도 락을 사용하여 구현해야 한다.
 * 값을 쓸 파일의 디스크립터 fd, 쓸 값이 들어있는 buffer, 쓸 값의 크기인 size를 파라미터로 받는다.
 * 다른 파일에 값을 쓰는 경우에는 file_write 함수를 통해 구현할 수 있다.
 * 파일에 값을 쓰지 못한 경우에는 -1을 return 하며, 값을 쓴 경우에는 쓴 값의 크기를 return 한다.
 * 
 * write도 read와 비슷하게 standard output에 값을 쓸 수 있다. 
 * 파일 디스크립터의 값은 1이며, 콘솔에 값을 쓰는 경우에 해당한다.
 * 이때는 putbuf 함수를 통해 구현한다.
 */
void
putbuf (const char *buffer, size_t n) {
	acquire_console ();
	while (n-- > 0)
		putchar_have_lock (*buffer++);
	release_console ();
}

/* Writes C to the vga display and serial port. */
int
putchar (int c) {
	acquire_console ();
	putchar_have_lock (c);
	release_console ();

	return c;
}

/* Helper function for vprintf(). */
static void
vprintf_helper (char c, void *char_cnt_) {
	int *char_cnt = char_cnt_;
	(*char_cnt)++;
	putchar_have_lock (c);
}

/* Writes C to the vga display and serial port.
   The caller has already acquired the console lock if
   appropriate. */
static void
putchar_have_lock (uint8_t c) {
	ASSERT (console_locked_by_current_thread ());
	write_cnt++;
	serial_putc (c);
	vga_putc (c);
}
