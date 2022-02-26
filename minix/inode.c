#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "minix.h"

/*
 * Minix file operations.
 */
struct file_operations_t minix_file_fops = {
  .read               = minix_file_read,
  .write              = minix_file_write,
};

/*
 * Minix directory operations.
 */
struct file_operations_t minix_dir_fops = {
  .read               = minix_file_read,
  .write              = minix_file_write,
  .getdents64         = minix_getdents64,
};

/*
 * Minix file inode operations.
 */
struct inode_operations_t minix_file_iops = {
  .fops               = &minix_file_fops,
  .follow_link        = minix_follow_link,
  .readlink           = minix_readlink,
  .truncate           = minix_truncate,
};

/*
 * Minix directory inode operations.
 */
struct inode_operations_t minix_dir_iops = {
  .fops               = &minix_dir_fops,
  .lookup             = minix_lookup,
  .create             = minix_create,
  .link               = minix_link,
  .unlink             = minix_unlink,
  .symlink            = minix_symlink,
  .mkdir              = minix_mkdir,
  .rmdir              = minix_rmdir,
  .rename             = minix_rename,
  .truncate           = minix_truncate,
};

/*
 * Allocate a Minix inode.
 */
int minix_alloc_inode(struct inode_t *inode)
{
  int i;

  if (!inode)
    return -EINVAL;

  /* allocate minix specific inode */
  inode->i_private = malloc(sizeof(struct minix_inode_info_t));
  if (!inode->i_private)
    return -ENOMEM;

  /* reset data zones */
  for (i = 0; i < 10; i++)
    minix_i(inode)->i_zone[i] = 0;

  return 0;
}

/*
 * Release a Minix inode.
 */
void minix_release_inode(struct inode_t *inode)
{
  if (inode && inode->i_private)
    free(inode->i_private);
}

/*
 * Read a Minix V1 inode on disk.
 */
static int minix_read_inode_v1(struct inode_t *inode)
{
  struct minix_inode_info_t *minix_inode = minix_i(inode);
  struct minix_sb_info_t *sbi = minix_sb(inode->i_sb);
  struct minix1_inode_t *raw_inode;
  struct buffer_head_t *bh;
  int inodes_per_block, i;
  uint32_t block;
  
  /* compute inode store block */
  inodes_per_block = inode->i_sb->s_blocksize / sizeof(struct minix1_inode_t);
  block = 2 + sbi->s_imap_blocks + sbi->s_zmap_blocks + (inode->i_ino - 1) / inodes_per_block;
  
  /* read inode store block */
  bh = sb_bread(inode->i_sb, block);
  if (!bh)
    return -EIO;
  
  /* get raw inode */
  raw_inode = &((struct minix1_inode_t *) bh->b_data)[(inode->i_ino - 1) % inodes_per_block];
  
  /* set inode */
  inode->i_mode = raw_inode->i_mode;
  inode->i_nlinks = raw_inode->i_nlinks;
  inode->i_uid = raw_inode->i_uid;
  inode->i_gid = raw_inode->i_gid;
  inode->i_size = raw_inode->i_size;
  inode->i_atime.tv_sec = raw_inode->i_time;
  inode->i_atime.tv_nsec = 0;
  inode->i_mtime.tv_sec = raw_inode->i_time;
  inode->i_mtime.tv_nsec = 0;
  inode->i_ctime.tv_sec = raw_inode->i_time;
  inode->i_ctime.tv_nsec = 0;
  for (i = 0; i < 9; i++)
    minix_inode->i_zone[i] = raw_inode->i_zone[i];
  minix_inode->i_zone[9] = 0;
  
  /* release block buffer */
  brelse(bh);
  
  return 0;
}

/*
 * Read a Minix V2/V3 inode on disk.
 */
