#include <stdio.h>
#include <errno.h>

#include "ftpfs.h"

/*
 * Follow a link (inode will be released).
 */
int ftpfs_follow_link(struct inode_t *dir, struct inode_t *inode, struct inode_t **res_inode)
{
	char target[FTPFS_NAME_LEN];

	/* reset result inode */
	*res_inode = NULL;

	if (!inode)
		return -ENOENT;

	/* check if a inode is a link */
	if (!S_ISLNK(inode->i_mode)) {
		*res_inode = inode;
		return 0;
	}

	/* get target */
	memcpy(target, ftpfs_i(inode)->i_cache.data, ftpfs_i(inode)->i_cache.len);
	target[ftpfs_i(inode)->i_cache.len] = 0;

	/* release inode */
	vfs_iput(inode);

	/* resolve target inode */
	*res_inode = vfs_namei(NULL, dir, target, 0);
	if (!*res_inode)
		return -EACCES;

	return 0;
}

/*
 * Read value of a symbolic link.
 */
ssize_t ftpfs_readlink(struct inode_t *inode, char *buf, size_t bufsize)
{
	struct ftpfs_inode_info_t *ftpfs_inode = ftpfs_i(inode);
	ssize_t len;

	/* inode must be a link */
	if (!S_ISLNK(inode->i_mode)) {
		vfs_iput(inode);
		return -EINVAL;
	}

	/* limit buffer size to target link */
	bufsize--;
	if (bufsize > ftpfs_inode->i_cache.len)
		bufsize = ftpfs_inode->i_cache.len;

	/* copy target */
	for (len = 0; len < bufsize; len++)
		buf[len] = ftpfs_inode->i_cache.data[len];
	buf[len] = 0;

	/* release inode */
	vfs_iput(inode);

	return len;
}
