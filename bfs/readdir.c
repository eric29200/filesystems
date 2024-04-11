#include <string.h>
#include <errno.h>

#include "bfs.h"

/*
 * Get directory entries.
 */
int bfs_getdents64(struct file *filp, void *dirp, size_t count)
{
	struct bfs_dir_entry de;
	struct dirent64 *dirent;
	int entries_size;
	size_t name_len;

	/* for each entry */
	for (entries_size = 0, dirent = (struct dirent64 *) dirp;;) {
		/* read bfs dir entry */
		if (bfs_file_read(filp, (char *) &de, BFS_DIRENT_SIZE) != BFS_DIRENT_SIZE)
			return entries_size;

		/* skip null entries */
		if (le16toh(de.d_ino) == 0)
			continue;

		/* not enough space to fill in next dir entry : break */
		name_len = strnlen(de.d_name, BFS_NAME_LEN);
		if (count < sizeof(struct dirent64) + name_len + 1) {
			filp->f_pos -= BFS_DIRENT_SIZE;
			return entries_size;
		}

		/* fill in dirent */
		dirent->d_inode = le16toh(de.d_ino);
		dirent->d_off = 0;
		dirent->d_reclen = sizeof(struct dirent64) + name_len + 1;
		dirent->d_type = 0;
		memcpy(dirent->d_name, de.d_name, name_len);
		dirent->d_name[name_len] = 0;

		/* go to next entry */
		count -= dirent->d_reclen;
		entries_size += dirent->d_reclen;
		dirent = (struct dirent64 *) ((char *) dirent + dirent->d_reclen);
	}

	return entries_size;
}
