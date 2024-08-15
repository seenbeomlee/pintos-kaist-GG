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
/** void halt (void)
 * 핀토스를 종료시키는 시스템 콜
 * power_off()를 호출해서 pintos를 종료한다.
 * power_off()는 src/include/threads/init.h에 선언되어 있다.
 * 이 함수는 웬만하면 사용되지 않아야 한다.
 * deadlock 상황에 대한 정보 등 뭔가 조금 잃어버릴 수 있다.
 */
void
halt (void) {
	power_off();
}

/** 
 * 현재 프로세스를 종료시키는 시스템 콜
 * 종료 시, '프로세스 이름 : exit(status)'를 출력한다. (Process Termination Message)
 * 정상적으로 종료 시, status == 0;
 */
void
exit (int status) {
	struct thread *t = thread_current();
	t->exit_status = status;
  printf("%s: exit(%d)\n", t->name, t->exit_status); // Process Termination Message
	thread_exit();
}

/**  pid_t fork (const char *thread_name); 
 * https://codable.tistory.com/27
 * thread_name이라는 이름을 가진 현재 process의 복제본인 새 프로세스를 만든다.
 * callee 저장 레지스터인 %rbx, %rsp, %rbp, %r12~%r15를 제외한 레지스터 값은 복사할 필요가 없다.
 * 부모 프로세스에는 자식 프로세스의 pid를 반환해야 한다.
 * 자식 프로세스에는 0을 반환해야 한다.
 * 자식 프로세스에는 파일 식별자 및 가상 메모리 공간을 포함한 복제된 리소스가 있어야 한다.
 * 부모 프로세스는 자식 프로세스가 성공적으로 복제되었는지 여부를 알 때까지 fork에서 반환해서는 안 된다.
 * 즉, 자식 프로세스가 리소스를 복제하지 못하면 부모의 fork() 호출은 TID_ERROR를 반환한다.
*/

// fork는 시스템 콜이므로, 유저 프로그램에서 fork를 호출하면, 먼저 syscall handler 함수로 진입하게 된다.
// child와 parent 간의 context switching에 의해 부모 프로세스의 interrupt frame이 바뀌게 된다.
// 이때, 부모의 interrupt frame에는 kernel의 context가 담겨있다.
// 따라서, 자식 프로세스가 부모 프로세스의 리소스를 복사하는 과정이 담겨있는 __do_fork 함수에서,
// 부모의 user context interrupt frame을 가져오기 위해서는 다른 방법이 필요하다.
// 왜냐하면, 단순히 부모 프로세스에 접근해 parent->tf와 같은 방식으로 부모의 interrupt frame을 가져오면,
// kernel context의 interrupt frame을 가져오게 된다.
pid_t
fork (const char *thread_name){
}

int
exec (const char *file) {
}

int
wait (pid_t pid) {
}

/**
 * 파일을 생성하는 시스템 콜
 * 성공할 경우, true
 * 실패할 경우, false를 return한다.
 * file : 생성할 파일의 이름 및 경로 정보
 * initial_size : 생성할 파일의 크기를 말한다.
 */
bool
create (const char *file, unsigned initial_size) {
	check_address(file);
	return filesys_create(file, initial_size);
}

/**
 * 파일을 삭제하는 시스템 콜
 * 성공할 경우, true
 * 실패할 경우, false를 return한다.
 * file : 제거할 파일의 이름 및 경로 정보
 */
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

/**
 * 해당하는 주소 값이 유저 영역에서 사용하는 주소 값인지 확인한다.
 * 유저 영역을 벗어난 영역일 경우 프로세스를 종료시킨다.
 */
void 
check_address (void *addr)
{
    if (is_kernel_vaddr(addr) || addr == NULL || pml4_get_page(thread_current()->pml4, addr) == NULL)
        exit(-1);
}