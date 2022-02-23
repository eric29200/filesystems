#include <sys/stat.h>

#include "minixfs.h"

#define DIRECT_BLOCK(inode, block_size)               (((inode)->i_size + (block_size) - 1) / (block_size))
#define INDIRECT_BLOCK(inode, offset, block_size)     (DIRECT_BLOCK(inode, block_size) - offset)
#define DINDIRECT_BLOCK(inode, offset, block_size)    ((DIRECT_BLOCK(inode, block_size) - offset) / ((block_size) / 4))
#define TINDIRECT_BLOCK(inode, offset, block_size)    ((DIRECT_BLOCK(inode, block_size) - offset) / ((block_size) / 4))

/*
 * Free Minix direct blocks.
 */
static void minixfs_free_direct_blocks(struct inode_t *inode)
{
  int i;
  
  for (i = DIRECT_BLOCK(inode, inode->i_sb->s_blocksize); i < 7; i++) {
    if (inode->i_zone[i]) {
      minixfs_free_block(inode->i_sb, inode->i_zone[i]);
      inode->i_zone[i] = 0;
    }
  }
}

/*
 * Free Minix single indirect blocks.
 */
static void minixfs_free_indirect_blocks(struct inode_t *inode, int offset, uint32_t *block)
{
  struct buffer_head_t *bh;
  uint32_t *blocks;
  int i;

  if (!*block)
    return;
  
  /* get block */
  bh = sb_bread(inode->i_sb, *block);
  if (!bh)
    return;
  
  /* free all pointed blocks */
  blocks = (uint32_t * ) bh->b_data;
  for (i = INDIRECT_BLOCK(inode, offset, inode->i_sb->s_blocksize); i < inode->i_sb->s_blocksize / 4; i++)
    if (blocks[i])
      minixfs_free_block(inode->i_sb, blocks[i]);
  
  /* get first used address */
  for (i = 0; i < inode->i_sb->s_blocksize / 4; i++)
    if (blocks[i])
      break;
  
  /* indirect block not used anymore : free it */
  if (i >= inode->i_sb->s_blocksize / 4) {
    minixfs_free_block(inode->i_sb, *block);
    *block = 0;
  }
  
  /* release buffer */
  brelse(bh);
}

/*
 * Free Minix double indirect blocks.
 */
static void minixfs_free_dindirect_blocks(struct inode_t *inode, int offset, uint32_t *block)
{
  struct buffer_head_t *bh;
  uint32_t *blocks;
  int i;

  if (!*block)
    return;
  
  /* get block */
  bh = sb_bread(inode->i_sb, *block);
  if (!bh)
    return;
  
  /* free all pointed blocks */
  blocks = (uint32_t * ) bh->b_data;
  for (i = DINDIRECT_BLOCK(inode, offset, inode->i_sb->s_blocksize); i < inode->i_sb->s_blocksize / 4; i++)
    if (blocks[i])
      minixfs_free_indirect_blocks(inode, offset + (i / (inode->i_sb->s_blocksize / 4)), &blocks[i]);
  
  /* get first used address */
  for (i = 0; i < inode->i_sb->s_blocksize / 4; i++)
    if (blocks[i])
      break;
  
  /* indirect block not used anymore : free it */
  if (i >= inode->i_sb->s_blocksize / 4) {
    minixfs_free_block(inode->i_sb, *block);
    *block = 0;
  }
  
  /* release buffer */
  brelse(bh);
}

/*
 * Free Minix triple indirect blocks.
 */
static void minixfs_free_tindirect_blocks(struct inode_t *inode, int offset, uint32_t *block)
{
  struct buffer_head_t *bh;
  uint32_t *blocks;
  int i;

  if (!*block)
    return;
  
  /* get block */
  bh = sb_bread(inode->i_sb, *block);
  if (!bh)
    return;
  
  /* free all pointed blocks */
  blocks = (uint32_t * ) bh->b_data;
  for (i = TINDIRECT_BLOCK(inode, offset, inode->i_sb->s_blocksize); i < inode->i_sb->s_blocksize / 4; i++)
    if (blocks[i])
      minixfs_free_indirect_blocks(inode, offset + (i / (inode->i_sb->s_blocksize / 4)), &blocks[i]);
  
  /* get first used address */
  for (i = 0; i < inode->i_sb->s_blocksize / 4; i++)
    if (blocks[i])
      break;
  
  /* indirect block not used anymore : free it */
  if (i >= inode->i_sb->s_blocksize / 4) {
    minixfs_free_block(inode->i_sb, *block);
    *block = 0;
  }
  
  /* release buffer */
  brelse(bh);
}

/*
 * Truncate a Minix inode.
 */
void minixfs_truncate(struct inode_t *inode)
{
  int addr_per_block;
  
  /* only allowed on regular files and directories */
  if (!inode || !(S_ISREG(inode->i_mode) || S_ISDIR(inode->i_mode)))
    return;
  
  /* compute number of addressed per block */
  addr_per_block = inode->i_sb->s_blocksize / 4;
  
  /* free blocks */
  minixfs_free_direct_blocks(inode);
  minixfs_free_indirect_blocks(inode, 7, &inode->i_zone[7]);
  minixfs_free_dindirect_blocks(inode, 7 + addr_per_block, &inode->i_zone[8]);
  minixfs_free_tindirect_blocks(inode, 7 + addr_per_block + addr_per_block * addr_per_block, &inode->i_zone[9]);
  
  /* mark inode dirty */
  inode->i_dirt = 1;
}