#include <string.h>
#include <errno.h>
#include <time.h>

#include "minixfs.h"

#define MINIXFS_BITMAP_SET(bh, i)       ((bh)->b_data[(i) / 8] |= (0x1 << ((i) % 8)))
#define MINIXFS_BITMAP_CLR(bh, i)       ((bh)->b_data[(i) / 8] &= ~(0x1 << ((i) % 8)))

/*
 * Count number of free bits in a bitmap.
 */
static uint32_t minixfs_count_free_bitmap(struct super_block_t *sb, struct buffer_head_t **maps, int nb_maps)
{
  uint32_t *bits, res = 0;
  register int i, j, k;

  for (i = 0; i < nb_maps; i++) {
    bits = (uint32_t *) maps[i]->b_data;

    for (j = 0; j < sb->s_blocksize / 4; j++)
      if (bits[j] != 0xFFFFFFFF)
        for (k = 0; k < 32; k++)
          if (!(bits[j] & (0x1 << k)))
            res++;
  }

  return res;
}

/*
 * Get first free bit in a bitmap block.
 */
static inline int minixfs_get_free_bitmap(struct super_block_t *sb, struct buffer_head_t *bh)
{
  uint32_t *bits = (uint32_t *) bh->b_data;
  register int i, j;
  
  for (i = 0; i < sb->s_blocksize / 4; i++)
    if (bits[i] != 0xFFFFFFFF)
      for (j = 0; j < 32; j++)
        if (!(bits[i] & (0x1 << j)))
          return 32 * i + j;
  
  return -1;
}

/*
 * Create a new Minix inode.
 */
struct inode_t *minixfs_new_inode(struct super_block_t *sb)
{
  struct minix_sb_info_t *sbi = minixfs_sb(sb);
  struct inode_t *inode;
  int i, j;
  
  /* get an empty inode */
  inode = vfs_get_empty_inode(sb);
  if (!inode)
    return NULL;
  
  /* get free ino in bitmap */
  for (i = 0; i < sbi->s_imap_blocks; i++) {
    j = minixfs_get_free_bitmap(sb, sbi->s_imap[i]);
    if (j != -1)
      break;
  }
  
  /* no free inode */
  if (j == -1) {
    vfs_iput(inode);
    return NULL;
  }
  
  /* set inode */
  inode->i_ino = i * sb->s_blocksize * 8 + j;
  inode->i_atime = inode->i_mtime = inode->i_ctime = time(NULL);
  inode->i_nlinks = 1;
  inode->i_ref = 1;
  
  /* set inode in bitmap */
  MINIXFS_BITMAP_SET(sbi->s_imap[i], j);
  
  /* update bitmap on disk */
  bwrite(sbi->s_imap[i]);
  
  return inode;
}

/*
 * Create a new Minix block (return block number or 0 on failure).
 */
uint32_t minixfs_new_block(struct super_block_t *sb)
{
  struct minix_sb_info_t *sbi = minixfs_sb(sb);
  struct buffer_head_t *bh;
  uint32_t block_nr, i;
  int j;
  
  /* get free block in bitmap */
  for (i = 0; i < sbi->s_zmap_blocks; i++) {
    j = minixfs_get_free_bitmap(sb, sbi->s_zmap[i]);
    if (j != -1)
      break;
  }
  
  /* no free block */
  if (j == -1)
    return 0;
  
  /* compute real block number */
  block_nr = j + i * sb->s_blocksize * 8 + sbi->s_firstdatazone - 1;
  if (block_nr >= sbi->s_nzones)
    return 0;
  
  /* read block on disk */
  bh = sb_bread(sb, block_nr);
  if (!bh)
    return 0;
  
  /* memzero buffer and release it */
  memset(bh->b_data, 0, sb->s_blocksize);
  bh->b_dirt = 1;
  brelse(bh);
  
  /* set block in bitmap */
  MINIXFS_BITMAP_SET(sbi->s_zmap[i], j);
  
  /* update bitmap on disk */
  bwrite(sbi->s_zmap[i]);
  
  return block_nr;
}

/*
 * Free a Minix inode.
 */
int minixfs_free_inode(struct inode_t *inode)
{
  struct minix_sb_info_t *sbi;
  struct buffer_head_t *bh;
  
  if (!inode)
    return 0;
  
  /* get minix super block */
  sbi = minixfs_sb(inode->i_sb);
  
  /* check if inode is still referenced */
  if (inode->i_ref > 1) {
    fprintf(stderr, "MinixFS: trying to free inode %ld with ref=%d\n", inode->i_ino, inode->i_ref);
    return -EINVAL;
  }
  
  /* clear inode in bitmap */
  bh = sbi->s_imap[inode->i_ino / (inode->i_sb->s_blocksize * 8)];
  MINIXFS_BITMAP_CLR(bh, inode->i_ino & (inode->i_sb->s_blocksize * 8 - 1));
  
  /* update bitmap on disk */
  bwrite(bh);
  
  return 0;
}

/*
 * Free a Minix block.
 */
int minixfs_free_block(struct super_block_t *sb, uint32_t block)
{
  struct minix_sb_info_t *sbi = minixfs_sb(sb);
  struct buffer_head_t *bh;
  
  /* check block number */
  if (block < sbi->s_firstdatazone || block >= sbi->s_nzones)
    return -EINVAL;
  
  /* get block buffer */
  bh = sb_bread(sb, block);
  if (!bh)
    goto clear_bitmap;
  
  /* clear buffer */
  memset(bh->b_data, 0, sb->s_blocksize);
  bh->b_dirt = 1;
  brelse(bh);
  
clear_bitmap:
  /* clear block in bitmap */
  block -= sbi->s_firstdatazone - 1;
  bh = sbi->s_zmap[block / (sb->s_blocksize * 8)];
  MINIXFS_BITMAP_CLR(bh, block & (sb->s_blocksize * 8 - 1));
  
  /* update bitmap on disk */
  bwrite(bh);
  
  return 0;
}

/*
 * Get number of free inodes.
 */
uint32_t minixfs_count_free_inodes(struct super_block_t *sb)
{
  struct minix_sb_info_t *sbi = minixfs_sb(sb);
  return minixfs_count_free_bitmap(sb, sbi->s_imap, sbi->s_imap_blocks);
}

/*
 * Get number of free blocks.
 */
uint32_t minixfs_count_free_blocks(struct super_block_t *sb)
{
  struct minix_sb_info_t *sbi = minixfs_sb(sb);
  return minixfs_count_free_bitmap(sb, sbi->s_zmap, sbi->s_zmap_blocks);
}
