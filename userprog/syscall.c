#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"

/* ********** ********** ********** project 2 : Hierarchical Process Structure ********** ********** ********** */
#include "threads/mmu.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/palloc.h"
#include "userprog/process.h"

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

/* ********** ********** ********** project 2 : FILE I/O ********** ********** ********** */
struct lock filesys_lock; // 파일 읽기/쓰기용 lock 변수를 global lock으로 설정한다.

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

/* ********** ********** ********** project 2 : FILE I/O ********** ********** ********** */
// 파일 읽기/쓰기용 global lock 초기화 
	lock_init(&filesys_lock);
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
	switch (syscall_number) {
		// Argument 순서는 다음과 같다.
		// %rdi %rsi %rdx %r10 %r8 %r9
		case SYS_HALT:
			halt();
			break;
		case SYS_EXIT:
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
		case SYS_DUP2:
			f->R.rax = dup2(f->R.rdi, f->R.rsi);
			break;
		default:
			exit(-1);
	}
	// printf ("system call!\n");
	// thread_exit ();
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

/* ********** ********** ********** project 2 : Hierarchical Process Structure ********** ********** ********** */
pid_t
fork (const char *thread_name) {
	check_address(thread_name);

	return process_fork(thread_name, NULL);
}

int
exec (const char *cmd_line) {
	check_address(cmd_line);

	off_t size = strlen(cmd_line) + 1;
	char *cmd_copy = palloc_get_page(PAL_ZERO);

	if (cmd_copy == NULL)
		return -1;
	
	/** 
	 * cmd_line을 process_exec에 전달할 복사본을 만든다. 왜 그냥 넘기면 안될까?
	 * process_exec 함수 안에서 전달 받은 인자를 parsing하는 과정이 있기 때문에 복사본을 만들어서 전달해야 한다.
	 * cmd_line*은 const char* 타입이라서 수정할 수 없다.
	 */
	memcpy(cmd_copy, cmd_line, size);

	if (process_exec(cmd_copy) == -1)
		return -1;

	return 0; // process_exec 성공시 return 값 없다. (do_iret)
}

int
wait (pid_t pid) {
	return process_wait(pid);
}
/* ********** ********** ********** project 2 : Hierarchical Process Structure ********** ********** ********** */
 
bool
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
	check_address(file);
	struct file *newfile = filesys_open(file);
	if (newfile == NULL) { // 파일을 불러오는데 실패할 경우, open system call은 -1을 반환한다.
		return -1;
	}

	int fd = process_add_file(newfile);
	if (fd == -1) {
		file_close(newfile);
	}

	return fd;
}

int
filesize (int fd) {
	struct file *file = process_get_file(fd);

	if (file == NULL)
		return -1;

	return file_length(file);
}

/* ********** ********** ********** project 2 : Extend File Descriptor ********** ********** ********** */
int
read (int fd, void *buffer, unsigned length) {
	struct thread *curr = thread_current();
	check_address(buffer);

  struct file *file = process_get_file(fd); // 읽을 파일을 가져온다.

  // fd == 0(stdin)이라면, keyboard로 직접 입력한다.
	// file == STDIN
	if (file == STDIN) {
		/**
		 * char data-type은 단순한 8bits 정수형이지 문자를 저장하는 type이 아니다.
		 * char형을 사용하는 이유는, ASCII가 7bits 형태의 체계를 따르고 있기 때문이다. 
		 * 나머지 1bit는 통신 확인용 페리티 비트이다.
		 */
		int i = 0;
		char c;
		/**
		 * signed char는 음수표시까지 가능하다. -128~127
		 * unsigned char는 양수만 표시 가능하다. 0~255
		 * 따라서, 주소값의 경우 음수가 존재하지 않고, 부호비트가 있을 경우 잘못된 데이터 전송이 일어날 수 있어, unsigned char를 사용한다.
		 */
		unsigned char *buf = buffer; // stdin으로 들어온 문자열에서 음수는 없을 것이다?
		for(; i < length; i++) {
			c = input_getc();
			*buf++ = c;
			if (c == '\0') 
				break;
		}
		return i;
	}
	// // 빈 파일, stdout, stderr를 읽으려고 할 경우
	else if(file == NULL || file == STDOUT || file == STDERR) 
		return -1;
	else {
		//struct file *file = process_get_file(fd); // 읽을 파일을 가져온다.

		// if(file == NULL) // 읽을 파일을 가져오는 것에 실패했을 경우 
		// 	return -1;

		off_t bytes = -1;

		lock_acquire(&filesys_lock);
		bytes = file_read(file, buffer, length);
		lock_release(&filesys_lock);

		return bytes;
	}
}

int
write (int fd, const void *buffer, unsigned length) {
	check_address(buffer);

  struct thread *curr = thread_current();
	off_t bytes = -1;

	struct file *file = process_get_file(fd); // 쓸 파일을 가져온다.

	if(file == STDIN || file == NULL) // stdin에 쓰라고 할 경우 or 쓸 파일이 없는 경우
		return -1;
	else if(file == STDOUT || file == STDERR) { // 1(stdout), 2(stderr) 모두 console로 출력한다.
		putbuf(buffer, length);
		return length;
	}
	else {
		// struct file *file = process_get_file(fd); // 쓸 파일을 가져온다.

		// if (file == NULL) // 쓸 내용이 없을 경우
		// 	return -1;

		lock_acquire(&filesys_lock);
		bytes = file_write(file, buffer, length);
		lock_release(&filesys_lock);

		return bytes;
	}
}

void
seek (int fd, unsigned position) {
	if(fd < 3) return;

	struct file *file = process_get_file(fd);
	if(file == NULL)
		return;
	else
		file_seek(file, position);
}

unsigned
tell (int fd) {
	if(fd < 3) return;

	struct file *file = process_get_file(fd);
	if(file == NULL)
		return;
	else
		return file_tell(file);
}

void
close (int fd) {
	struct thread *curr = thread_current();
  struct file *file = process_get_file(fd);

	if (file == NULL)
		return;

	process_close_file(fd);

	if (file == STDIN || file == STDOUT || file == STDERR) {
		file = 0;
		return;
	}
	else if (file->dup_count == 0)
		file_close(file);
	else
		file->dup_count--;

	// if(fd < 3) return;

	// struct file *file = process_get_file(fd);
	// if(file == NULL)
	// 	return;
	// else {
	// 	process_close_file(fd);
	// 	file_close(file);
	// }
}

void 
check_address (void *addr) {
    if (is_kernel_vaddr(addr) || addr == NULL || pml4_get_page(thread_current()->pml4, addr) == NULL)
        exit(-1);
}

/* ********** ********** ********** project 2 : Extend File Descriptor ********** ********** ********** */
int
dup2(int oldfd, int newfd) {
	if (oldfd < 0 || newfd < 0)
		return -1;

	struct file *oldfile = process_get_file(oldfd);

	if (oldfile == NULL)
		return -1;

	if (oldfd == newfd)
		return newfd;

	struct file *newfile = process_get_file(newfd);

	if (oldfile == newfile) 
		return newfd;

	close(newfd);

	newfd = process_insert_file(newfd, oldfile);

	return newfd;
}