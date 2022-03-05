#include <stdlib.h>
#include <errno.h>

#include "ext2.h"

/*
 * Ext2 file operations.
 */
struct file_operations_t ext2_file_fops = {
  .read               = ext2_file_read,
  .write              = NULL,
};

/*
 * Ext2 directory operations.
 */
struct file_operations_t ext2_dir_fops = {
  .getdents64         = ext2_getdents64,
};

/*
 * Ext2 file inode operations.
 */
struct inode_operations_t ext2_file_iops = {
  .fops               = &ext2_file_fops,
  .follow_link        = NULL,
  .readlink           = NULL,
  .truncate           = NULL,
};

/*
 * Ext2 directory inode operations.
 */
struct inode_operations_t ext2_dir_iops = {
  .fops               = &ext2_dir_fops,
  .lookup             = NULL,
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
 * Allocate a Ext2 inode.
 */
struct inode_t *ext2_alloc_inode(struct super_block_t *sb)
{
  struct ext2_inode_info_t *ext2_inode;
  int i;

  /* allocate ext2 specific inode */
  ext2_inode = malloc(sizeof(struct ext2_inode_info_t));
  if (!ext2_inode)
    return NULL;

  /* reset data zones */
  for (i = 0; i < 15; i++)
    ext2_inode->i_data[i] = 0;

  return &ext2_inode->vfs_inode;
}

/*
 * Release a ext2 inode.
 */
void ext2_put_inode(struct inode_t *inode)
{
  if (!inode)
    return;

  /* free inode */
  free(ext2_i(inode));
}

/*
 * Read a Ext2 inode.
 */
int ext2_read_inode(struct inode_t *inode)
{
  struct ext2_inode_info_t *ext2_inode = ext2_i(inode);
  struct ext2_sb_info_t *sbi = ext2_sb(inode->i_sb);
  uint32_t block_group, offset, block;
  struct ext2_inode_t *raw_inode;
  struct ext2_group_desc_t *gdp;
  struct buffer_head_t *bh;
  int i;

  /* check inode number */
  if ((inode->i_ino != EXT2_ROOT_INO && inode->i_ino < sbi->s_first_ino)
      || inode->i_ino > le32toh(sbi->s_es->s_inodes_count))
    return -EINVAL;

  /* get group descriptor */
  block_group = (inode->i_ino - 1) / sbi->s_inodes_per_group;
  gdp = ext2_get_group_desc(inode->i_sb, block_group, NULL);
  if (!gdp)
    return -EINVAL;

  /* get inode table block */
  offset = ((inode->i_ino - 1) % sbi->s_inodes_per_group) * sbi->s_inode_size;
  block = le32toh(gdp->bg_inode_table) + (offset >> EXT2_BLOCK_SIZE_BITS);

  /* read inode table block buffer */
  bh = sb_bread(inode->i_sb, block);
  if (!bh)
    return -EIO;

  /* get inode */
  offset &= (EXT2_BLOCK_SIZE - 1);
  raw_inode = (struct ext2_inode_t *) (bh->b_data + offset);

  /* set generic inode */
  inode->i_mode = le16toh(raw_inode->i_mode);
  inode->i_nlinks = le16toh(raw_inode->i_links_count);
  inode->i_uid = le16toh(raw_inode->i_uid) | (le16toh(raw_inode->i_uid_high) << 16);
  inode->i_gid = le16toh(raw_inode->i_gid) | (le16toh(raw_inode->i_gid_high) << 16);
  inode->i_size = le32toh(raw_inode->i_size);
  inode->i_atime.tv_sec = le32toh(raw_inode->i_atime);
  inode->i_atime.tv_nsec = 0;
  inode->i_mtime.tv_sec = le32toh(raw_inode->i_mtime);
  inode->i_mtime.tv_nsec = 0;
  inode->i_ctime.tv_sec = le32toh(raw_inode->i_ctime);
  inode->i_ctime.tv_nsec = 0;
  for (i = 0; i < EXT2_N_BLOCKS; i++)
    ext2_inode->i_data[i] = raw_inode->i_block[i];

  /* set operations */
  /* set operations */
  if (S_ISDIR(inode->i_mode))
    inode->i_op = &ext2_dir_iops;
  else
    inode->i_op = &ext2_file_iops;

  /* release block buffer */
  brelse(bh);

  return 0;
}

/*
 * Read a Ext2 inode block.
 */
struct buffer_head_t *ext2_bread(struct inode_t *inode, uint32_t block)
{
  struct ext2_inode_info_t *ext2_inode = ext2_i(inode);
  struct super_block_t *sb = inode->i_sb;

  /* direct block */
  if (block < EXT2_NDIR_BLOCKS && ext2_inode->i_data[block])
    return sb_bread(sb, ext2_inode->i_data[block]);

  return NULL;
}
