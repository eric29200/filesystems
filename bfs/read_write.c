#include <string.h>
#include <sys/fcntl.h>

#include "bfs.h"

/*
 * Read a BFS file.
 */
int bfs_file_read(struct file_t *filp, char *buf, int count)
{
  struct bfs_inode_info_t *bfs_inode;
  struct super_block_t *sb;
  struct buffer_head_t *bh;
  struct inode_t *inode;
  int pos, nb_chars, left;

  /* get inode */
  inode = filp->f_inode;
  bfs_inode = bfs_i(inode);
  sb = inode->i_sb;

  /* adjust size */
  if (filp->f_pos + count > inode->i_size)
    count = inode->i_size - filp->f_pos;

  /* no more data to read */
  if (count <= 0)
    return 0;

  /* read block by block */
  for (left = count; left > 0;) {
    /* read block */
    bh = sb_bread(sb, bfs_inode->i_sblock + filp->f_pos / sb->s_blocksize);
    if (!bh)
      goto out;

    /* find position and numbers of chars to read */
    pos = filp->f_pos % sb->s_blocksize;
    nb_chars = sb->s_blocksize - pos <= left ? sb->s_blocksize - pos : left;

    /* copy to buffer */
    memcpy(buf, bh->b_data + pos, nb_chars);

    /* release block */
    brelse(bh);

    /* update sizes */
    filp->f_pos += nb_chars;
    buf += nb_chars;
    left -= nb_chars;
  }

out:
  return count - left;
}
