#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>

#include "bfs.h"

/*
 * Get first free bit in a bitmap block.
 */
static inline int bfs_get_free_bitmap(char *bitmap, size_t bitmap_len)
{
  uint32_t *bits = (uint32_t *) bitmap;
  register int i, j;

  /* compute double words */
  for (i = 0; i < bitmap_len / 4; i++)
    if (bits[i] != 0xFFFFFFFF)
      for (j = 0; j < 32; j++)
        if (!(bits[i] & (0x1 << j)))
          return 32 * i + j;

  /* compute remaining bytes */
  for (i = bitmap_len / 4; i < bitmap_len; i++)
    if (!(bits[i] & (0x1 << j)))
      return i;

  return -1;
}

/*
 * Create a new Minix inode.
 */
struct inode_t *bfs_new_inode(struct super_block_t *sb)
{
  struct bfs_sb_info_t *sbi = bfs_sb(sb);
  struct inode_t *inode;
  int ino;

  /* get an empty inode */
  inode = vfs_get_empty_inode(sb);
  if (!inode)
    return NULL;

  /* get free ino in bitmap */
  ino = bfs_get_free_bitmap(sbi->s_imap, sbi->s_lasti);

  /* no free inode */
  if (ino == -1) {
    vfs_iput(inode);
    return NULL;
  }

  /* set inode */
  bfs_i(inode)->i_dsk_ino = ino;
  bfs_i(inode)->i_sblock = 0;
  bfs_i(inode)->i_eblock = 0;
  inode->i_ino = ino;
  inode->i_uid = getuid();
  inode->i_gid = getgid();
  inode->i_atime = inode->i_mtime = inode->i_ctime = current_time();
  inode->i_nlinks = 1;
  inode->i_ref = 1;

  /* set inode in bitmap */
  BITMAP_SET(sbi->s_imap, ino);

  /* decrease number of free inodes */
  sbi->s_freei--;

  return inode;
}
