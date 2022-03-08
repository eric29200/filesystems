#include <sys/stat.h>

#include "minix.h"

#define DIRECT_BLOCK(inode)               (((inode)->i_size + (inode)->i_sb->s_blocksize - 1) / (inode)->i_sb->s_blocksize)
#define INDIRECT_BLOCK(inode, offset)     (DIRECT_BLOCK((inode)) - offset)
#define DINDIRECT_BLOCK(inode, offset)    (INDIRECT_BLOCK(inode, offset) / addr_per_block)
#define TINDIRECT_BLOCK(inode, offset)    (INDIRECT_BLOCK(inode, offset) / (addr_per_block * addr_per_block))


/*
 * Free Minix direct blocks.
 */
static void minix_free_direct_blocks(struct inode_t *inode)
{
  struct minix_inode_info_t *minix_inode = minix_i(inode);
  int i;
  
  /* free direct blocks */
  for (i = DIRECT_BLOCK(inode); i < 7; i++) {
    if (i < 0)
      i = 0;

    /* free block */
    if (minix_inode->i_zone[i]) {
      minix_free_block(inode->i_sb, minix_inode->i_zone[i]);
      minix_inode->i_zone[i] = 0;
    }
  }
}

/*
 * Free Minix single indirect blocks.
 */
static void minix_free_indirect_blocks(struct inode_t *inode, int offset, uint32_t *block, int addr_per_block)
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
  for (i = INDIRECT_BLOCK(inode, offset); i < addr_per_block; i++) {
    if (i < 0)
      i = 0;

    /* free block */
    if (blocks[i])
      minix_free_block(inode->i_sb, blocks[i]);

    /* mark parent block dirty */
    blocks[i] = 0;
    bh->b_dirt = 1;
  }
  
  /* get first used address */
  for (i = 0; i < addr_per_block; i++)
    if (blocks[i])
      break;
  
  /* indirect block not used anymore : free it */
  if (i >= addr_per_block) {
    minix_free_block(inode->i_sb, *block);
    *block = 0;
  }
  
  /* release buffer */
  brelse(bh);
}

/*
 * Free Minix double indirect blocks.
 */
static void minix_free_dindirect_blocks(struct inode_t *inode, int offset, uint32_t *block, int addr_per_block)
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
  for (i = DINDIRECT_BLOCK(inode, offset); i < addr_per_block; i++) {
    if (i < 0)
      i = 0;

    /* free block */
    if (blocks[i])
      minix_free_indirect_blocks(inode, offset + (i / addr_per_block), &blocks[i], addr_per_block);

    /* mark parent block dirty */
    blocks[i] = 0;
    bh->b_dirt = 1;
  }
  
  /* get first used address */
  for (i = 0; i < addr_per_block; i++)
    if (blocks[i])
      break;
  
  /* indirect block not used anymore : free it */
  if (i >= addr_per_block) {
    minix_free_block(inode->i_sb, *block);
    *block = 0;
  }
  
  /* release buffer */
  brelse(bh);
}

/*
 * Free Minix triple indirect blocks.
 */
static void minix_free_tindirect_blocks(struct inode_t *inode, int offset, uint32_t *block, int addr_per_block)
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
  for (i = TINDIRECT_BLOCK(inode, offset); i < addr_per_block; i++) {
    if (i < 0)
      i = 0;

    /* free block */
    if (blocks[i])
      minix_free_dindirect_blocks(inode, offset + (i / addr_per_block), &blocks[i], addr_per_block);

    /* mark parent block dirty */
    blocks[i] = 0;
    bh->b_dirt = 1;
  }
  
  /* get first used address */
  for (i = 0; i < addr_per_block; i++)
    if (blocks[i])
      break;
  
  /* indirect block not used anymore : free it */
  if (i >= addr_per_block) {
    minix_free_block(inode->i_sb, *block);
    *block = 0;
  }
  
  /* release buffer */
  brelse(bh);
}

/*
 * Truncate a Minix inode.
 */
void minix_truncate(struct inode_t *inode)
{
  struct minix_inode_info_t *minix_inode = minix_i(inode);
  int addr_per_block;
  
  /* only allowed on regular files and directories */
  if (!inode || !(S_ISREG(inode->i_mode) || S_ISDIR(inode->i_mode)))
    return;
  
  /* compute number of addressed per block */
  if (minix_sb(inode->i_sb)->s_version == MINIX_V1)
    addr_per_block = inode->i_sb->s_blocksize / 2;
  else
    addr_per_block = inode->i_sb->s_blocksize / 4;
  
  /* free direct, indirect and double indirect blocks */
  minix_free_direct_blocks(inode);
  minix_free_indirect_blocks(inode, 7, &minix_inode->i_zone[7], addr_per_block);
  minix_free_dindirect_blocks(inode, 7 + addr_per_block, &minix_inode->i_zone[8], addr_per_block);

  /* if Minix 2/3, free triple indirect blocks */
  if (minix_sb(inode->i_sb)->s_version != MINIX_V1)
    minix_free_tindirect_blocks(inode, 7 + addr_per_block + addr_per_block * addr_per_block, &minix_inode->i_zone[9],
                                addr_per_block);
  
  /* mark inode dirty */
  inode->i_mtime = inode->i_ctime = current_time();
  inode->i_dirt = 1;
}
