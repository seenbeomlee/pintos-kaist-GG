#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/mmu.h"
#include "threads/vaddr.h"
#include "intrinsic.h"
/* ********** ********** ********** project 2 : File I/O ********** ********** ********** */
#include "userprog/syscall.h"
#ifdef VM
#include "vm/vm.h"
#endif

static void process_cleanup (void);
static bool load (const char *file_name, struct intr_frame *if_);
static void initd (void *f_name);
static void __do_fork (void *);

/* General process initializer for initd and other process. */
static void
process_init (void) {
	struct thread *current = thread_current ();
}

/* Starts the first userland program, called "initd", loaded from FILE_NAME.
 * The new thread may be scheduled (and may even exit)
 * before process_create_initd() returns. Returns the initd's
 * thread id, or TID_ERROR if the thread cannot be created.
 * Notice that THIS SHOULD BE CALLED ONCE. */
tid_t
process_create_initd (const char *file_name) {
	char *fn_copy;
	tid_t tid;

	/* Make a copy of FILE_NAME. FILE_NAME의 사본을 만듭니다.
	 * Otherwise there's a race between the caller and load(). 그렇지 않으면 호출자와 load() 사이에 race가 발생합니다. 
	 */
	fn_copy = palloc_get_page (0);
	if (fn_copy == NULL)
		return TID_ERROR;
	strlcpy (fn_copy, file_name, PGSIZE);

/* ********** ********** ********** project 2 : argument parsing ********** ********** ********** */
/* ********** ********** ********** for test ********** ********** ********** */
    /* Project2: for Test Case - 직접 프로그램을 실행할 때에는 이 함수를 사용하지 않지만 make check에서
     * 이 함수를 통해 process_create를 실행하기 때문에 이 부분을 수정해주지 않으면 Test Case의 Thread_name이
     * 커맨드 라인 전체로 바뀌게 되어 Pass할 수 없다.
		 * 즉, 아래의 두 줄은 테스트를 위한 임시 코드임
     */
    char *ptr;
    strtok_r(file_name, " ", &ptr);

	/* Create a new thread to execute FILE_NAME. FILE_NAME을 실행할 새 스레드를 만듭니다. */
	tid = thread_create (file_name, PRI_DEFAULT, initd, fn_copy);
	if (tid == TID_ERROR)
		palloc_free_page (fn_copy);
	return tid;
}

/* A thread function that launches first user process. */
static void
initd (void *f_name) {
#ifdef VM
	supplemental_page_table_init (&thread_current ()->spt);
#endif

	process_init ();

	if (process_exec (f_name) < 0)
		PANIC("Fail to launch initd\n");
	NOT_REACHED ();
}

/* Clones the current process as `name`. Returns the new process's thread id, or
 * TID_ERROR if the thread cannot be created. */
tid_t
process_fork (const char *name, struct intr_frame *if_ UNUSED) {
	/* Clone current thread to new thread.*/
	return thread_create (name,
			PRI_DEFAULT, __do_fork, thread_current ());
}

#ifndef VM
/* Duplicate the parent's address space by passing this function to the
 * pml4_for_each. This is only for the project 2. */
static bool
duplicate_pte (uint64_t *pte, void *va, void *aux) {
	struct thread *current = thread_current ();
	struct thread *parent = (struct thread *) aux;
	void *parent_page;
	void *newpage;
	bool writable;

	/* 1. TODO: If the parent_page is kernel page, then return immediately. */

	/* 2. Resolve VA from the parent's page map level 4. */
	parent_page = pml4_get_page (parent->pml4, va);

	/* 3. TODO: Allocate new PAL_USER page for the child and set result to
	 *    TODO: NEWPAGE. */

	/* 4. TODO: Duplicate parent's page to the new page and
	 *    TODO: check whether parent's page is writable or not (set WRITABLE
	 *    TODO: according to the result). */

	/* 5. Add new page to child's page table at address VA with WRITABLE
	 *    permission. */
	if (!pml4_set_page (current->pml4, va, newpage, writable)) {
		/* 6. TODO: if fail to insert page, do error handling. */
	}
	return true;
}
#endif

/* A thread function that copies parent's execution context.
 * Hint) parent->tf does not hold the userland context of the process.
 *       That is, you are required to pass second argument of process_fork to
 *       this function. */
