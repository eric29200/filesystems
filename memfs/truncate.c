#include <stdlib.h>

#include "memfs.h"

/*
 * Truncate an inode.
 */
void memfs_truncate(struct inode_t *inode)
{
	struct memfs_inode_info_t *memfs_inode = memfs_i(inode);

	/* just free data */
	if (inode->i_size <= 0 && memfs_inode->i_data) {
		free(memfs_inode->i_data);
		memfs_inode->i_data = NULL;
		goto out;
	}

	/* reallocate with new size */
	memfs_inode->i_data = (char *) realloc(memfs_inode->i_data, inode->i_size);

out:
	/* update inode */
	inode->i_mtime = inode->i_ctime = current_time();
	inode->i_dirt = 1;
}
