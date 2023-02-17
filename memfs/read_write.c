#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/fcntl.h>

#include "memfs.h"

/*
 * Read a file.
 */
int memfs_file_read(struct file_t *filp, char *buf, int count)
{
	/* adjust size */
	if (filp->f_pos + count > filp->f_inode->i_size)
		count = filp->f_inode->i_size - filp->f_pos;

	/* copy data */
	memcpy(buf, memfs_i(filp->f_inode)->i_data + filp->f_pos, count);
	filp->f_pos += count;

	/* update inode */
	filp->f_inode->i_atime = current_time();
	filp->f_inode->i_dirt = 1;

	return count;
}

/*
 * Write to a file.
 */
int memfs_file_write(struct file_t *filp, const char *buf, int count)
{
	/* handle append flag */
	if (filp->f_flags & O_APPEND)
		filp->f_pos = filp->f_inode->i_size;

	/* grow file if needed */
	if (filp->f_pos + count > filp->f_inode->i_size) {
		memfs_i(filp->f_inode)->i_data = (char *) realloc(memfs_i(filp->f_inode)->i_data, filp->f_pos + count);
		if (!memfs_i(filp->f_inode)->i_data)
			return -ENOMEM;

		filp->f_inode->i_size = filp->f_pos + count;
	}

	/* copy data */
	memcpy(memfs_i(filp->f_inode)->i_data + filp->f_pos, buf, count);
	filp->f_pos += count;

	/* update inode */
	filp->f_inode->i_mtime = filp->f_inode->i_ctime = current_time();
	filp->f_inode->i_dirt = 1;

	return count;
}