static void
__do_fork (void *aux) {
	struct intr_frame if_;
	struct thread *parent = (struct thread *) aux;
	struct thread *current = thread_current ();
	/* TODO: somehow pass the parent_if. (i.e. process_fork()'s if_) */
	struct intr_frame *parent_if;
	bool succ = true;

	/* 1. Read the cpu context to local stack. */
	memcpy (&if_, parent_if, sizeof (struct intr_frame));

	/* 2. Duplicate PT */
	current->pml4 = pml4_create();
	if (current->pml4 == NULL)
		goto error;

	process_activate (current);
#ifdef VM
	supplemental_page_table_init (&current->spt);
	if (!supplemental_page_table_copy (&current->spt, &parent->spt))
		goto error;
#else
	if (!pml4_for_each (parent->pml4, duplicate_pte, parent))
		goto error;
#endif

	/* TODO: Your code goes here.
	 * TODO: Hint) To duplicate the file object, use `file_duplicate`
	 * TODO:       in include/filesys/file.h. Note that parent should not return
	 * TODO:       from the fork() until this function successfully duplicates
	 * TODO:       the resources of parent.*/

	process_init ();

	/* Finally, switch to the newly created process. */
	if (succ)
		do_iret (&if_);
error:
	thread_exit ();
}

/* Switch the current execution context to the f_name.
 * Returns -1 on fail. */
int
process_exec (void *f_name) {
	char *file_name = f_name;
	bool success;

	/* We cannot use the intr_frame in the thread structure.
	 * This is because when current thread rescheduled,
	 * it stores the execution information to the member. */
	struct intr_frame _if;
	_if.ds = _if.es = _if.ss = SEL_UDSEG;
	_if.cs = SEL_UCSEG;
	_if.eflags = FLAG_IF | FLAG_MBS;

	/* We first kill the current context */
	process_cleanup ();

/* ********** ********** ********** project 2 : argument parsing ********** ********** ********** */
	char *ptr, *arg;
  int arg_cnt = 0;
  char *arg_list[32];
  // char *ptr == char *save_ptr
	// char *arg == char *token
	// int arg_cht == int argc
 	// char *arg_list[32] == char *argv[64]
 	
	// 문제는, 만약 입력으로 들어오는 명령이 argument를 포함하고 있다면,
	// load()의 코드에서 filename이 이제 진정한 파일의 이름이 아닐 수 있다는 점이다.
	// 따라서, 순수한 실행 파일의 이름을 얻어내기 위해서, 파싱 자체는 load() 함수의 시작 즈음에 이루어져야 한다.
	// 파싱을 위해서 manual에서는 strtok_r() 함수를 사용할 것을 권장하기에, 이를 사용한다.
	// 또한, 대대적 개편 이후 pintOS는 x86이 아닌, x86-64 방식을 채택하게 되었다.
	// 따라서, pintOS에서 포인터 변수의 크기가 8 bytes 씩이다.
	for (arg = strtok_r(file_name, " ", &ptr); arg != NULL; arg = strtok_r(NULL, " ", &ptr))
  arg_list[arg_cnt++] = arg;

	/* And then load the binary */
	// load() 함수는 실행할 프로그램의 binary 파일을 메모리에 올리는 역할을 한다.
	// file_name == 사용자가 커맨드 라인에 적은 f_name을 받은 변수이다.
	// _if == 인터럽트 프레임 구조체이다.
	success = load (file_name, &_if);

/* ********** ********** ********** project 2 : argument parsing ********** ********** ********** */
	argument_stack(arg_list, arg_cnt, &_if);

	/* If load failed, quit. */
	palloc_free_page (file_name);
	if (!success)
		return -1;

/* ********** ********** ********** project 2 : argument parsing ********** ********** ********** */
/* ********** ********** ********** for test ********** ********** ********** */
// 이 함수는 메모리의 내용을 16진수 형식으로 출력해줘서 스택에 저장된 값들을 확인할 수 있다.
//  hex_dump(_if.rsp, _if.rsp, USER_STACK - _if.rsp, true); // 0x47480000	

	/* Start switched process. */
	// load()가 성공적으로 끝난 뒤에는 바로 해당 프로세스를 실행하도록 리턴하는 것으로, process_exec()도 끝난다.
	do_iret (&_if);
	NOT_REACHED ();
}


/* Waits for thread TID to die and returns its exit status.  If
 * it was terminated by the kernel (i.e. killed due to an
 * exception), returns -1.  If TID is invalid or if it was not a
 * child of the calling process, or if process_wait() has already
 * been successfully called for the given TID, returns -1
 * immediately, without waiting.
 *
 * This function will be implemented in problem 2-2.  For now, it
 * does nothing. */
