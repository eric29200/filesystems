#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>

#include "memfs.h"

/*
 * Follow a link (inode will be released).
 */
int memfs_follow_link(struct inode_t *dir, struct inode_t *inode, struct inode_t **res_inode)
{
	/* reset result inode */
	*res_inode = NULL;

	if (!inode)
		return -ENOENT;

	/* check if a inode is a link */
	if (!S_ISLNK(inode->i_mode)) {
		*res_inode = inode;
		return 0;
	}

	/* release inode */
	vfs_iput(inode);

	/* resolve target inode */
	*res_inode = vfs_namei(NULL, dir, memfs_i(inode)->i_data, 0);
	if (!*res_inode)
		return -EACCES;

	/* release block buffer */
	return 0;
}

/*
 * Read value of a symbolic link.
 */
ssize_t memfs_readlink(struct inode_t *inode, char *buf, size_t bufsize)
{
	/* inode must be a link */
	if (!S_ISLNK(inode->i_mode)) {
		vfs_iput(inode);
		return -EINVAL;
	}

	/* limit buffer size to inode size */
	if (bufsize > inode->i_size)
		bufsize = inode->i_size;

	/* release inode */
	vfs_iput(inode);

	/* copy target */
	memcpy(buf, memfs_i(inode)->i_data, bufsize);

	return bufsize;
}
