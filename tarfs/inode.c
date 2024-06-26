#include <stdlib.h>
#include <errno.h>

#include "tarfs.h"

/*
 * TarFS file operations.
 */
struct file_operations tarfs_file_fops = {
	.read			= tarfs_file_read,
};

/*
 * TarFS directory operations.
 */
struct file_operations tarfs_dir_fops = {
	.getdents64		= tarfs_getdents64,
};

/*
 * TarFS file inode operations.
 */
struct inode_operations tarfs_file_iops = {
	.fops			= &tarfs_file_fops,
};

/*
 * TarFS directory inode operations.
 */
struct inode_operations tarfs_dir_iops = {
	.fops			= &tarfs_dir_fops,
	.lookup			= tarfs_lookup,
};

/*
 * TarFS symbolic link operations.
 */
struct inode_operations tarfs_symlink_iops = {
	.follow_link		= tarfs_follow_link,
	.readlink		= tarfs_readlink,
};

/*
 * Allocate a TarFS inode.
 */
struct inode *tarfs_alloc_inode(struct super_block *sb)
{
	struct tarfs_inode_info *tarfs_inode;

	/* allocate TarFS inode */
	tarfs_inode = (struct tarfs_inode_info *) malloc(sizeof(struct tarfs_inode_info));
	if (!tarfs_inode)
		return NULL;

	return &tarfs_inode->vfs_inode;
}

/*
 * Release a TarFS inode.
 */
void tarfs_put_inode(struct inode *inode)
{
	if (!inode)
		return;

	/* unhash inode */
	htable_delete(&inode->i_htable);

	/* free inode */
	free(tarfs_i(inode));
}

/*
 * Read a TarFS inode.
 */
int tarfs_read_inode(struct inode *inode)
{
	struct tar_entry *entry;

	/* check inode number */
	if (!inode || inode->i_ino >= tarfs_sb(inode->i_sb)->s_ninodes)
		return -EINVAL;

	/* get tar entry */
	entry = tarfs_sb(inode->i_sb)->s_tar_entries[inode->i_ino];
	if (!entry)
		return -EINVAL;

	/* set inode */
	inode->i_mode = entry->mode;
	inode->i_uid = entry->uid;
	inode->i_gid = entry->gid;
	inode->i_size = entry->data_len;
	inode->i_atime = entry->atime;
	inode->i_mtime = entry->mtime;
	inode->i_ctime = entry->ctime;
	inode->i_nlinks = 1;
	tarfs_i(inode)->entry = entry;

	/* set operations */
	if (S_ISDIR(inode->i_mode)) {
		inode->i_op = &tarfs_dir_iops;
		inode->i_nlinks = 2;
	} else if (S_ISLNK(inode->i_mode)) {
		inode->i_op = &tarfs_symlink_iops;
	} else {
		inode->i_op = &tarfs_file_iops;
	}

	return 0;
}