// child process / 자식 프로세스가 종료될 때까지 대기한다.
int
process_wait (tid_t child_tid UNUSED) {
	/* XXX: Hint) The pintos exit if process_wait (initd), we recommend you
	 * XXX:       to add infinite loop here before
	 * XXX:       implementing the process_wait. */

/* ********** ********** ********** project 2 : argument parsing ********** ********** ********** */
/* ********** ********** ********** for test ********** ********** ********** */
// 테스트를 위한 일시적인 루프
// 핀토스는 유저 프로세스를 생성한 후 프로세스 종료를 대기해야 하는데 자식 프로세스가 종료될 때까지 일정시간 대기한다.
// for (int i = 0; i < 1000000000; i++) {}
	
	return -1;
}

/* Exit the process. This function is called by thread_exit (). */
void
process_exit (void) {
	struct thread *curr = thread_current ();
	/* TODO: Your code goes here.
	 * TODO: Implement process termination message (see
	 * TODO: project2/process_termination.html).
	 * TODO: We recommend you to implement process resource cleanup here. */

		for (int fd = 0; fd < curr->fd_idx; fd++)  // FDT 비우기
			close(fd);

    file_close(curr->runn_file);  // 현재 프로세스가 실행중인 파일 종료

    palloc_free_multiple(curr->fdt, FDT_PAGES);

    process_cleanup();

    sema_up(&curr->wait_sema);  // 자식 프로세스가 종료될 때까지 대기하는 부모에게 signal

    sema_down(&curr->exit_sema);  // 부모 프로세스가 종료될 떄까지 대기
}

/* Free the current process's resources. */
static void
process_cleanup (void) {
	struct thread *curr = thread_current ();

#ifdef VM
	supplemental_page_table_kill (&curr->spt);
#endif

	uint64_t *pml4;
	/* Destroy the current process's page directory and switch back
	 * to the kernel-only page directory. */
	pml4 = curr->pml4;
	if (pml4 != NULL) {
		/* Correct ordering here is crucial.  We must set
		 * cur->pagedir to NULL before switching page directories,
		 * so that a timer interrupt can't switch back to the
		 * process page directory.  We must activate the base page
		 * directory before destroying the process's page
		 * directory, or our active page directory will be one
		 * that's been freed (and cleared). */
		curr->pml4 = NULL;
		pml4_activate (NULL);
		pml4_destroy (pml4);
	}
}

/* Sets up the CPU for running user code in the nest thread.
 * This function is called on every context switch. */
void
process_activate (struct thread *next) {
	/* Activate thread's page tables. */
	pml4_activate (next->pml4);

	/* Set thread's kernel stack for use in processing interrupts. */
	tss_update (next);
}

/* We load ELF binaries.  The following definitions are taken
 * from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
#define EI_NIDENT 16

#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
 * This appears at the very beginning of an ELF binary. */
struct ELF64_hdr {
	unsigned char e_ident[EI_NIDENT];
	uint16_t e_type;
	uint16_t e_machine;
	uint32_t e_version;
	uint64_t e_entry;
	uint64_t e_phoff;
	uint64_t e_shoff;
	uint32_t e_flags;
	uint16_t e_ehsize;
	uint16_t e_phentsize;
	uint16_t e_phnum;
	uint16_t e_shentsize;
	uint16_t e_shnum;
	uint16_t e_shstrndx;
};

struct ELF64_PHDR {
	uint32_t p_type;
	uint32_t p_flags;
	uint64_t p_offset;
	uint64_t p_vaddr;
	uint64_t p_paddr;
	uint64_t p_filesz;
	uint64_t p_memsz;
	uint64_t p_align;
};

/* Abbreviations */
#define ELF ELF64_hdr
#define Phdr ELF64_PHDR

