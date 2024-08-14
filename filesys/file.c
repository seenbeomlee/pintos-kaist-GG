#include "filesys/file.h"
#include <debug.h>
#include "filesys/inode.h"
#include "threads/malloc.h"

/* An open file. */
struct file {
	struct inode *inode;        /* File's inode. */
	off_t pos;                  /* Current position. */
	bool deny_write;            /* Has file_deny_write() been called? */
};

/* Opens a file for the given INODE, of which it takes ownership,
 * and returns the new file.  Returns a null pointer if an
 * allocation fails or if INODE is null. */
struct file *
file_open (struct inode *inode) {
	struct file *file = calloc (1, sizeof *file); // 파일을 열 때 calloc을 통해 동적 메모리 할당을 받는다. -> free 해주어야 한다?
	if (inode != NULL && file != NULL) {
		file->inode = inode; // 새로 할당받은 파일과 inode를 연결하여 파일 구조체가 실제 값을 참조할 수 있게 해 주었다.
		file->pos = 0;
		file->deny_write = false;
		return file;
	} else {
		inode_close (inode);
		free (file);
		return NULL;
	}
}

/* Opens and returns a new file for the same inode as FILE.
 * Returns a null pointer if unsuccessful. */
struct file *
file_reopen (struct file *file) {
	return file_open (inode_reopen (file->inode));
}

/* Duplicate the file object including attributes and returns a new file for the
 * same inode as FILE. Returns a null pointer if unsuccessful. */
struct file *
file_duplicate (struct file *file) {
	struct file *nfile = file_open (inode_reopen (file->inode));
	if (nfile) {
		nfile->pos = file->pos;
		if (file->deny_write)
			file_deny_write (nfile);
	}
	return nfile;
}

/* Closes FILE. */
/** close(int fd)
 * open으로 열었던 파일을 닫는 함수이다.
 * void 형태의 함수로 return 값은 없다.
 */
void
file_close (struct file *file) {
	if (file != NULL) {
		// 할당받은 메모리를 반환하는 과정을 확인할 수 있다.
		// 허나, thread가 비정상적으로 종료되거나,
		// 정상적으로 종료하는 상황에서도 열었던 파일들에 대해 close를 진행해주어야 한다.
		file_allow_write (file);
		inode_close (file->inode);
		free (file);
	}
}

/* Returns the inode encapsulated by FILE. */
struct inode *
file_get_inode (struct file *file) {
	return file->inode;
}

/* Reads SIZE bytes from FILE into BUFFER,
 * starting at the file's current position.
 * Returns the number of bytes actually read,
 * which may be less than SIZE if end of file is reached.
 * Advances FILE's position by the number of bytes read. */
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
off_t
file_read (struct file *file, void *buffer, off_t size) {
	off_t bytes_read = inode_read_at (file->inode, buffer, size, file->pos);
	file->pos += bytes_read;
	return bytes_read;
}

/* Reads SIZE bytes from FILE into BUFFER,
 * starting at offset FILE_OFS in the file.
 * Returns the number of bytes actually read,
 * which may be less than SIZE if end of file is reached.
 * The file's current position is unaffected. */
off_t
file_read_at (struct file *file, void *buffer, off_t size, off_t file_ofs) {
	return inode_read_at (file->inode, buffer, size, file_ofs);
}

/* Writes SIZE bytes from BUFFER into FILE,
 * starting at the file's current position.
 * Returns the number of bytes actually written,
 * which may be less than SIZE if end of file is reached.
 * (Normally we'd grow the file in that case, but file growth is
 * not yet implemented.)
 * Advances FILE's position by the number of bytes read. */
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
off_t
file_write (struct file *file, const void *buffer, off_t size) {
	off_t bytes_written = inode_write_at (file->inode, buffer, size, file->pos);
	file->pos += bytes_written;
	return bytes_written;
}

/* Writes SIZE bytes from BUFFER into FILE,
 * starting at offset FILE_OFS in the file.
 * Returns the number of bytes actually written,
 * which may be less than SIZE if end of file is reached.
 * (Normally we'd grow the file in that case, but file growth is
 * not yet implemented.)
 * The file's current position is unaffected. */
off_t
file_write_at (struct file *file, const void *buffer, off_t size,
		off_t file_ofs) {
	return inode_write_at (file->inode, buffer, size, file_ofs);
}

/* Prevents write operations on FILE's underlying inode
 * until file_allow_write() is called or FILE is closed. */
void
file_deny_write (struct file *file) {
	ASSERT (file != NULL);
	if (!file->deny_write) {
		file->deny_write = true;
		inode_deny_write (file->inode);
	}
}

/* Re-enables write operations on FILE's underlying inode.
 * (Writes might still be denied by some other file that has the
 * same inode open.) */
void
file_allow_write (struct file *file) {
	ASSERT (file != NULL);
	if (file->deny_write) {
		file->deny_write = false;
		inode_allow_write (file->inode);
	}
}

/* Returns the size of FILE in bytes. */
/** filesize(int fd)
 * 파라미터로 구할 파일의 파일 디스크립터를 받는다.
 * 해당 파일 디스크립터에 존재하는 파일의 크기를 return 하는 함수이다.
 * 만약 파일을 찾지 못하면 -1을 return 한다.
 */
off_t
file_length (struct file *file) {
	ASSERT (file != NULL);
	return inode_length (file->inode);
}

/* Sets the current position in FILE to NEW_POS bytes from the
 * start of the file. */
/** seek(int fd, unsigned int position)
 * 파일의 offset을 position으로 바꿔주는 함수이다.
 * file_seek 함수를 통해 구현할 수 있다.
 * void 형태로 return 값이 없다.
 */
void
file_seek (struct file *file, off_t new_pos) {
	ASSERT (file != NULL);
	ASSERT (new_pos >= 0);
	file->pos = new_pos;
}

/* Returns the current position in FILE as a byte offset from the
 * start of the file. */
/** tell(int fd)
 * 파일의 offset 값을 return 하는 함수이다.
 * 존재하지 않는 fd 값이 들어오거나, 이미 close 한 파일을 찾는 경우에는 -1을 return 한다.
 */
off_t
file_tell (struct file *file) {
	ASSERT (file != NULL);
	return file->pos;
}
