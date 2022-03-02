#include <stdlib.h>
#include <errno.h>
#include <byteswap.h>

#include "bfs.h"

/*
 * BFS file operations.
 */
struct file_operations_t bfs_file_fops = {
  .read               = bfs_file_read,
  .write              = bfs_file_write,
};

/*
 * BFS directory operations.
 */
struct file_operations_t bfs_dir_fops = {
  .getdents64         = bfs_getdents64,
};

/*
 * BFS file inode operations.
 */
struct inode_operations_t bfs_file_iops = {
  .fops               = &bfs_file_fops,
  .follow_link        = NULL,
  .readlink           = NULL,
  .truncate           = NULL,
};

/*
 * BFS directory inode operations.
 */
struct inode_operations_t bfs_dir_iops = {
  .fops               = &bfs_dir_fops,
  .lookup             = bfs_lookup,
  .create             = bfs_create,
  .link               = bfs_link,
  .unlink             = bfs_unlink,
  .symlink            = NULL,
  .mkdir              = NULL,
  .rmdir              = NULL,
  .rename             = bfs_rename,
  .truncate           = NULL,
};

/*
 * Allocate a BFS inode.
 */
struct inode_t *bfs_alloc_inode(struct super_block_t *sb)
{
  struct bfs_inode_info_t *bfs_inode;

  /* allocate BFS inode */
  bfs_inode = (struct bfs_inode_info_t *) malloc(sizeof(struct bfs_inode_info_t));
  if (!bfs_inode)
    return NULL;

  return &bfs_inode->vfs_inode;
}

/*
 * Delete a BFS inode.
 */
void bfs_delete_inode(struct inode_t *inode)
{
  if (!inode)
    return;

  /* truncate and free inode */
  if (!inode->i_nlinks) {
    inode->i_size = 0;
    bfs_truncate(inode);
    bfs_free_inode(inode);
  }
}

/*
 * Read a BFS inode.
 */
int bfs_read_inode(struct inode_t *inode)
{
  struct bfs_sb_info_t *sbi = bfs_sb(inode->i_sb);
  struct bfs_inode_t *bfs_inode;
  struct buffer_head_t *bh;
  uint32_t block, off;

  /* check inode number */
  if (inode->i_ino < BFS_ROOT_INO || inode->i_ino > sbi->s_lasti) {
    fprintf(stderr, "BFS : bad inode number %ld\n", inode->i_ino);
    return -EINVAL;
  }

  /* compute inode block */
  block = (inode->i_ino - BFS_ROOT_INO) / BFS_INODES_PER_BLOCK + 1;

  /* read inode block buffer */
  bh = sb_bread(inode->i_sb, block);
  if (!bh)
    return -EIO;

  /* get bfs inode */
  off = (inode->i_ino - BFS_ROOT_INO) % BFS_INODES_PER_BLOCK;
  bfs_inode = (struct bfs_inode_t *) bh->b_data + off;

  /* set inode */
  bfs_i(inode)->i_sblock = le32toh(bfs_inode->i_sblock);
  bfs_i(inode)->i_eblock = le32toh(bfs_inode->i_eblock);
  bfs_i(inode)->i_dsk_ino = le16toh(bfs_inode->i_ino);
  inode->i_mode = 0x0000FFFF & le32toh(bfs_inode->i_mode);
  inode->i_nlinks = le32toh(bfs_inode->i_nlink);
  inode->i_uid = le32toh(bfs_inode->i_uid);
  inode->i_gid = le32toh(bfs_inode->i_gid);
  inode->i_size = BFS_FILE_SIZE(bfs_inode);
  inode->i_atime.tv_sec = le32toh(bfs_inode->i_atime);
  inode->i_atime.tv_nsec = 0;
  inode->i_mtime.tv_sec = le32toh(bfs_inode->i_mtime);
  inode->i_mtime.tv_nsec = 0;
  inode->i_ctime.tv_sec = le32toh(bfs_inode->i_ctime);
  inode->i_ctime.tv_nsec = 0;

  /* set operations */
  if (le32toh(bfs_inode->i_vtype) == BFS_VDIR) {
    inode->i_mode |= S_IFDIR;
    inode->i_op = &bfs_dir_iops;
  } else if (le32toh(bfs_inode->i_vtype) == BFS_VREG) {
    inode->i_mode |= S_IFREG;
    inode->i_op = &bfs_file_iops;
  }

  /* release block buffer */
  brelse(bh);

  return 0;
}

/*
 * Write a BFS inode.
 */
int bfs_write_inode(struct inode_t *inode)
{
  struct bfs_inode_info_t *bfs_inode = bfs_i(inode);
  struct bfs_sb_info_t *sbi = bfs_sb(inode->i_sb);
  struct bfs_inode_t *raw_inode;
  struct buffer_head_t *bh;
  uint32_t block, off;

  /* check inode */
  if (!inode)
    return 0;
  if (inode->i_ino < BFS_ROOT_INO || inode->i_ino > sbi->s_lasti) {
    fprintf(stderr, "BFS : bad inode number %ld\n", inode->i_ino);
    return -EINVAL;
  }

  /* compute inode block */
  block = (inode->i_ino - BFS_ROOT_INO) / BFS_INODES_PER_BLOCK + 1;

  /* read inode block */
  bh = sb_bread(inode->i_sb, block);
  if (!bh)
    return -EIO;

  /* read bfs raw inode */
  off = (inode->i_ino - BFS_ROOT_INO) % BFS_INODES_PER_BLOCK;
  raw_inode = (struct bfs_inode_t *) bh->b_data + off;

  /* set on disk inode */
  if (inode->i_ino == BFS_ROOT_INO)
    raw_inode->i_vtype = BFS_VDIR;
  else
    raw_inode->i_vtype = BFS_VREG;

  /* set on disk inode */
  raw_inode->i_ino = inode->i_ino;
  raw_inode->i_mode = inode->i_mode;
  raw_inode->i_uid = inode->i_uid;
  raw_inode->i_gid = inode->i_gid;
  raw_inode->i_nlink = inode->i_nlinks;
  raw_inode->i_atime = inode->i_atime.tv_sec;
  raw_inode->i_mtime = inode->i_mtime.tv_sec;
  raw_inode->i_ctime = inode->i_ctime.tv_sec;
  raw_inode->i_sblock = bfs_inode->i_sblock;
  raw_inode->i_eblock = bfs_inode->i_eblock;
  raw_inode->i_eoffset = bfs_inode->i_sblock * BFS_BLOCK_SIZE + inode->i_size - 1;

  /* release buffer */
  bh->b_dirt = 1;
  brelse(bh);

  return 0;
}

/*
 * Release a BFS inode.
 */
void bfs_put_inode(struct inode_t *inode)
{
  if (!inode)
    return;

  /* free inode */
  free(bfs_i(inode));
}
