#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>

#include "ftpfs.h"

/*
 * FTPFS file operations.
 */
struct file_operations ftpfs_file_fops = {
	.open		= ftpfs_open,
	.close		= ftpfs_close,
	.read		= ftpfs_file_read,
	.write		= ftpfs_file_write,
};

/*
 * FTPFS file inode operations.
 */
struct inode_operations ftpfs_file_iops = {
	.fops		= &ftpfs_file_fops,
	.truncate	= NULL,
};

/*
 * FTPFS directory operations.
 */
struct file_operations ftpfs_dir_fops = {
	.getdents64	= ftpfs_getdents64,
};

/*
 * FTPFS directory inode operations.
 */
struct inode_operations ftpfs_dir_iops = {
	.fops		= &ftpfs_dir_fops,
	.lookup		= ftpfs_lookup,
	.create		= ftpfs_create,
	.unlink		= ftpfs_unlink,
	.mkdir		= ftpfs_mkdir,
	.rmdir		= ftpfs_rmdir,
	.rename		= ftpfs_rename,
	.truncate	= NULL,
};

/*
 * FTPFS symbolic link inode operations.
 */
struct inode_operations ftpfs_symlink_iops = {
	.follow_link	= ftpfs_follow_link,
	.readlink	= ftpfs_readlink,
};

/*
 * Allocate a FTPFS inode.
 */
struct inode *ftpfs_alloc_inode(struct super_block *sb)
{
	struct ftpfs_inode_info *ftpfs_inode;

	/* allocate FTPFS inode */
	ftpfs_inode = (struct ftpfs_inode_info *) malloc(sizeof(struct ftpfs_inode_info));
	if (!ftpfs_inode)
		return NULL;

	return &ftpfs_inode->vfs_inode;
}

/*
 * Clear an inode (remove it from cache and release it).
 */
static void ftpfs_clear_inode(struct super_block *sb, struct ftpfs_inode_info *inode)
{
	/* unhash inode */
	list_del(&inode->i_list);
	htable_delete(&inode->vfs_inode.i_htable);

	/* free cached data */
	if (inode->i_cache.data)
		free(inode->i_cache.data);

	/* free path */
	if (inode->i_path)
		free(inode->i_path);

	/* free inode */
	free(inode);

	/* update inodes cache size */
	ftpfs_sb(sb)->s_inodes_cache_size--;
}

/*
 * Clear inodes cache.
 */
static void ftpfs_clear_inode_cache(struct super_block *sb)
{
	struct ftpfs_inode_info *inode;
	struct list_head *pos, *n;

	/* free all unused inodes */
	list_for_each_safe(pos, n, &ftpfs_sb(sb)->s_inodes_cache_list) {
		inode = list_entry(pos, struct ftpfs_inode_info, i_list);

		/* if inode is unused delete it */
		if (inode->vfs_inode.i_ref <= 0)
			ftpfs_clear_inode(sb, inode);

		if (ftpfs_sb(sb)->s_inodes_cache_size <= FTPFS_INODE_HTABLE_SIZE / 3)
			break;
	}
}

/*
 * Release a FTPFS inode.
 */
void ftpfs_put_inode(struct inode *inode)
{
	/* nothing to do */
}

/*
 * Delete a FTPFS inode inode.
 */
void ftpfs_delete_inode(struct inode *inode)
{
	if (!inode || inode->i_nlinks)
		return;

	/* clear inode */
	ftpfs_clear_inode(inode->i_sb, ftpfs_i(inode));
}

/*
 * Build full path (concat dir and name).
 */
char *ftpfs_build_path(struct inode *dir, struct ftpfs_fattr *fattr)
{
	size_t dir_path_len, name_len;
	char *path;

	/* compute name length */
	name_len = strlen(fattr->name);

	/* allocate path */
	dir_path_len = dir ? strlen(ftpfs_i(dir)->i_path) : 0;
	path = (char *) malloc(dir_path_len + name_len + 2);
	if (!path)
		return NULL;

	/* add dir path */
	if (dir)
		memcpy(path, ftpfs_i(dir)->i_path, dir_path_len);

	/* add '/' */
	path[dir_path_len] = '/';

	/* add filename */
	memcpy(path + dir_path_len + 1, fattr->name, name_len);

	/* end last 0 */
	path[dir_path_len + 1 + name_len] = 0;

	return path;
}

/*
 * Load inode data (= store directory listing or link target).
 */