static int minix_read_inode_v2(struct inode_t *inode)
{
  struct minix_inode_info_t *minix_inode = minix_i(inode);
  struct minix_sb_info_t *sbi = minix_sb(inode->i_sb);
  struct minix2_inode_t *raw_inode;
  struct buffer_head_t *bh;
  int inodes_per_block, i;
  uint32_t block;
  
  /* compute inode store block */
  inodes_per_block = inode->i_sb->s_blocksize / sizeof(struct minix2_inode_t);
  block = 2 + sbi->s_imap_blocks + sbi->s_zmap_blocks + (inode->i_ino - 1) / inodes_per_block;
  
  /* read inode store block */
  bh = sb_bread(inode->i_sb, block);
  if (!bh)
    return -EIO;
  
  /* get raw inode */
  raw_inode = &((struct minix2_inode_t *) bh->b_data)[(inode->i_ino -1 ) % inodes_per_block];
  
  /* set inode */
  inode->i_mode = raw_inode->i_mode;
  inode->i_nlinks = raw_inode->i_nlinks;
  inode->i_uid = raw_inode->i_uid;
  inode->i_gid = raw_inode->i_gid;
  inode->i_size = raw_inode->i_size;
  inode->i_atime.tv_sec = raw_inode->i_atime;
  inode->i_atime.tv_nsec = 0;
  inode->i_mtime.tv_sec = raw_inode->i_mtime;
  inode->i_mtime.tv_nsec = 0;
  inode->i_ctime.tv_sec = raw_inode->i_ctime;
  inode->i_ctime.tv_nsec = 0;
  for (i = 0; i < 10; i++)
    minix_inode->i_zone[i] = raw_inode->i_zone[i];
  
  /* release block buffer */
  brelse(bh);
  
  return 0;
}

/*
 * Read a Minix inode on disk.
 */
int minix_read_inode(struct inode_t *inode)
{
  struct minix_sb_info_t *sbi = minix_sb(inode->i_sb);
  int err;
  
  /* check inode number */
  if (!inode->i_ino || inode->i_ino > sbi->s_ninodes) {
    fprintf(stderr, "Minix: bad inode number %ld\n", inode->i_ino);
    return -EINVAL;
  }
  
  /* read inode on disk */
  if (sbi->s_version == MINIX_V1)
    err = minix_read_inode_v1(inode);
  else
    err = minix_read_inode_v2(inode);
  
  /* set operations */
  if (S_ISDIR(inode->i_mode))
    inode->i_op = &minix_dir_iops;
  else
    inode->i_op = &minix_file_iops;
  
  return err;
}

/*
 * Write a Minix V1 inode on disk.
 */
static int minix_write_inode_v1(struct inode_t *inode)
{
  struct minix_inode_info_t *minix_inode = minix_i(inode);
  struct minix_sb_info_t *sbi = minix_sb(inode->i_sb);
  struct minix1_inode_t *raw_inode;
  struct buffer_head_t *bh;
  int inodes_per_block, i;
  uint32_t block;
  
  if (!inode)
    return 0;
  
  /* compute inode block */
  inodes_per_block = inode->i_sb->s_blocksize / sizeof(struct minix1_inode_t);
  block = 2 + sbi->s_imap_blocks + sbi->s_zmap_blocks + (inode->i_ino - 1) / inodes_per_block;
  
  /* read inode block */
  bh = sb_bread(inode->i_sb, block);
  if (!bh)
    return -EIO;
  
  /* read minix inode */
  raw_inode = &((struct minix1_inode_t *) bh->b_data)[(inode->i_ino - 1) % inodes_per_block];
  
  /* set on disk inode */
  raw_inode->i_mode = inode->i_mode;
  raw_inode->i_nlinks = inode->i_nlinks;
  raw_inode->i_uid = inode->i_uid;
  raw_inode->i_gid = inode->i_gid;
  raw_inode->i_size = inode->i_size;
  raw_inode->i_time = inode->i_mtime.tv_sec;
  for (i = 0; i < 9; i++)
    raw_inode->i_zone[i] = minix_inode->i_zone[i];
  
  /* release buffer */
  bh->b_dirt = 1;
  brelse(bh);
  
  return 0;
}

/*
 * Write a Minix V2/V3 inode on disk.
 */
static int minix_write_inode_v2(struct inode_t *inode)
{
  struct minix_inode_info_t *minix_inode = minix_i(inode);
  struct minix_sb_info_t *sbi = minix_sb(inode->i_sb);
  struct minix2_inode_t *raw_inode;
  struct buffer_head_t *bh;
  int inodes_per_block, i;
  uint32_t block;
  
  if (!inode)
    return 0;
  
  /* compute inode block */
  inodes_per_block = inode->i_sb->s_blocksize / sizeof(struct minix2_inode_t);
  block = 2 + sbi->s_imap_blocks + sbi->s_zmap_blocks + (inode->i_ino - 1) / inodes_per_block;
  
  /* read inode block */
  bh = sb_bread(inode->i_sb, block);
  if (!bh)
    return -EIO;
  
  /* read minix inode */
  raw_inode = &((struct minix2_inode_t *) bh->b_data)[(inode->i_ino - 1) % inodes_per_block];
  
  /* set on disk inode */
  raw_inode->i_mode = inode->i_mode;
  raw_inode->i_nlinks = inode->i_nlinks;
  raw_inode->i_uid = inode->i_uid;
  raw_inode->i_gid = inode->i_gid;
  raw_inode->i_size = inode->i_size;
  raw_inode->i_atime = inode->i_atime.tv_sec;
  raw_inode->i_mtime = inode->i_mtime.tv_sec;
  raw_inode->i_ctime = inode->i_ctime.tv_sec;
  for (i = 0; i < 10; i++)
    raw_inode->i_zone[i] = minix_inode->i_zone[i];
  
  /* release buffer */
  bh->b_dirt = 1;
  brelse(bh);
  
  return 0;
}

