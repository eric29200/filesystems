#include <errno.h>

#include "tarfs.h"

/*
 * Follow a link (inode will be released).
 */
int tarfs_follow_link(struct inode_t *dir, struct inode_t *inode, struct inode_t **res_inode)
{
	struct tarfs_inode_info_t *tarfs_inode = tarfs_i(inode);

	/* reset result inode */
	*res_inode = NULL;

	if (!inode)
		return -ENOENT;

	/* check if a inode is a link */
	if (!S_ISLNK(inode->i_mode) || !tarfs_inode->entry->linkname) {
		*res_inode = inode;
		return 0;
	}

	/* resolve target inode */
	*res_inode = vfs_namei(NULL, dir, tarfs_inode->entry->linkname, 0);
	if (!*res_inode) {
		vfs_iput(inode);
		return -EACCES;
	}

	/* release inode */
	vfs_iput(inode);

	return 0;
}

/*
 * Read value of a symbolic link.
 */
ssize_t tarfs_readlink(struct inode_t *inode, char *buf, size_t bufsize)
{
	struct tarfs_inode_info_t *tarfs_inode = tarfs_i(inode);
	ssize_t len;

	/* inode must be a link */
	if (!S_ISLNK(inode->i_mode)) {
		vfs_iput(inode);
		return -EINVAL;
	}

	/* no link name */
	if (!tarfs_inode->entry->linkname)
		return 0;

	/* get link name length and adjust buf size */
	len = strlen(tarfs_i(inode)->entry->linkname);
	if (len > bufsize)
		len = bufsize;

	/* copy link name */
	memcpy(buf, tarfs_i(inode)->entry->linkname, len);

	return len;
}
