#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "minixfs.h"

/*
 * Minix file operations.
 */
struct file_operations_t minixfs_file_fops = {
  .read               = minixfs_file_read,
  .write              = minixfs_file_write,
};

/*
 * Minix directory operations.
 */
struct file_operations_t minixfs_dir_fops = {
  .read               = minixfs_file_read,
  .write              = minixfs_file_write,
  .getdents64         = minixfs_getdents64,
};

/*
 * Minix file inode operations.
 */
struct inode_operations_t minixfs_file_iops = {
  .fops               = &minixfs_file_fops,
  .follow_link        = minixfs_follow_link,
  .readlink           = minixfs_readlink,
  .truncate           = minixfs_truncate,
};

/*
 * Minix directory inode operations.
 */
struct inode_operations_t minixfs_dir_iops = {
  .fops               = &minixfs_dir_fops,
  .lookup             = minixfs_lookup,
  .create             = minixfs_create,
  .link               = minixfs_link,
  .unlink             = minixfs_unlink,
  .symlink            = minixfs_symlink,
  .mkdir              = minixfs_mkdir,
  .rmdir              = minixfs_rmdir,
  .rename             = minixfs_rename,
  .truncate           = minixfs_truncate,
};

/*
 * Read a Minix V1 inode on disk.
 */
static int minixfs_read_inode_v1(struct inode_t *inode)
{
  struct minix_sb_info_t *sbi = minixfs_sb(inode->i_sb);
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
  inode->i_atime = raw_inode->i_time;
  inode->i_mtime = raw_inode->i_time;
  inode->i_ctime = raw_inode->i_time;
  for (i = 0; i < 9; i++)
    inode->i_zone[i] = raw_inode->i_zone[i];
  inode->i_zone[9] = 0;
  
  /* release block buffer */
  brelse(bh);
  
  return 0;
}

/*
 * Read a Minix V2/V3 inode on disk.
 */
static int minixfs_read_inode_v2(struct inode_t *inode)
{
  struct minix_sb_info_t *sbi = minixfs_sb(inode->i_sb);
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
  inode->i_atime = raw_inode->i_atime;
  inode->i_mtime = raw_inode->i_mtime;
  inode->i_ctime = raw_inode->i_ctime;
  for (i = 0; i < 10; i++)
    inode->i_zone[i] = raw_inode->i_zone[i];
  
  /* release block buffer */
  brelse(bh);
  
  return 0;
}

/*
 * Read a Minix inode on disk.
 */
int minixfs_read_inode(struct inode_t *inode)
{
  struct minix_sb_info_t *sbi = minixfs_sb(inode->i_sb);
  int err;
  
  /* check inode number */
  if (!inode->i_ino || inode->i_ino > sbi->s_ninodes) {
    fprintf(stderr, "MinixFS: bad inode number %ld\n", inode->i_ino);
    return -EINVAL;
  }
  
  /* read inode on disk */
  if (sbi->s_version == MINIXFS_V1)
    err = minixfs_read_inode_v1(inode);
  else
    err = minixfs_read_inode_v2(inode);
  
  /* set operations */
  if (S_ISDIR(inode->i_mode))
    inode->i_op = &minixfs_dir_iops;
  else
    inode->i_op = &minixfs_file_iops;
  
  return err;
}

/*
 * Write a Minix V1 inode on disk.
 */
static int minixfs_write_inode_v1(struct inode_t *inode)
{
  struct minix_sb_info_t *sbi = minixfs_sb(inode->i_sb);
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
  raw_inode->i_time = inode->i_mtime;
  for (i = 0; i < 9; i++)
    raw_inode->i_zone[i] = inode->i_zone[i];
  
  /* release buffer */
  bh->b_dirt = 1;
  brelse(bh);
  
  return 0;
}

/*
 * Write a Minix V2/V3 inode on disk.
 */
