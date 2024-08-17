#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "devices/disk.h"

/* The disk that contains the file system. */
struct disk *filesys_disk;

static void do_format (void);

/* Initializes the file system module.
 * If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) {
	filesys_disk = disk_get (0, 1);
	if (filesys_disk == NULL)
		PANIC ("hd0:1 (hdb) not present, file system initialization failed");

	inode_init ();

#ifdef EFILESYS
	fat_init ();

	if (format)
		do_format ();

	fat_open ();
#else
	/* Original FS */
	free_map_init ();

	if (format)
		do_format ();

	free_map_open ();
#endif
}

/* Shuts down the file system module, writing any unwritten data
 * to disk. */
void
filesys_done (void) {
	/* Original FS */
#ifdef EFILESYS
	fat_close ();
#else
	free_map_close ();
#endif
}

/* Creates a file named NAME with the given INITIAL_SIZE.
 * Returns true if successful, false otherwise.
 * Fails if a file named NAME already exists,
 * or if internal memory allocation fails. */
/** create(const char *file, unsigned int initial_size) 
 * 첫 번째로 create 함수이다.
 * 생성할 파일의 이름과 만들 파일의 사이즈를 파라미터로 받고,
 * 디스크에 해당 이름으로 파일을 만드는 시스템 콜이다.
 * 파일 생성에 성공하면 true를, 실패하면 false를 return 한다.
*/
bool
filesys_create (const char *name, off_t initial_size) {
	disk_sector_t inode_sector = 0;
	struct dir *dir = dir_open_root ();
	bool success = (dir != NULL
			&& free_map_allocate (1, &inode_sector)
			&& inode_create (inode_sector, initial_size)
			&& dir_add (dir, name, inode_sector));
	if (!success && inode_sector != 0)
		free_map_release (inode_sector, 1);
	dir_close (dir);

	return success;
}

/* Opens the file with the given NAME.
 * Returns the new file if successful or a null pointer
 * otherwise.
 * Fails if no file named NAME exists,
 * or if an internal memory allocation fails. */
/** open(const char *file) 
 * 파라미터로 받은 file과 같은 이름을 가진 파일을 디스크에서 찾아 연다.
 * 파일이 정상적으로 열린 경우, 해당 파일에 대한 구조체 포인터인 struct file *을 반환한다.
 * 파일을 정상적으로 열지 못한 경우(파일을 찾지 못하거나, 내부 메모리 할당에 실패한 경우)에는 -1을 return 한다.
*/
struct file *
filesys_open (const char *name) {
	struct dir *dir = dir_open_root (); 
	struct inode *inode = NULL;

	if (dir != NULL)
		dir_lookup (dir, name, &inode); // 파일을 open하기 전에 먼저 디렉터리에서 파일을 찾는다.
	dir_close (dir);

	return file_open (inode); // 파일이 존재하는 inode를 찾아, 그 inode를 파라미터로 넘겨 file_open 함수를 실행시킨다.
}

/* Deletes the file named NAME.
 * Returns true if successful, false on failure.
 * Fails if no file named NAME exists,
 * or if an internal memory allocation fails. */
/** remove(const char *file)
 * 지울 파일의 이름을 파라미터로 받고, 디스크에서 해당 이름과 같은 이름을 가진 파일을 지우는 시스템 콜이다.
 * 파일 삭제에 성공하면 true를, 실패하면 false를 return 한다.
 */
bool
filesys_remove (const char *name) {
	struct dir *dir = dir_open_root ();
	bool success = dir != NULL && dir_remove (dir, name);
	dir_close (dir);

	return success;
}

/* Formats the file system. */
static void
do_format (void) {
	printf ("Formatting file system...");

#ifdef EFILESYS
	/* Create FAT and save it to the disk. */
	fat_create ();
	fat_close ();
#else
	free_map_create ();
	if (!dir_create (ROOT_DIR_SECTOR, 16))
		PANIC ("root directory creation failed");
	free_map_close ();
#endif

	printf ("done.\n");
}
