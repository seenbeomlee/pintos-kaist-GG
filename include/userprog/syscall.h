#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

/* ********** ********** ********** project 2 : system call ********** ********** ********** */
typedef int pid_t;
#include <stdbool.h>
/* ********** ********** ********** project 2 : system call ********** ********** ********** */

void syscall_init (void);

/* ********** ********** ********** project 2 : system call ********** ********** ********** */
/** void halt (void)
 * 핀토스를 종료시키는 시스템 콜
 * power_off()를 호출해서 pintos를 종료한다.
 * power_off()는 src/include/threads/init.h에 선언되어 있다.
 * 이 함수는 웬만하면 사용되지 않아야 한다.
 * deadlock 상황에 대한 정보 등 뭔가 조금 잃어버릴 수 있다.
 */
void halt (void);
/** 
 * 현재 프로세스를 종료시키는 시스템 콜
 * 종료 시, '프로세스 이름 : exit(status)'를 출력한다. (Process Termination Message)
 * 정상적으로 종료 시, status == 0;
 */
void exit (int status);
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
pid_t fork (const char *thread_name);
int exec (const char *file);
int wait (pid_t pid);

/* ********** ********** ********** project 2 : File I/O ********** ********** ********** */
/** create(const char *file, unsigned int initial_size)
 * 파일을 생성하는 시스템 콜
 * 파일 생성에 성공할 경우, true
 * 파일 생성에 실패할 경우, false를 return한다.
 * file : 생성할 파일의 이름 및 경로 정보
 * initial_size : 생성할 파일의 크기를 말한다.
 * filesys_create 함수를 통해 구현할 수 있다.
 */
bool create (const char *file, unsigned initial_size);
/** remove(const char *file)
 * remove(const char *file)
 * 파일을 삭제하는 시스템 콜
 * 성공할 경우, true
 * 실패할 경우, false를 return한다.
 * file : 제거할 파일의 이름 및 경로 정보
 * filesys_remove 함수를 통해 구현할 수 있다.
 */
bool remove (const char *file);
/** open(const char *file)
 * 
 * filesys_open 함수를 통해 구현할 수 있다.
 */
int open (const char *file);
/** filesize(int fd)
 * 
 * file_length 함수를 통해 쉽게 구현할 수 있다.
 */
int filesize (int fd);
/** read(int fd, void *buffer, unsigned int size)
 * 
 * standard input에서 값을 읽어오는 경우 input_getc 함수를 통해 구현할 수 있고, 
 * 다른 파일을 열어서 읽는 경우 file_read 함수를 통해 구현할 수 있다.
 */
int read (int fd, void *buffer, unsigned size);
/** write(int fd, void *buffer, unsigned int size)
 * 
 * standard output에 값을 쓰는 경우 putbuf를 통해 구현할 수 있으며, 
 * 다른 파일에 값을 쓰는 경우에는 file_write 함수를 통해 구현할 수 있다.
 */
int write (int fd, const void *buffer, unsigned size);
/** seek(int fd, unsigned int position)
 * 
 * file_seek 함수를 통해 구현할 수 있다.
 */
void seek (int fd, unsigned position);
/** tell(int fd)
 * 
 * file_tell 함수를 통해 구현할 수 있다.
 */
unsigned tell (int fd);
/** close(int fd)
 * 
 * file_close를 통해 구현할 수 있다.
 */
void close (int fd);

/**
 * 해당하는 주소 값이 유저 영역에서 사용하는 주소 값인지 확인한다.
 * 유저 영역을 벗어난 영역일 경우 프로세스를 종료시킨다.
 */
void check_address (void *addr);

#endif /* userprog/syscall.h */