static int minixfs_write_inode_v2(struct inode_t *inode)
{
  struct minix_sb_info_t *sbi = minixfs_sb(inode->i_sb);
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
  raw_inode->i_atime = inode->i_atime;
  raw_inode->i_mtime = inode->i_mtime;
  raw_inode->i_ctime = inode->i_ctime;
  for (i = 0; i < 10; i++)
    raw_inode->i_zone[i] = inode->i_zone[i];
  
  /* release buffer */
  bh->b_dirt = 1;
  brelse(bh);
  
  return 0;
}

/*
 * Write a Minix inode on disk.
 */
int minixfs_write_inode(struct inode_t *inode)
{
  struct minix_sb_info_t *sbi = minixfs_sb(inode->i_sb);
  int err;
  
  /* read inode on disk */
  if (sbi->s_version == MINIXFS_V1)
    err = minixfs_write_inode_v1(inode);
  else
    err = minixfs_write_inode_v2(inode);
  
  return err;
}

/*
 * Release a Minix inode.
 */
int minixfs_put_inode(struct inode_t *inode)
{
  if (!inode)
    return -EINVAL;
  
  if (!inode->i_nlinks) {
    inode->i_size = 0;
    minixfs_truncate(inode);
    minixfs_free_inode(inode);
  }
  
  return 0;
}

/*
 * Read a Minix inode block.
 */
static struct buffer_head_t *minixfs_inode_getblk(struct inode_t *inode, uint32_t inode_block, int create)
{
  /* create block if needed */
  if (create && !inode->i_zone[inode_block]) {
    inode->i_zone[inode_block] = minixfs_new_block(inode->i_sb);
    if (inode->i_zone[inode_block])
      inode->i_dirt = 1;
  }
  
  /* check block */
  if (!inode->i_zone[inode_block])
    return NULL;
  
  /* read block on disk */
  return sb_bread(inode->i_sb, inode->i_zone[inode_block]);
}

/*
 * Read a Minix indirect block.
 */
static struct buffer_head_t *minixfs_block_getblk(struct super_block_t *sb, struct buffer_head_t *bh,
                                                  uint32_t block_block, int create)
{
  int i;

  if (!bh)
    return NULL;
  
  /* create block if needed */
  i = ((uint32_t *) bh->b_data)[block_block];
  if (create && !i) {
    i = minixfs_new_block(sb);
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
struct buffer_head_t *minixfs_bread(struct inode_t *inode, uint32_t block, int create)
{
  struct super_block_t *sb;
  struct buffer_head_t *bh;
  int addresses_per_block;
  
  /* get super block */
  sb = inode->i_sb;

  /* check block number */
  if (block >= sb->s_max_size / sb->s_blocksize)
    return NULL;
  
  /* compute number of addresses per block */
  addresses_per_block = sb->s_blocksize / 4;
  
  /* direct block */
  if (block < 7)
    return minixfs_inode_getblk(inode, block, create);
  
  /* indirect block */
  block -= 7;
  if (block < addresses_per_block) {
    bh = minixfs_inode_getblk(inode, 7, create);
    return minixfs_block_getblk(sb, bh, block, create);
  }
  
  /* double indirect block */
  block -= addresses_per_block;
  if (block < addresses_per_block * addresses_per_block) {
    bh = minixfs_inode_getblk(inode, 8, create);
    bh = minixfs_block_getblk(sb, bh, (block / addresses_per_block) & (addresses_per_block - 1), create);
    return minixfs_block_getblk(sb, bh, block & (addresses_per_block - 1), create);
  }
  
  /* trip indirect block */
  block -= addresses_per_block * addresses_per_block;
  bh = minixfs_inode_getblk(inode, 9, create);
  bh = minixfs_block_getblk(sb, bh, (block >> (addresses_per_block * 2)) & (addresses_per_block - 1), create);
  bh = minixfs_block_getblk(sb, bh, (block / addresses_per_block) & (addresses_per_block - 1), create);
  return minixfs_block_getblk(sb, bh, block & (addresses_per_block - 1), create);
}
