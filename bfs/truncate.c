#include <string.h>
#include <errno.h>

#include "bfs.h"

/*
 * Truncate a BFS inode.
 */
void bfs_truncate(struct inode_t *inode)
{
  struct bfs_inode_info_t *bfs_inode = bfs_i(inode);
  struct super_block_t *sb = inode->i_sb;
  struct bfs_sb_info_t *sbi = bfs_sb(sb);
  struct buffer_head_t *bh;
  int i;

  /* do not truncate if inode is not the last file */
  if (sbi->s_lf_eblk != bfs_inode->i_eblock)
    return;

  /* memzero all file blocks */
  for (i = bfs_inode->i_sblock; i <= bfs_inode->i_eblock; i++) {
    /* get block buffer */
    bh = sb_bread(sb, i);
    if (!bh)
      return;

    /* memzero block buffer */
    memset(bh->b_data, 0, bh->b_size);

    /* release block buffer */
    bh->b_dirt = 1;
    brelse(bh);
  }

  /* update super block */
  sbi->s_lf_eblk = bfs_inode->i_sblock - 1;

  /* mark inode dirty */
  inode->i_mtime = inode->i_ctime = current_time();
  inode->i_dirt = 1;
}
