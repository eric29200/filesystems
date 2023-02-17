#include <errno.h>

#include "vfs.h"

/*
 * Truncate a file.
 */
int vfs_truncate(struct inode_t *root, const char *pathname, off_t length)
{
	struct inode_t *inode;

	/* get inode */
	inode = vfs_namei(root, NULL, pathname, 1);
	if (!inode)
		return -ENOENT;

	/* set new size */
	inode->i_size = length;

	/* truncate */
	if (inode->i_op && inode->i_op->truncate)
		inode->i_op->truncate(inode);

	/* release inode */
	inode->i_dirt = 1;
	vfs_iput(inode);

	return 0;
}