static bool setup_stack (struct intr_frame *if_);
static bool validate_segment (const struct Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes,
		bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
 * Stores the executable's entry point into *RIP
 * and its initial stack pointer into *RSP.
 * Returns true if successful, false otherwise. */
static bool
load (const char *file_name, struct intr_frame *if_) {
	struct thread *t = thread_current ();
	struct ELF ehdr;
	struct file *file = NULL;
	off_t file_ofs;
	bool success = false;
	int i;

	/* Allocate and activate page directory. */
	// 각 프로세스가 실행이 될 때, 각 프로세스에 해당하는 VM(virtual memory)이 만들어져야 하므로,
	// 이를 위해 페이지 테이블 엔트리를 생성하는 과정이 우선된다.
	t->pml4 = pml4_create ();
	if (t->pml4 == NULL)
		goto done;
	process_activate (thread_current ());

	/* Open executable file. */
	// 문제는, 만약 입력으로 들어오는 명령이 argument를 포함하고 있다면,
	// load()의 코드에서 filename이 이제 진정한 파일의 이름이 아닐 수 있다는 점이다.
	// 따라서, 순수한 실행 파일의 이름을 얻어내기 위해서, 파싱 자체는 load() 함수의 시작 즈음에 이루어져야 한다.
	// 파싱을 위해서 manual에서는 strtok_r() 함수를 사용할 것을 권장하기에, 이를 사용한다.
	// 또한, 대대적 개편 이후 pintOS는 x86이 아닌, x86-64 방식을 채택하게 되었다.
	// 따라서, pintOS에서 포인터 변수의 크기가 8 bytes 씩이다.
	file = filesys_open (file_name);
	if (file == NULL) {
		printf ("load: %s: open failed\n", file_name);
		goto done;
	}

/* ********** ********** ********** project 2 : argument parsing ********** ********** ********** */
	t->runn_file = file;
	file_deny_write(file);
	
	/* Read and verify executable header. */
	if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
			|| memcmp (ehdr.e_ident, "\177ELF\2\1\1", 7)
			|| ehdr.e_type != 2
			|| ehdr.e_machine != 0x3E // amd64
			|| ehdr.e_version != 1
			|| ehdr.e_phentsize != sizeof (struct Phdr)
			|| ehdr.e_phnum > 1024) {
		printf ("load: %s: error loading executable\n", file_name);
		goto done;
	}

	/* Read program headers. */
	file_ofs = ehdr.e_phoff;
	for (i = 0; i < ehdr.e_phnum; i++) {
		struct Phdr phdr;

		if (file_ofs < 0 || file_ofs > file_length (file))
			goto done;
		file_seek (file, file_ofs);

		if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
			goto done;
		file_ofs += sizeof phdr;
		switch (phdr.p_type) {
			case PT_NULL:
			case PT_NOTE:
			case PT_PHDR:
			case PT_STACK:
			default:
				/* Ignore this segment. */
				break;
			case PT_DYNAMIC:
			case PT_INTERP:
			case PT_SHLIB:
				goto done;
			case PT_LOAD:
			// 그 뒤, 파일을 실제로 VM에 올리는 과정이 진행된다.
			// 파일이 제대로 된 ELF인지 검사하는 과정이 동반되며,
			// 세그먼트 단위로 PT_LOAD의 헤더 타입을 가진 부분을 하나씩 메모리로 올리는 작업을 진행한다.
			// 리눅스 기반 시스템의 기본 바이너리 형식인 ELF(Executable and Linkable Format)을 말한다.
			//ELF binaries: 리눅스에서 실행 가능(Executable)하고 링크 가능(Linkable)한 File의 Format을 ELF라고 한다. 
			// 즉, 실행 가능한 바이너리 또는 오브젝트 파일 등의 형식을 규정한 것이다.
			// https://pu1et-panggg.tistory.com/32
				if (validate_segment (&phdr, file)) {
					bool writable = (phdr.p_flags & PF_W) != 0;
					uint64_t file_page = phdr.p_offset & ~PGMASK;
					uint64_t mem_page = phdr.p_vaddr & ~PGMASK;
					uint64_t page_offset = phdr.p_vaddr & PGMASK;
					uint32_t read_bytes, zero_bytes;
					if (phdr.p_filesz > 0) {
						/* Normal segment.
						 * Read initial part from disk and zero the rest. */
						read_bytes = page_offset + phdr.p_filesz;
						zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
								- read_bytes);
					} else {
						/* Entirely zero.
						 * Don't read anything from disk. */
						read_bytes = 0;
						zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
					}
					if (!load_segment (file, file_page, (void *) mem_page,
								read_bytes, zero_bytes, writable))
						goto done;
				}
				else
					goto done;
				break;
		}
	}

	/* Set up stack. */
	// 전부 메모리로 올린 뒤에 스택을 만드는 과정이 실행된다.
	if (!setup_stack (if_))
		goto done;

	/* Start address. */
	// 어떤 명령부터 실행되는지를 가리키는, 즉, entry point 역할의 rip를 설정하고,
	if_->rip = ehdr.e_entry;

	// 일단 스택이 만들어진 다음에 스택의 내용물을 채워넣어야 하기 때문에, 
	// 우리가 구현할 부분은 setup_stack() 이후에 들어간다.
	/* TODO: Your code goes here.
	 * TODO: Implement argument passing (see project2/argument_passing.html). */

  // 파일을 성공적으로 load() 했음을 나타내기 위해 success = true;를 설정한다.
	success = true;