int ftpfs_load_inode_data(struct inode *inode, struct ftpfs_fattr *fattr)
{
	struct ftpfs_inode_info *ftpfs_inode = ftpfs_i(inode);
	struct super_block *sb = inode->i_sb;
	size_t link_len;

	/* data cache already set */
	if (ftpfs_inode->i_cache.data)
		return 0;

	/* symbolic link load target link in inode data cache */
	if (S_ISLNK(inode->i_mode)) {
		link_len = strlen(fattr->link);
		if (link_len > 0) {
			/* allocate inode cache (to store target link) */
			ftpfs_inode->i_cache.data = (char *) malloc(link_len);
			if (!ftpfs_inode->i_cache.data)
				return -ENOMEM;

			/* copy target link to inode cache */
			ftpfs_inode->i_cache.len = link_len;
			ftpfs_inode->i_cache.capacity = link_len;
			memcpy(ftpfs_inode->i_cache.data, fattr->link, link_len);
		}
		ftpfs_inode->i_cache.capacity = strlen(fattr->link);

		return 0;
	}

	/* get list from server if needed */
	if (S_ISDIR(inode->i_mode))
		return ftp_list(sb->s_fd, &ftpfs_sb(sb)->s_addr, ftpfs_inode->i_path, &ftpfs_inode->i_cache);

	return 0;
}

/*
 * Reload inode data.
 */
int ftpfs_reload_inode_data(struct inode *inode, struct ftpfs_fattr *fattr)
{
	struct ftpfs_inode_info *ftpfs_inode = ftpfs_i(inode);

	/* clear cache */
	if (ftpfs_inode->i_cache.data) {
		free(ftpfs_inode->i_cache.data);
		ftpfs_inode->i_cache.data = NULL;
		ftpfs_inode->i_cache.len = 0;
		ftpfs_inode->i_cache.capacity = 0;
	}

	/* load inode data */
	return ftpfs_load_inode_data(inode, fattr);
}

/*
 * Read an inode.
 */
static int ftpfs_read_inode(struct inode *inode, struct ftpfs_fattr *fattr, char *path)
{
	int err;

	/* set inode */
	inode->i_mode = fattr->statbuf.st_mode;
	inode->i_nlinks = fattr->statbuf.st_nlink;
	inode->i_uid = getuid();
	inode->i_gid = getgid();
	inode->i_size = fattr->statbuf.st_size;
	inode->i_blocks = 0;
	inode->i_atime = inode->i_mtime = inode->i_ctime = current_time();
	inode->i_ino = 0;
	inode->i_ref = 1;
	inode->i_op = &ftpfs_dir_iops;
	ftpfs_i(inode)->i_path = path;
	ftpfs_i(inode)->i_cache.data = NULL;
	ftpfs_i(inode)->i_cache.len = 0;
	ftpfs_i(inode)->i_cache.capacity = 0;

	/* load target link in inode data cache */
	if (S_ISLNK(inode->i_mode)) {
		err = ftpfs_load_inode_data(inode, fattr);
		if (err)
			return err;
	}

	/* set operations */
	if (S_ISDIR(inode->i_mode))
		inode->i_op = &ftpfs_dir_iops;
	else if (S_ISLNK(inode->i_mode))
		inode->i_op = &ftpfs_symlink_iops;
	else
		inode->i_op = &ftpfs_file_iops;

	return 0;
}

/*
 * Get a FTPFS inode.
 */
struct inode *ftpfs_iget(struct super_block *sb, struct inode *dir, struct ftpfs_fattr *fattr)
{
	struct htable_link *node;
	struct inode *inode;
	char *path;
	int err;

	/* build full path */
	path = ftpfs_build_path(dir, fattr);
	if (!path)
		return NULL;

	/* try to find inode in cache */
	node = htable_lookupstr(ftpfs_sb(sb)->s_inodes_cache_htable, path, FTPFS_INODE_HTABLE_BITS);
	while (node) {
		inode = htable_entry(node, struct inode, i_htable);
		if (strcmp(ftpfs_i(inode)->i_path, path) == 0) {
			free(path);
			inode->i_ref++;
			return inode;
		}

		node = node->next;
	}

	/* get new empty inode */
	inode = vfs_get_empty_inode(sb);
	if (!inode) {
		free(path);
		return NULL;
	}

	/* read/set inode */
	err = ftpfs_read_inode(inode, fattr, path);
	if (err)
		return NULL;

	/* if inode cache is full, clear cache */
	if (ftpfs_sb(inode->i_sb)->s_inodes_cache_size >= FTPFS_INODE_HTABLE_SIZE)
		ftpfs_clear_inode_cache(inode->i_sb);

	/* hash inode */
	list_add(&ftpfs_i(inode)->i_list, &ftpfs_sb(sb)->s_inodes_cache_list);
	htable_insertstr(ftpfs_sb(sb)->s_inodes_cache_htable, &inode->i_htable, path, FTPFS_INODE_HTABLE_BITS);
	ftpfs_sb(sb)->s_inodes_cache_size++;

	return inode;
}
