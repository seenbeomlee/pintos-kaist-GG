#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* The main system call interface */
/** System Call이란?
 *  시스템 콜은 사용자가 커널 영역에 접근하고 싶을 때, 원하는 목적을 대신해서 작업하는 프로그래밍 인터페이스이다. 
 *  그렇기 때문에 시스템 콜은 커널 모드에서 실행되고, 작업 후 사용자 모드로 복귀한다. 
 *  PintOS에서는 이를 시스템 콜 핸들러를 통해 시스템 콜을 호출한다. 
 */
void
syscall_handler (struct intr_frame *f UNUSED) {
	// TODO: Your implementation goes here.

	/** syscall_n 에는 f->R.rax에 저장된 시스템 콜 넘버를 받아온다.
	 * 이후, switch문을 통해서 호출해야하는 시스템 콜 넘버에 해당하는 코드가 실행된다.
	 * int syscall_n = f->R.rax; 
	 */

	/** 함수 리턴 값을 위한 x86-64의 관례는 그 값을 RAX 레지스터에 넣는 것이다.
	 * 값을 리턴하는 시스템 콜도 struct intr_frame의 rax 멤버를 수정하는 식으로 이 관례를 따를 수 있다.
	 */

  int syscall_number = f->R.rax;
	switch (syscall_number)
	{
		// Argument 순서는 다음과 같다.
		// %rdi %rsi %rdx %r10 %r8 %r9
		case SYS_HALT:
			halt();
			break;
		case SYS_EXIT:
		/** 현재 동작중인 유저 프로그램을 종료한다.
		 * 커널에 상태를 return 하면서 종료한다.
		 * 만약, 부모 프로세스가 현재 유저 프로그램의 종료를 기다리던 중이라면,
		 * 그 말은 종료되면서 return될 그 상태를 기다린다는 것이다.
		 * 관례적으로 status == 0은 성공을 뜻하고, 0이 아닌 값들은 error를 의미한다.
		 */
			exit(f->R.rdi);
			break;
		case SYS_FORK:
			f->R.rax = fork(f->R.rdi);
			break;
		case SYS_EXEC:
			f->R.rax = exec(f->R.rdi);
			break;
		case SYS_WAIT:
			f->R.rax = wait(f->R.rdi);
			break;
		case SYS_CREATE:
			f->R.rax = create(f->R.rdi, f->R.rsi);
			break;
		case SYS_REMOVE:
			f->R.rax = remove(f->R.rdi);
			break;
		case SYS_OPEN:
			f->R.rax = open(f->R.rdi);
			break;
		case SYS_FILESIZE:
			f->R.rax = filesize(f->R.rdi);
			break;
		case SYS_READ:
			f->R.rax = read(f->R.rdi, f->R.rsi, f->R.rdx);
			break;
		case SYS_WRITE:
			f->R.rax = write(f->R.rdi, f->R.rsi, f->R.rdx);
			break;
		case SYS_SEEK:
			seek(f->R.rdi, f->R.rsi);
			break;
		case SYS_TELL:
			f->R.rax = tell(f->R.rdi);
			break;
		case SYS_CLOSE:
			close(f->R.rdi);
			break;
		default:
			exit(-1);
	}

	printf ("system call!\n");
	thread_exit ();
}

/* ********** ********** ********** project 2 : system call ********** ********** ********** */
void
halt (void) {
	power_off();
}

void
exit (int status) {
	struct thread *t = thread_current();
	t->exit_status = status;
  printf("%s: exit(%d)\n", t->name, t->exit_status); // Process Termination Message
	thread_exit();
}

pid_t
fork (const char *thread_name){
}

int
exec (const char *file) {
}

int
wait (pid_t pid) {
}

create (const char *file, unsigned initial_size) {
	check_address(file);
	return filesys_create(file, initial_size);
}

bool
remove (const char *file) {
		check_address(file);
	return filesys_remove(file);
}

int
open (const char *file) {
}

int
filesize (int fd) {

}

int
read (int fd, void *buffer, unsigned size) {
}

int
write (int fd, const void *buffer, unsigned size) {
}

void
seek (int fd, unsigned position) {
}

unsigned
tell (int fd) {
}

void
close (int fd) {
}

void 
check_address (void *addr)
{
    if (is_kernel_vaddr(addr) || addr == NULL || pml4_get_page(thread_current()->pml4, addr) == NULL)
        exit(-1);
}