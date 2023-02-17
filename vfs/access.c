#include <fcntl.h>
#include <errno.h>

#include "vfs.h"

/*
 * Check user's permissions for a file.
 */
int vfs_access(struct inode_t *root, const char *pathname, int flags)
{
	struct inode_t *inode;

	/* get inode */
	inode = vfs_namei(root, NULL, pathname, flags & AT_SYMLINK_NOFOLLOW ? 0 : 1);
	if (!inode)
		return -ENOENT;

	/* release inode */
	vfs_iput(inode);
	return 0;
}
