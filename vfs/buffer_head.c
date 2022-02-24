#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>

#include "vfs.h"

/* global buffers table */
static struct buffer_head_t buffer_table[VFS_NR_BUFFER];

/*
 * Get an empty buffer.
 */
static struct buffer_head_t *vfs_get_empty_buffer(uint16_t block_size)
{
  struct buffer_head_t *bh = NULL;
  int i;
  
  /* find a free buffer */
  for (i = 0; i < VFS_NR_BUFFER; i++) {
    if (!buffer_table[i].b_ref) {
      bh = &buffer_table[i];
      break;
    }
  }
  
  /* no free buffer */
  if (!bh)
    return NULL;

  /* reset buffer */
  memset(bh, 0, sizeof(struct buffer_head_t));
  
  /* allocate data */
  bh->b_data = (char *) malloc(block_size);
  if (!bh->b_data)
    return NULL;
  
  /* mark buffer used */
  bh->b_ref = 1;
  
  return bh;
}

/*
 * Read a block buffer.
 */
struct buffer_head_t *sb_bread(struct super_block_t *sb, uint32_t block)
{
  struct buffer_head_t *bh;
  int i;
  
  /* try to find in buffer table */
  for (i = 0; i < VFS_NR_BUFFER; i++) {
    if (buffer_table[i].b_block == block && buffer_table[i].b_sb == sb) {
      bh = &buffer_table[i];
      bh->b_ref++;
      return bh;
    }
  }
  
  /* get new empty buffer */
  bh = vfs_get_empty_buffer(sb->s_blocksize);
  if (!bh)
    return NULL;
  
  /* set block */
  bh->b_block = block;
  bh->b_sb = sb;
  
  /* seek to block */
  if (lseek(sb->s_fd, block * sb->s_blocksize, SEEK_SET) == -1)
    goto err;
  
  /* read block */
  if (read(sb->s_fd, bh->b_data, sb->s_blocksize) != sb->s_blocksize)
    goto err;
  
  return bh;
err:
  bh->b_ref = 0;
  free(bh->b_data);
  return NULL;
}

/*
 * Write a block buffer on disk.
 */
int bwrite(struct buffer_head_t *bh)
{
  if (!bh)
    return -EINVAL;
  
  /* seek to block */
  if (lseek(bh->b_sb->s_fd, bh->b_block * bh->b_sb->s_blocksize, SEEK_SET) == -1)
    return -EIO;
    
  /* write block */
  if (write(bh->b_sb->s_fd, bh->b_data, bh->b_sb->s_blocksize) != bh->b_sb->s_blocksize)
    return -EIO;
  
  /* mark buffer clear */
  bh->b_dirt = 0;
  
  return 0;
}

/*
 * Release a block buffer.
 */
void brelse(struct buffer_head_t *bh)
{
  if (!bh) 
    return;
  
  /* write it on disk if needed */
  if (bh->b_dirt)
    bwrite(bh);
  
  /* update reference count */
  bh->b_ref--;
  
  /* free buffer */
  if (!bh->b_ref) {
    free(bh->b_data);
    memset(bh, 0, sizeof(struct buffer_head_t));
  }
}
