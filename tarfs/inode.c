#include <stdlib.h>
#include <errno.h>

#include "tarfs.h"

/*
 * TarFS file operations.
 */
struct file_operations_t tarfs_file_fops = {
  .read               = tarfs_file_read,
};

/*
 * TarFS directory operations.
 */
struct file_operations_t tarfs_dir_fops = {
  .getdents64         = tarfs_getdents64,
};

/*
 * TarFS file inode operations.
 */
struct inode_operations_t tarfs_file_iops = {
  .fops               = &tarfs_file_fops,
};

/*
 * TarFS directory inode operations.
 */
struct inode_operations_t tarfs_dir_iops = {
  .fops               = &tarfs_dir_fops,
  .lookup             = tarfs_lookup,
};

/*
 * Allocate a TarFS inode.
 */
struct inode_t *tarfs_alloc_inode(struct super_block_t *sb)
{
  struct tarfs_inode_info_t *tarfs_inode;

  /* allocate TarFS inode */
  tarfs_inode = (struct tarfs_inode_info_t *) malloc(sizeof(struct tarfs_inode_info_t));
  if (!tarfs_inode)
    return NULL;

  return &tarfs_inode->vfs_inode;
}

/*
 * Release a TarFS inode.
 */
void tarfs_put_inode(struct inode_t *inode)
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
int tarfs_read_inode(struct inode_t *inode)
{
  struct tar_entry_t *entry;

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
  tarfs_i(inode)->entry = entry;

  /* set operations */
  if (S_ISDIR(inode->i_mode)) {
    inode->i_op = &tarfs_dir_iops;
    inode->i_nlinks = 2;
  } else {
    inode->i_op = &tarfs_file_iops;
    inode->i_nlinks = 1;
  }

  return 0;
}
