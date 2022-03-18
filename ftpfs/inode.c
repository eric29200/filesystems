#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>

#include "ftpfs.h"

/*
 * FTPFS file operations.
 */
struct file_operations_t ftpfs_file_fops = {
  .read               = NULL,
  .write              = NULL,
};

/*
 * FTPFS file inode operations.
 */
struct inode_operations_t ftpfs_file_iops = {
  .fops               = &ftpfs_file_fops,
  .follow_link        = NULL,
  .readlink           = NULL,
  .truncate           = NULL,
};

/*
 * FTPFS directory operations.
 */
struct file_operations_t ftpfs_dir_fops = {
  .getdents64         = ftpfs_getdents64,
};

/*
 * FTPFS directory inode operations.
 */
struct inode_operations_t ftpfs_dir_iops = {
  .fops               = &ftpfs_dir_fops,
  .lookup             = ftpfs_lookup,
  .create             = NULL,
  .link               = NULL,
  .unlink             = NULL,
  .symlink            = NULL,
  .mkdir              = NULL,
  .rmdir              = NULL,
  .rename             = NULL,
  .truncate           = NULL,
};

/*
 * Allocate a FTPFS inode.
 */
struct inode_t *ftpfs_alloc_inode(struct super_block_t *sb)
{
  struct ftpfs_inode_info_t *ftpfs_inode;

  /* allocate FTPFS inode */
  ftpfs_inode = (struct ftpfs_inode_info_t *) malloc(sizeof(struct ftpfs_inode_info_t));
  if (!ftpfs_inode)
    return NULL;

  return &ftpfs_inode->vfs_inode;
}

/*
 * Release a FTPFS inode.
 */
void ftpfs_put_inode(struct inode_t *inode)
{
  if (!inode)
    return;

  /* remove inode from list */
  list_del(&inode->i_list);

  /* free cached data */
  if (ftpfs_i(inode)->i_cache.data)
    free(ftpfs_i(inode)->i_cache.data);

  /* free path */
  if (ftpfs_i(inode)->i_path)
    free(ftpfs_i(inode)->i_path);

  /* free inode */
  free(ftpfs_i(inode));
}

/*
 * Get a FTPFS inode.
 */
struct inode_t *ftpfs_iget(struct super_block_t *sb, struct inode_t *dir, const char *name, size_t name_len,
                           struct stat *statbuf)
{
  struct inode_t *inode;
  size_t dir_path_len;

  /* get new empty inode */
  inode = vfs_get_empty_inode(sb);
  if (!inode)
    return NULL;

  /* set inode */
  inode->i_mode = statbuf->st_mode;
  inode->i_nlinks = statbuf->st_nlink;
  inode->i_uid = getuid();
  inode->i_gid = getgid();
  inode->i_size = statbuf->st_size;
  inode->i_blocks = 0;
  inode->i_atime = inode->i_mtime = inode->i_ctime = current_time();
  inode->i_ino = 0;
  inode->i_ref = 1;
  inode->i_op = &ftpfs_dir_iops;
  ftpfs_i(inode)->i_cache.data = NULL;
  ftpfs_i(inode)->i_cache.len = 0;
  ftpfs_i(inode)->i_cache.capacity = 0;

  /* allocate path */
  dir_path_len = dir ? strlen(ftpfs_i(dir)->i_path) : 0;
  ftpfs_i(inode)->i_path = (char *) malloc(dir_path_len + name_len + 2);
  if (!ftpfs_i(inode)->i_path) {
    inode->i_ref = 0;
    vfs_iput(inode);
    return NULL;
  }

  /* add dir path */
  if (dir)
    memcpy(ftpfs_i(inode)->i_path, ftpfs_i(dir)->i_path, dir_path_len);

  /* add '/' */
  ftpfs_i(inode)->i_path[dir_path_len] = '/';

  /* add filename */
  if (name)
    memcpy(ftpfs_i(inode)->i_path + dir_path_len + 1, name, name_len);

  /* end last 0 */
  ftpfs_i(inode)->i_path[dir_path_len + 1 + name_len] = 0;

  /* set operations */
  if (S_ISDIR(inode->i_mode))
    inode->i_op = &ftpfs_dir_iops;
  else
    inode->i_op = &ftpfs_file_iops;

  return inode;
}
