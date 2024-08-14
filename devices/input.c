#include "devices/input.h"
#include <debug.h>
#include "devices/intq.h"
#include "devices/serial.h"

/* Stores keys from the keyboard and serial port. */
static struct intq buffer;

/* Initializes the input buffer. */
void
input_init (void) {
	intq_init (&buffer);
}

/* Adds a key to the input buffer.
   Interrupts must be off and the buffer must not be full. */
void
input_putc (uint8_t key) {
	ASSERT (intr_get_level () == INTR_OFF);
	ASSERT (!intq_full (&buffer));

	intq_putc (&buffer, key);
	serial_notify ();
}

/* Retrieves a key from the input buffer.
   If the buffer is empty, waits for a key to be pressed. */
/** read(int fd, void *buffer, unsigned int size)
 * read와 write 함수는 다른 시스템 콜들과는 다르게 락을 활용해야 한다.
 * 파일을 읽는 도중, 또는 파일에 값을 쓰는 과정에서 다른 스레드가 파일에 접근하여 값을 바꿔버릴 수 있으므로,
 * 한 파일에는 하나의 스레드만 접근할 수 있게 하기 위해 락을 사용한다.
 * 파라미터로 파일의 디스크립터 fd, 파일에서 값을 읽어 저장할 buffer, 읽어들일 값의 크기인 size를 받아온다.
 * 파일을 제대로 읽어 오지 못하는 경우 -1을 return하며, 다른 경우에는 읽어온 값의 크기를 return 한다.
 * 
 * read의 경우, standard input에서 값을 받아올 수도 있다.
 * 이 경우, 파일디스크립터 값은 0이 된다.
 * standard input에서 값을 읽어오는 경우에는 device > input_getc 함수를 통해 구현할 수 있다.
 */
uint8_t
input_getc (void) {
	enum intr_level old_level;
	uint8_t key;

	old_level = intr_disable ();
	key = intq_getc (&buffer);
	serial_notify ();
	intr_set_level (old_level);

	return key;
}

/* Returns true if the input buffer is full,
   false otherwise.
   Interrupts must be off. */
bool
input_full (void) {
	ASSERT (intr_get_level () == INTR_OFF);
	return intq_full (&buffer);
}
