#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>

#include "memfs.h"

/*
 * MemFS file operations.
 */
struct file_operations_t memfs_file_fops = {
  .read               = memfs_file_read,
  .write              = memfs_file_write,
};

/*
 * MemFS directory operations.
 */
struct file_operations_t memfs_dir_fops = {
  .getdents64         = memfs_getdents64,
};

/*
 * MemFS file inode operations.
 */
struct inode_operations_t memfs_file_iops = {
  .fops               = &memfs_file_fops,
  .truncate           = memfs_truncate,
};

/*
 * MemFS symbolic link inode operations.
 */
struct inode_operations_t memfs_symlink_iops = {
  .follow_link        = memfs_follow_link,
  .readlink           = memfs_readlink,
};

/*
 * MemFS directory inode operations.
 */
struct inode_operations_t memfs_dir_iops = {
  .fops               = &memfs_dir_fops,
  .lookup             = memfs_lookup,
  .create             = memfs_create,
  .link               = memfs_link,
  .unlink             = memfs_unlink,
  .symlink            = memfs_symlink,
  .mkdir              = memfs_mkdir,
  .rmdir              = memfs_rmdir,
  .rename             = memfs_rename,
  .truncate           = memfs_truncate,
};

/*
 * Allocate a MemFS inode.
 */
struct inode_t *memfs_alloc_inode(struct super_block_t *sb)
{
  struct memfs_inode_info_t *memfs_inode;

  /* allocate memfs specific inode */
  memfs_inode = malloc(sizeof(struct memfs_inode_info_t));
  if (!memfs_inode)
    return NULL;

  /* reset data */
  memfs_inode->i_data = NULL;

  return &memfs_inode->vfs_inode;
}

/*
 * Release a MemFS inode.
 */
void memfs_put_inode(struct inode_t *inode)
{
  /* nothing to do */
}

/*
 * Delete a MemFS inode.
 */
void memfs_delete_inode(struct inode_t *inode)
{
  /* check if inode is still referenced */
  if (inode->i_nlinks)
    return;

  /* remove inode from list */
  list_del(&inode->i_list);

  /* truncate */
  inode->i_size = 0;
  memfs_truncate(inode);

  /* update super block */
  memfs_sb(inode->i_sb)->s_ninodes--;

  /* free inode */
  free(memfs_i(inode));
}

/*
 * Create a new MemFS inode.
 */
struct inode_t *memfs_new_inode(struct super_block_t *sb, mode_t mode)
{
  struct memfs_sb_info_t *sbi = memfs_sb(sb);
  struct inode_t *inode;

  /* get an empty inode */
  inode = vfs_get_empty_inode(sb);
  if (!inode)
    return NULL;

  /* set inode */
  inode->i_ino = sbi->s_inodes_cpt++;
  inode->i_mode = mode;
  inode->i_uid = getuid();
  inode->i_gid = getgid();
  inode->i_atime = inode->i_mtime = inode->i_ctime = current_time();
  inode->i_size = 0;
  inode->i_blocks = 0;
  inode->i_ref = 1;
  inode->i_dirt = 1;
  memfs_i(inode)->i_data = NULL;

  /* set operations */
  if (S_ISDIR(mode)) {
    inode->i_nlinks = 2;
    inode->i_op = &memfs_dir_iops;
  } else if (S_ISLNK(mode)) {
    inode->i_nlinks = 1;
    inode->i_op = &memfs_symlink_iops;
  } else {
    inode->i_nlinks = 1;
    inode->i_op = &memfs_file_iops;
  }

  /* mark inode dirty */
  inode->i_dirt = 1;

  /* update super block */
  sbi->s_ninodes++;

  return inode;
}
