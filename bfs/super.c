#include <stdlib.h>
#include <errno.h>
#include <endian.h>

#include "bfs.h"

/*
 * BFS super operations.
 */
struct super_operations_t bfs_sops = {
  .alloc_inode          = bfs_alloc_inode,
  .put_inode            = bfs_put_inode,
  .delete_inode         = bfs_delete_inode,
  .read_inode           = bfs_read_inode,
  .write_inode          = bfs_write_inode,
  .put_super            = bfs_put_super,
  .statfs               = bfs_statfs,
};

/*
 * Read a BFS super block.
 */
int bfs_read_super(struct super_block_t *sb)
{
  struct bfs_super_block_t *bfs_sb;
  int err = -ENOSPC, i, block, off;
  struct buffer_head_t *bh, *sbh;
  struct bfs_inode_t *bfs_inode;
  struct bfs_sb_info_t *sbi;
  size_t imap_len;

  /* allocate BFS super block */
  sb->s_fs_info = sbi = (struct bfs_sb_info_t *) malloc(sizeof(struct bfs_sb_info_t));
  if (!sbi)
    return -ENOMEM;

  /* set block size */
  sb->s_blocksize_bits = BFS_BLOCK_SIZE_BITS;
  sb->s_blocksize = BFS_BLOCK_SIZE;

  /* read first block = super block */
  sbh = sb_bread(sb, 0);
  if (!sbh) {
    err = -EIO;
    goto err_bad_sb;
  }

  /* check magic number */
  bfs_sb = (struct bfs_super_block_t *) sbh->b_data;
  if (le32toh(bfs_sb->s_magic) != BFS_MAGIC)
    goto err_bad_magic;

  /* set super block */
  sbi->s_blocks = (le32toh(bfs_sb->s_end) + 1) >> BFS_BLOCK_SIZE_BITS;
  sbi->s_freeb = (le32toh(bfs_sb->s_end) + 1 - le32toh(bfs_sb->s_start)) >> BFS_BLOCK_SIZE_BITS;
  sbi->s_freei = 0;
  sbi->s_lf_eblk = 0;
  sbi->s_lasti = (le32toh(bfs_sb->s_start) - BFS_BLOCK_SIZE) / sizeof(struct bfs_inode_t) + BFS_ROOT_INO - 1;
  sbi->s_imap = NULL;
  sb->s_magic = le32toh(bfs_sb->s_magic);
  sb->s_op = &bfs_sops;
  sb->s_root_inode = NULL;

  /* allocate inodes bitmap */
  imap_len = (sbi->s_lasti / 8) + 1;
  sbi->s_imap = (char *) malloc(imap_len);
  if (!sbi->s_imap) {
    err = -ENOMEM;
    goto err_no_map;
  }

  /* mark inodes before root */
  for (i = 0; i < BFS_ROOT_INO; i++)
    BITMAP_SET(sbi->s_imap, i);

  /* parse all inodes to set bitmap and to get number of free blocks */
  for (i = BFS_ROOT_INO, bh = NULL; i <= sbi->s_lasti; i++) {
    block = (i - BFS_ROOT_INO) / BFS_INODES_PER_BLOCK + 1;
    off = (i - BFS_ROOT_INO) % BFS_INODES_PER_BLOCK;

    /* read next block */
    if (!off) {
      brelse(bh);
      bh = sb_bread(sb, block);
    }

    if (!bh)
      continue;

    /* get inode */
    bfs_inode = (struct bfs_inode_t *) bh->b_data + off;

    /* free inode */
    if (!bfs_inode->i_ino) {
      sbi->s_freei++;
      continue;
    }

    /* set inode in bitmap */
    BITMAP_SET(sbi->s_imap, i);

    /* decrement number of free blocks */
    sbi->s_freeb -= BFS_FILE_BLOCKS(bfs_inode);

    /* update last file end block */
    if (bfs_inode->i_eblock > sbi->s_lf_eblk)
      sbi->s_lf_eblk = bfs_inode->i_eblock;
  }

  /* release last block buffer and super block buffer */
  brelse(bh);
  brelse(sbh);

  /* get root inode */
  sb->s_root_inode = vfs_iget(sb, BFS_ROOT_INO);
  if (!sb->s_root_inode)
    goto err_root_inode;

  return 0;
err_root_inode:
  fprintf(stderr, "BFS : can't get root inode\n");
  free(sbi->s_imap);
  goto err_release_sb;
err_no_map:
  fprintf(stderr, "BFS : can't allocate inodes bitmap\n");
  goto err_release_sb;
err_bad_magic:
  fprintf(stderr, "BFS : wrong magic number\n");
err_release_sb:
  brelse(sbh);
  goto err;
err_bad_sb:
  fprintf(stderr, "BFS : can't read super block\n");
err:
  free(sbi);
  return err;
}

/*
 * Release a BFS super block.
 */
void bfs_put_super(struct super_block_t *sb)
{
  struct bfs_sb_info_t *sbi = bfs_sb(sb);

  /* release root inode */
  vfs_iput(sb->s_root_inode);

  /* free inodes bitmap */
  free(sbi->s_imap);

  /* free in memory super block */
  free(sbi);
}

/*
 * Get BFS file system status.
 */
int bfs_statfs(struct super_block_t *sb, struct statfs *buf)
{
  struct bfs_sb_info_t *sbi = bfs_sb(sb);

  buf->f_type = sb->s_magic;
  buf->f_bsize = sb->s_blocksize;
  buf->f_blocks = sbi->s_blocks;
  buf->f_bfree = sbi->s_freeb;
  buf->f_bavail = sbi->s_freeb;
  buf->f_files = sbi->s_lasti + 1 - BFS_ROOT_INO;
  buf->f_ffree = sbi->s_freei;
  buf->f_namelen = BFS_NAME_LEN;

  return 0;
}
