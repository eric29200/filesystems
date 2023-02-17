#include <stdio.h>
#include <errno.h>

#include "vfs.h"

/*
 * Read from a file.
 */
ssize_t vfs_read(struct file_t *filp, char *buf, int count)
{
	/* no data to read */
	if (!filp || count <= 0)
		return 0;

	/* read not implemented */
	if (!filp->f_op || !filp->f_op->read)
		return -EPERM;

	return filp->f_op->read(filp, buf, count);
}

/*
 * Write to a file.
 */
ssize_t vfs_write(struct file_t *filp, const char *buf, int count)
{
	/* no data to write */
	if (!filp || count <= 0)
		return 0;

	/* write not implemented */
	if (!filp->f_op || !filp->f_op->write)
		return -EPERM;

	return filp->f_op->write(filp, buf, count);
}

/*
 * Seek a file.
 */
off_t vfs_lseek(struct file_t *filp, off_t offset, int whence)
{
	off_t new_offset;

	/* check fd */
	if (!filp)
		return -EBADF;

	/* compute new offset */
	switch (whence) {
		case SEEK_SET:
			new_offset = offset;
			break;
		case SEEK_CUR:
			new_offset = filp->f_pos + offset;
			break;
		case SEEK_END:
			new_offset = filp->f_inode->i_size + offset;
			break;
		default:
			new_offset = -1;
			break;
	}

	/* bad offset */
	if (new_offset < 0)
		return -EINVAL;

	/* change offset */
	filp->f_pos = new_offset;
	return filp->f_pos;
}