/*
 * Write a Minix inode on disk.
 */
int minix_write_inode(struct inode_t *inode)
{
  struct minix_sb_info_t *sbi = minix_sb(inode->i_sb);
  int err;
  
  /* read inode on disk */
  if (sbi->s_version == MINIX_V1)
    err = minix_write_inode_v1(inode);
  else
    err = minix_write_inode_v2(inode);
  
  return err;
}

/*
 * Release a Minix inode.
 */
int minix_put_inode(struct inode_t *inode)
{
  if (!inode)
    return -EINVAL;
  
  if (!inode->i_nlinks) {
    inode->i_size = 0;
    minix_truncate(inode);
    minix_free_inode(inode);
  }
  
  return 0;
}

/*
 * Read a Minix inode block.
 */
static struct buffer_head_t *minix_inode_getblk(struct inode_t *inode, uint32_t inode_block, int create)
{
  struct minix_inode_info_t *minix_inode = minix_i(inode);

  /* create block if needed */
  if (create && !minix_inode->i_zone[inode_block]) {
    minix_inode->i_zone[inode_block] = minix_new_block(inode->i_sb);
    if (minix_inode->i_zone[inode_block])
      inode->i_dirt = 1;
  }
  
  /* check block */
  if (!minix_inode->i_zone[inode_block])
    return NULL;
  
  /* read block on disk */
  return sb_bread(inode->i_sb, minix_inode->i_zone[inode_block]);
}

/*
 * Read a Minix indirect block.
 */
static struct buffer_head_t *minix_block_getblk(struct super_block_t *sb, struct buffer_head_t *bh,
                                                  uint32_t block_block, int create)
{
  int i;

  if (!bh)
    return NULL;
  
  /* create block if needed */
  i = ((uint32_t *) bh->b_data)[block_block];
  if (create && !i) {
    i = minix_new_block(sb);
    if (i) {
      ((uint32_t *) bh->b_data)[block_block] = i;
      bh->b_dirt = 1;
    }
  }
  
  /* release parent block */
  brelse(bh);
  
  if (!i)
    return NULL;
  
  return sb_bread(sb, i);
}

/*
 * Read a Minix inode block.
 */
struct buffer_head_t *minix_bread(struct inode_t *inode, uint32_t block, int create)
{
  struct minix_sb_info_t *sbi;
  struct super_block_t *sb;
  struct buffer_head_t *bh;
  int addresses_per_block;

  /* get super block */
  sb = inode->i_sb;
  sbi = minix_sb(sb);

  /* check block number */
  if (block >= sbi->s_max_size / sb->s_blocksize)
    return NULL;
  
  /* compute number of addresses per block */
  addresses_per_block = sb->s_blocksize / 4;
  
  /* direct block */
  if (block < 7)
    return minix_inode_getblk(inode, block, create);
  
  /* indirect block */
  block -= 7;
  if (block < addresses_per_block) {
    bh = minix_inode_getblk(inode, 7, create);
    return minix_block_getblk(sb, bh, block, create);
  }
  
  /* double indirect block */
  block -= addresses_per_block;
  if (block < addresses_per_block * addresses_per_block) {
    bh = minix_inode_getblk(inode, 8, create);
    bh = minix_block_getblk(sb, bh, (block / addresses_per_block) & (addresses_per_block - 1), create);
    return minix_block_getblk(sb, bh, block & (addresses_per_block - 1), create);
  }
  
  /* trip indirect block */
  block -= addresses_per_block * addresses_per_block;
  bh = minix_inode_getblk(inode, 9, create);
  bh = minix_block_getblk(sb, bh, (block >> (addresses_per_block * 2)) & (addresses_per_block - 1), create);
  bh = minix_block_getblk(sb, bh, (block / addresses_per_block) & (addresses_per_block - 1), create);
  return minix_block_getblk(sb, bh, block & (addresses_per_block - 1), create);
}