done:
	/* We arrive here whether the load is successful or not. */
	// 열었던 실행 파일을 닫는 것으로 load()가 끝난다.
	// file_close (file);
	return success;
}

/* Checks whether PHDR describes a valid, loadable segment in
 * FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Phdr *phdr, struct file *file) {
	/* p_offset and p_vaddr must have the same page offset. */
	if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK))
		return false;

	/* p_offset must point within FILE. */
	if (phdr->p_offset > (uint64_t) file_length (file))
		return false;

	/* p_memsz must be at least as big as p_filesz. */
	if (phdr->p_memsz < phdr->p_filesz)
		return false;

	/* The segment must not be empty. */
	if (phdr->p_memsz == 0)
		return false;

	/* The virtual memory region must both start and end within the
	   user address space range. */
	if (!is_user_vaddr ((void *) phdr->p_vaddr))
		return false;
	if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
		return false;

	/* The region cannot "wrap around" across the kernel virtual
	   address space. */
	if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
		return false;

	/* Disallow mapping page 0.
	   Not only is it a bad idea to map page 0, but if we allowed
	   it then user code that passed a null pointer to system calls
	   could quite likely panic the kernel by way of null pointer
	   assertions in memcpy(), etc. */
	if (phdr->p_vaddr < PGSIZE)
		return false;

	/* It's okay. */
	return true;
}

/* ********** ********** ********** project 2 : argument parsing ********** ********** ********** */
// 인자값을 스택에 올리는 함수이다.
// argument_stack 함수는 char **argv로 받은 문자열 배열과 int argc로 받은 인자 개수를 처리한다.
void argument_stack(char **argv, int argc, struct intr_frame *if_) {
    char *arg_addr[100]; // char *agr_addr[100]은 문자열 주소를 저장하는 배열이다.
    int argv_len;

    // argv 배열을 역순으로 stack에 넣는다.
		// 그리고 각 문자열의 길이에 1을 더한 만큼의 공간을 할당하여 문자열을 복사한다.
    for (int i = argc - 1; i >= 0; i--) {
        argv_len = strlen(argv[i]) + 1;
        if_->rsp -= argv_len;
        memcpy(if_->rsp, argv[i], argv_len);
        arg_addr[i] = if_->rsp;
    }

    // stack을 8 byte로 정렬한다.
    while (if_->rsp % 8)
        *(uint8_t *)(--if_->rsp) = 0;

    if_->rsp -= 8;
    memset(if_->rsp, 0, sizeof(char *));

    // arg_addr 배열의 주소를 stack에 넣는다.
    for (int i = argc - 1; i >= 0; i--) {
        if_->rsp -= 8;
        memcpy(if_->rsp, &arg_addr[i], sizeof(char *));
    }

    // 이때, 마지막으로 NULL 포인터를 넣어 인자들의 끝을 표시한다.
    if_->rsp = if_->rsp - 8;
    memset(if_->rsp, 0, sizeof(void *));

    // stack에는 인자의 개수가 RDI 레지스터에, 그리고 인자들의 주소가 RSI 레지스터에 저장된다.
    if_->R.rdi = argc;
    if_->R.rsi = if_->rsp + 8;
}

/* ********** ********** ********** project 2 : FILE I/O ********** ********** ********** */
// 현재 thread fdt에 file을 추가한다.
int 
process_add_file(struct file *f) {
 /** 여기서 왜 굳이 struct file **fdt = curr->fdt; 를 해주는지 모르겠음.
	* 그냥 바로 curr->fdt 에다가 작업해주면 sync error가 생기나?
	* 나중에 코드 수정해보기.
  */
	struct thread *curr = thread_current();
	struct file **fdt = curr->fdt; // 굳이? 어차피 포인터로 참조하는데 왜?

	if (curr->fd_idx >= FDCOUNT_LIMIT) {
		return -1;
	}
	fdt[curr->fd_idx++] = f;

	return curr->fd_idx - 1;
}

// 현재 thread의 fd번째 파일 정보 얻기
struct file 
*process_get_file(int fd) 
{
    struct thread *curr = thread_current();

    if (fd < 0 || fd >= FDCOUNT_LIMIT)
        return NULL;

    return curr->fdt[fd];
}

