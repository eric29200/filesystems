#include <unistd.h>
#include <errno.h>
#include <sys/types.h>

#include "ftpfs.h"

/*
 * Read a file.
 */
int ftpfs_file_read(struct file *filp, char *buf, int count)
{
	int fd, n;

	/* check if temporary file is opened */
	if (!filp || !filp->f_private)
		return -EPERM;

	/* get temporary file descriptor */
	fd = *((int *) filp->f_private);

	/* seek to position */
	if (lseek(fd, filp->f_pos, SEEK_SET) == - 1)
		return -EIO;

	/* read data */
	n = read(fd, buf, count);
	if (n < 0)
		return -EIO;

	/* update file position */
	filp->f_pos += n;

	return n;
}

/*
 * Write a file.
 */
int ftpfs_file_write(struct file *filp, const char *buf, int count)
{
	int fd, n;

	/* check if temporary file is opened */
	if (!filp || !filp->f_private)
		return -EPERM;

	/* get temporary file descriptor */
	fd = *((int *) filp->f_private);

	/* seek to position */
	if (lseek(fd, filp->f_pos, SEEK_SET) == - 1)
		return -EIO;

	/* write data */
	n = write(fd, buf, count);
	if (n < 0)
		return -EIO;

	/* update file position */
	filp->f_pos += n;

	/* mark inode dirty */
	filp->f_inode->i_dirt = 1;

	/* update inode size */
	if (filp->f_pos > filp->f_inode->i_size)
		filp->f_inode->i_size = filp->f_pos;

	return n;
}
