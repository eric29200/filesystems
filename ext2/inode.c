#include <stdlib.h>
#include <errno.h>

#include "ext2.h"

/*
 * Ext2 file operations.
 */
struct file_operations_t ext2_file_fops = {
  .read               = ext2_file_read,
  .write              = ext2_file_write,
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
  .lookup             = ext2_lookup,
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
  block = le32toh(gdp->bg_inode_table) + (offset >> inode->i_sb->s_blocksize_bits);

  /* read inode table block buffer */
  bh = sb_bread(inode->i_sb, block);
  if (!bh)
    return -EIO;

  /* get inode */
  offset &= (inode->i_sb->s_blocksize - 1);
  raw_inode = (struct ext2_inode_t *) (bh->b_data + offset);

  /* set generic inode */
  inode->i_mode = le16toh(raw_inode->i_mode);
  inode->i_nlinks = le16toh(raw_inode->i_links_count);
  inode->i_uid = le16toh(raw_inode->i_uid) | (le16toh(raw_inode->i_uid_high) << 16);
  inode->i_gid = le16toh(raw_inode->i_gid) | (le16toh(raw_inode->i_gid_high) << 16);
  inode->i_size = le32toh(raw_inode->i_size);
  inode->i_blocks = le32toh(raw_inode->i_blocks);
  inode->i_atime.tv_sec = le32toh(raw_inode->i_atime);
  inode->i_atime.tv_nsec = 0;
  inode->i_mtime.tv_sec = le32toh(raw_inode->i_mtime);
  inode->i_mtime.tv_nsec = 0;
  inode->i_ctime.tv_sec = le32toh(raw_inode->i_ctime);
  inode->i_ctime.tv_nsec = 0;
  ext2_inode->i_flags = le32toh(raw_inode->i_flags);
  ext2_inode->i_faddr = le32toh(raw_inode->i_faddr);
  ext2_inode->i_frag_no = raw_inode->i_frag;
  ext2_inode->i_frag_size = raw_inode->i_fsize;
  ext2_inode->i_file_acl = le32toh(raw_inode->i_file_acl);
  ext2_inode->i_dir_acl = le32toh(raw_inode->i_dir_acl);
  ext2_inode->i_dtime = le32toh(raw_inode->i_dtime);
  ext2_inode->i_generation = le32toh(raw_inode->i_generation);
  ext2_inode->i_block_group = block_group;
  for (i = 0; i < EXT2_N_BLOCKS; i++)
    ext2_inode->i_data[i] = raw_inode->i_block[i];

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
 * Write a Ext2 inode.
 */
int ext2_write_inode(struct inode_t *inode)
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
  block = le32toh(gdp->bg_inode_table) + (offset >> inode->i_sb->s_blocksize_bits);

  /* read inode table block buffer */
  bh = sb_bread(inode->i_sb, block);
  if (!bh)
    return -EIO;

  /* get inode */
  offset &= (inode->i_sb->s_blocksize - 1);
  raw_inode = (struct ext2_inode_t *) (bh->b_data + offset);

  /* set raw inode */

  /* set generic inode */
  raw_inode->i_mode = htole16(inode->i_mode);
  inode->i_uid = htole16(inode->i_uid & 0xFFFF);
  raw_inode->i_size = htole32(inode->i_size);
  raw_inode->i_atime = htole32(inode->i_atime.tv_sec);
  raw_inode->i_ctime = htole32(inode->i_ctime.tv_sec);
  raw_inode->i_mtime = htole32(inode->i_mtime.tv_sec);
  raw_inode->i_dtime = htole32(ext2_inode->i_dtime);
  raw_inode->i_gid = htole16(inode->i_gid & 0xFFFF);
  raw_inode->i_links_count = htole16(inode->i_nlinks);
  raw_inode->i_blocks = htole32(inode->i_blocks);
  raw_inode->i_flags = htole32(ext2_inode->i_flags);
  raw_inode->i_generation = htole32(ext2_inode->i_generation);
  raw_inode->i_file_acl = htole32(ext2_inode->i_file_acl);
  raw_inode->i_dir_acl = htole32(ext2_inode->i_dir_acl);
  raw_inode->i_faddr = htole32(ext2_inode->i_faddr);
  raw_inode->i_frag = ext2_inode->i_frag_no;
  raw_inode->i_fsize = htole32(ext2_inode->i_frag_size);
  raw_inode->i_uid_high = htole16((inode->i_uid & 0xFFFF0000) >> 16);
  raw_inode->i_gid_high = htole16((inode->i_gid & 0xFFFF0000) >> 16);
  for (i = 0; i < EXT2_N_BLOCKS; i++)
    raw_inode->i_block[i] = ext2_inode->i_data[i];

