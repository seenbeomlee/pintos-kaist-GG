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
 * 만약, 부모 프로세스가 현재 유저 프로그램의 종료를 기다리던 중이라면,
 * 그 말은 종료되면서 return될 그 상태를 기다린다는 것이다.
 * 관례적으로 status == 0은 성공을 뜻하고, 0이 아닌 값들은 error를 의미한다.
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

/**
 * 현재의 process가 cmd_line에서 이름이 주어지는 실행가능한 process로 변경된다.
 * 이때, 주어진 인자들을 전달한다. 성공적으로 진행된다면 어떤 것도 반환하지 않는다.
 * 만약, 프로그램이 프로세스를 로드하지 못하거나 다른 이유로 돌리지 못하게 되면 exit state -1을 반환하며, 프로세스가 종료된다.
 * 이 함수는 exec 함수를 호출한 thread의 이름은 바꾸지 않는다.
 * file descriptor는 exec 함수 호출 시에 열린 상태로 있다.
 */
int exec (const char *cmd_line);

/**
 * 자식 프로세스(pid)를 기다려서, 자식의 종료 상태(exit status)를 가져온다.
 * 만약 pid가 아직 살아있으면, 종료될 때까지 기다린다.
 * 종료가 되면 그 프로세스가 exit 함수로 전달해준 상태(exit status)를 반환한다.
 * 
 * 만약, pid(자식 프로세스)가 exit() 함수를 호출하지 않고, 커널에 의해서 종료된다면(e.g exception에 의해 죽는 경우)
 * wait(pid)는 -1을 반환해야 한다.
 * 
 * 부모 process가 wait 함수를 호출한 시점에서, 이미 종료되어버린 자식 프로세스를 기다리도록 하는 것은 합당하지만,
 * kernal은 부모 프로세스에게 자식의 종료 상태를 알려주던지, 커널에 의해 종료되었다는 사실을 알려주어야 한다.
 * 자식들을 상속되지 않기 때문에, 한 프로세스는 어떤 주어진 자식에 대해서 최대 한번만 wait할 수 있다.
 * => 따라서, wait는 어렵다..
 */
int wait (pid_t pid);

/* ********** ********** ********** project 2 : File I/O ********** ********** ********** */
/** create(const char *file, unsigned int initial_size)
 * 파일을 생성하는 시스템 콜 != 파일을 여는 시스템 콜(open)과 구분되어야 한다.
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
 * 파일은 열려있는지, 닫혀있는지 여부와 관계없이 삭제될 수 있다.
 * 다만, 파일을 삭제하는 것이 그 파일을 닫았다는 것을 의미하지는 않는다.
 * 성공할 경우, true
 * 실패할 경우, false를 return한다.
 * file : 제거할 파일의 이름 및 경로 정보
 * filesys_remove 함수를 통해 구현할 수 있다.
 */
bool remove (const char *file);

/** open(const char *file)
 * 파일을 열 떄 사용하는 시스템 콜이다.
 * 파일이 없을 경우 실패하고, -1을 return 한다.
 * 파일을 여는데 성공했을 경우, fd를 생성하고, fd를 return 한다.
 * file == 파일의 이름 및 경로 정보
 * filesys_open 함수를 통해 구현할 수 있다.
 */
int open (const char *file);

/** filesize(int fd)
 * 파일의 크기를 알려주는 시스템 콜이다.
 * 성공시 파일의 크기를 return 한다.
 * 실패시 -1을 return 한다.
 * file_length 함수를 통해 쉽게 구현할 수 있다.
 */
int filesize (int fd);

/** read(int fd, void *buffer, unsigned int size)
 * 열린 파일의 데이터를 읽는 시스템 콜
 * standard input에서 값을 읽어오는 경우 input_getc 함수를 통해 구현할 수 있고, 
 * 다른 파일을 열어서 읽는 경우 file_read 함수를 통해 구현할 수 있다.
 * 성공시 읽은 바이트 수를 return,
 * 실패시 -1을 return 한다.
 * buffer : 읽은 데이터를 저장할 버퍼의 주소 값
 * size : 읽을 데이터 크기
 * fd 값이 0일 때, 키보드(standard input)의 데이터를 읽어 버퍼에 저장한다. (input_getc()를 이용)
 */