// 현재 thread의 fdt에서 파일을 삭제한다.
int 
process_close_file(int fd) 
{
    struct thread *curr = thread_current();

    if (fd < 0 || fd >= FDCOUNT_LIMIT)
        return -1;

    curr->fdt[fd] = NULL;
    return 0;
}

#ifndef VM
/* Codes of this block will be ONLY USED DURING project 2.
 * If you want to implement the function for whole project 2, implement it
 * outside of #ifndef macro. */

/* load() helpers. */
static bool install_page (void *upage, void *kpage, bool writable);

/* Loads a segment starting at offset OFS in FILE at address
 * UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
 * memory are initialized, as follows:
 *
 * - READ_BYTES bytes at UPAGE must be read from FILE
 * starting at offset OFS.
 *
 * - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.
 *
 * The pages initialized by this function must be writable by the
 * user process if WRITABLE is true, read-only otherwise.
 *
 * Return true if successful, false if a memory allocation error
 * or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
	ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT (pg_ofs (upage) == 0);
	ASSERT (ofs % PGSIZE == 0);

	file_seek (file, ofs);
	while (read_bytes > 0 || zero_bytes > 0) {
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* Get a page of memory. */
		uint8_t *kpage = palloc_get_page (PAL_USER);
		if (kpage == NULL)
			return false;

		/* Load this page. */
		if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes) {
			palloc_free_page (kpage);
			return false;
		}
		memset (kpage + page_read_bytes, 0, page_zero_bytes);

		/* Add the page to the process's address space. */
		if (!install_page (upage, kpage, writable)) {
			printf("fail\n");
			palloc_free_page (kpage);
			return false;
		}

		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		upage += PGSIZE;
	}
	return true;
}

/* Create a minimal stack by mapping a zeroed page at the USER_STACK */
static bool
setup_stack (struct intr_frame *if_) {
	uint8_t *kpage;
	bool success = false;

	kpage = palloc_get_page (PAL_USER | PAL_ZERO);
	if (kpage != NULL) {
		success = install_page (((uint8_t *) USER_STACK) - PGSIZE, kpage, true);
		if (success)
			if_->rsp = USER_STACK;
		else
			palloc_free_page (kpage);
	}
	return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
 * virtual address KPAGE to the page table.
 * If WRITABLE is true, the user process may modify the page;
 * otherwise, it is read-only.
 * UPAGE must not already be mapped.
 * KPAGE should probably be a page obtained from the user pool
 * with palloc_get_page().
 * Returns true on success, false if UPAGE is already mapped or
 * if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable) {
	struct thread *t = thread_current ();

	/* Verify that there's not already a page at that virtual
	 * address, then map our page there. */
	return (pml4_get_page (t->pml4, upage) == NULL
			&& pml4_set_page (t->pml4, upage, kpage, writable));
}
#else
/* From here, codes will be used after project 3.
 * If you want to implement the function for only project 2, implement it on the
 * upper block. */

static bool
lazy_load_segment (struct page *page, void *aux) {
	/* TODO: Load the segment from the file */
	/* TODO: This called when the first page fault occurs on address VA. */
	/* TODO: VA is available when calling this function. */
}

/* Loads a segment starting at offset OFS in FILE at address
 * UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
 * memory are initialized, as follows:
 *
 * - READ_BYTES bytes at UPAGE must be read from FILE
 * starting at offset OFS.
 *
 * - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.
 *
 * The pages initialized by this function must be writable by the
 * user process if WRITABLE is true, read-only otherwise.
 *
 * Return true if successful, false if a memory allocation error
 * or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
	ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT (pg_ofs (upage) == 0);
	ASSERT (ofs % PGSIZE == 0);

	while (read_bytes > 0 || zero_bytes > 0) {
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* TODO: Set up aux to pass information to the lazy_load_segment. */
		void *aux = NULL;
		if (!vm_alloc_page_with_initializer (VM_ANON, upage,
					writable, lazy_load_segment, aux))
			return false;

		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		upage += PGSIZE;
	}
	return true;
}

/* Create a PAGE of stack at the USER_STACK. Return true on success. */
static bool
setup_stack (struct intr_frame *if_) {
	bool success = false;
	void *stack_bottom = (void *) (((uint8_t *) USER_STACK) - PGSIZE);

	/* TODO: Map the stack on stack_bottom and claim the page immediately.
	 * TODO: If success, set the rsp accordingly.
	 * TODO: You should mark the page is stack. */
	/* TODO: Your code goes here */

	return success;
}
#endif /* VM */