  /* release block buffer */
  bh->b_dirt = 1;
  brelse(bh);

  return 0;
}

/*
 * Read a Ext2 inode block.
 */
static struct buffer_head_t *ext2_inode_getblk(struct inode_t *inode, int inode_block, int create)
{
  struct ext2_inode_info_t *ext2_inode = ext2_i(inode);
  struct ext2_sb_info_t *sbi = ext2_sb(inode->i_sb);
  uint32_t goal = 0;
  int i;

  /* create block if needed */
  if (create && !ext2_inode->i_data[inode_block]) {
    /* try to reuse last block of inode */
    for (i = inode_block - 1; i >= 0; i--) {
      if (ext2_inode->i_data[i]) {
        goal = ext2_inode->i_data[i];
        break;
      }
    }

    /* else use block of this inode */
    if (!goal)
      goal = ext2_inode->i_block_group * sbi->s_blocks_per_group + le32toh(sbi->s_es->s_first_data_block);

    /* create new block */
    ext2_inode->i_data[inode_block] = ext2_new_block(inode, goal);
    if (ext2_inode->i_data[inode_block]) {
      inode->i_blocks++;
      inode->i_dirt = 1;
    }
  }

  /* check block */
  if (!ext2_inode->i_data[inode_block])
    return NULL;

  /* read block on disk */
  return sb_bread(inode->i_sb, ext2_inode->i_data[inode_block]);
}

/*
 * Read a Ext2 indirect block.
 */
static struct buffer_head_t *ext2_block_getblk(struct inode_t *inode, struct buffer_head_t *bh, int block_block,
                                               int create)
{
  uint32_t goal = 0;
  int i, tmp;

  if (!bh)
    return NULL;

  /* create block if needed */
  i = ((uint32_t *) bh->b_data)[block_block];
  if (create && !i) {
    /* try to reuse previous blocks */
    for (tmp = block_block - 1; tmp >= 0; tmp--) {
      if (((uint32_t *) bh->b_data)[tmp]) {
        goal = ((uint32_t *) bh->b_data)[tmp];
      }
    }

    /* else use this indirect block */
    if (!goal)
      goal = bh->b_block;

    /* create new block */
    i = ext2_new_block(inode, goal);
    if (i) {
      ((uint32_t *) bh->b_data)[block_block] = i;
      bh->b_dirt = 1;
    }
  }

  /* release parent block */
  brelse(bh);

  if (!i)
    return NULL;

  return sb_bread(inode->i_sb, i);
}

/*
 * Read a Ext2 inode block.
 */
struct buffer_head_t *ext2_bread(struct inode_t *inode, uint32_t block, int create)
{
  struct super_block_t *sb = inode->i_sb;
  struct buffer_head_t *bh;
  int addr_per_block;

  /* compute number of addresses per block */
  addr_per_block = sb->s_blocksize / 4;

  /* check block number */
  if (block > EXT2_NDIR_BLOCKS + addr_per_block
      + addr_per_block * addr_per_block
      + addr_per_block * addr_per_block * addr_per_block)
    return NULL;

  /* direct block */
  if (block < EXT2_NDIR_BLOCKS)
    return ext2_inode_getblk(inode, block, create);

  /* indirect block */
  block -= EXT2_NDIR_BLOCKS;
  if (block < addr_per_block) {
    bh = ext2_inode_getblk(inode, EXT2_IND_BLOCK, create);
    return ext2_block_getblk(inode, bh, block, create);
  }

  /* double indirect block */
  block -= addr_per_block;
  if (block < addr_per_block * addr_per_block) {
    bh = ext2_inode_getblk(inode, EXT2_DIND_BLOCK, create);
    bh = ext2_block_getblk(inode, bh, block / addr_per_block, create);
    return ext2_block_getblk(inode, bh, block & (addr_per_block - 1), create);
  }

  /* triple indirect block */
  block -= addr_per_block * addr_per_block;
  bh = ext2_inode_getblk(inode, EXT2_TIND_BLOCK, create);
  bh = ext2_block_getblk(inode, bh, block / (addr_per_block * addr_per_block), create);
  bh = ext2_block_getblk(inode, bh, (block / addr_per_block) & (addr_per_block - 1), create);
  return ext2_block_getblk(inode, bh, block & (addr_per_block - 1), create);
}