int read (int fd, void *buffer, unsigned length);

/** write(int fd, void *buffer, unsigned int size)
 * 
 * 열린 파일의 데이터를 기록하는 시스템 콜이다.
 * 성공시 기록한 data의 bytes 수를 return 한다.
 * 실패시 -1을 return 한다.
 * buffer : 기록할 데이터를 저장한 버퍼의 주소 값이다.
 * length : 기록할 데이터의 크기이다.
 * standard output에 값을 쓰는 경우 putbuf를 통해 구현할 수 있으며, 
 * 다른 파일에 값을 쓰는 경우에는 file_write 함수를 통해 구현할 수 있다.
 * 
 * fd == 1(stdout)일 때는, 요구에 따라서 버퍼에 저장된 데이터를 화면에 출력한다.
 * 이는 putbuf()를 이용한다.
 */
int write (int fd, const void *buffer, unsigned length);

/** seek(int fd, unsigned int position)
 * 열린 파일의 위치(offset)를 이동하는 시스템 콜이다.
 * position == 현재 위치(offset)를 기준으로 이동할 거리를 의미한다.
 * file_seek 함수를 통해 구현할 수 있다.
 */
void seek (int fd, unsigned position);

/** tell(int fd)
 * 열린 파일의 위치(offset)를 알려주는 시스템 콜이다.
 * 성공시 파일의 위치(offset)를 return 한다.
 * 실패시 -1을 return 한다.
 * file_tell 함수를 통해 구현할 수 있다.
 */
unsigned tell (int fd);

/** close(int fd)
 * 
 * file_close를 통해 구현할 수 있다.
 */
void close (int fd);

/**
 * 1. null 포인터
 * 2. 매핑되지 않은 가상 메모리를 카리키는 포인터
 * 3. 커널 가상 주소 공간(KERN_BASE 이상)을 전달할 수 있으므로, 커널은 이를 매우 신중하게 처리해야 한다.
 * 이러한 유형의 잘못된 포인터는 거부되어야 한다.
 * 문제를 일으킨 프로세스를 종료하고, 그 자원을 해제해야 한다.
 */
void check_address (void *addr);

/**
 * dup2() 시스템 콜은 인자로 받은 oldfd 파일 디스크립터의 복사본을 생성하고,
 * 이 복사본의 파일디스크립터 값은 인자로 받은 newfd 값이 되게 한다.
 * dup2() 시스템 콜이 파일 디스크립터를 복사해서 새 파일 디스크립터를 생성하는 데 성공한다면 newfd를 return 한다.
 * 만약, newfd 값이 이전에 이미 열려있었다면, newfd는 재사용되기 전에 조용히 닫힌다.
 * 만약, oldfd가 유효한 파일 디스크립터가 아니라면, dup2() 콜은 실패하여 1을 return 하고, newfd는 닫히지 않는다.
 * 만약, oldfd가 유효한 파일 디스크립터이고, newfd는 oldfd와 같은 값을 가지고 있다면 dup2()가 할일은 따로 없고, newfd 값을 return 한다.
 * 이 시스템콜로부터 성공적으로 값을 반환받은 후에, oldfd와 newfd는 호환해서 사용이 가능하다.
 * 이 둘은 서로 다른 파일 디스크립터이긴하지만, 똑같은 열린 파일 디스크립터를 의미하기 때문에, 같은 file offset과 status flags를 공유하고 있다.
 * 예를 들어, 만약에 다른 디스크립터가 seek을 사용해서 file offset이 수정되었다면, 다른 스크립터에서도 이 값은 똑같이 수정된다.
 * 
 * 과제의 목표는 다음과 같다.
 * 1.stdin에 대한 fd를 닫아주면, 그 어떤 input도 읽어들이지 않는다.
 * 2.stdout에 대한 fd를 닫아주면, 그 어떤 output도 process에 의해 출력되지 않는다.
 * dup2를 구현해 주어진 stdin/stdout/file에 대한 fd를 fd_table 내에 복사해준다.
 */
int dup2(int oldfd, int newfd);

#endif /* userprog/syscall.h */
