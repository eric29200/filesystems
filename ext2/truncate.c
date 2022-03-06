#include <sys/stat.h>

#include "ext2.h"

#define DIRECT_BLOCK(inode, block_size)               (((inode)->i_size + (block_size) - 1) / (block_size))
#define INDIRECT_BLOCK(inode, offset, block_size)     (DIRECT_BLOCK(inode, block_size) - offset)
#define DINDIRECT_BLOCK(inode, offset, block_size)    ((DIRECT_BLOCK(inode, block_size) - offset) / ((block_size) / 4))
#define TINDIRECT_BLOCK(inode, offset, block_size)    ((DIRECT_BLOCK(inode, block_size) - offset) / ((block_size) / 4))

/*
 * Free Ext2 direct blocks.
 */
static void ext2_free_direct_blocks(struct inode_t *inode)
{
  struct ext2_inode_info_t *ext2_inode = ext2_i(inode);
  int i;

  for (i = DIRECT_BLOCK(inode, inode->i_sb->s_blocksize); i < EXT2_NDIR_BLOCKS; i++) {
    if (ext2_inode->i_data[i]) {
      ext2_free_block(inode, ext2_inode->i_data[i]);
      ext2_inode->i_data[i] = 0;
    }
  }
}

/*
 * Free Ext2 single indirect blocks.
 */
static void ext2_free_indirect_blocks(struct inode_t *inode, int offset, uint32_t *block)
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
      ext2_free_block(inode, blocks[i]);

  /* get first used address */
  for (i = 0; i < inode->i_sb->s_blocksize / 4; i++)
    if (blocks[i])
      break;

  /* indirect block not used anymore : free it */
  if (i >= inode->i_sb->s_blocksize / 4) {
    ext2_free_block(inode, *block);
    *block = 0;
  }

  /* release buffer */
  brelse(bh);
}

/*
 * Free Ext2 double indirect blocks.
 */
static void ext2_free_dindirect_blocks(struct inode_t *inode, int offset, uint32_t *block)
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
      ext2_free_indirect_blocks(inode, offset + (i / (inode->i_sb->s_blocksize / 4)), &blocks[i]);

  /* get first used address */
  for (i = 0; i < inode->i_sb->s_blocksize / 4; i++)
    if (blocks[i])
      break;

  /* indirect block not used anymore : free it */
  if (i >= inode->i_sb->s_blocksize / 4) {
    ext2_free_block(inode, *block);
    *block = 0;
  }

  /* release buffer */
  brelse(bh);
}

/*
 * Free Ext2 triple indirect blocks.
 */
static void ext2_free_tindirect_blocks(struct inode_t *inode, int offset, uint32_t *block)
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
      ext2_free_dindirect_blocks(inode, offset + (i / (inode->i_sb->s_blocksize / 4)), &blocks[i]);

  /* get first used address */
  for (i = 0; i < inode->i_sb->s_blocksize / 4; i++)
    if (blocks[i])
      break;

  /* indirect block not used anymore : free it */
  if (i >= inode->i_sb->s_blocksize / 4) {
    ext2_free_block(inode, *block);
    *block = 0;
  }

  /* release buffer */
  brelse(bh);
}

/*
 * Truncate a Ext2 inode.
 */
void ext2_truncate(struct inode_t *inode)
{
  struct ext2_inode_info_t *ext2_inode = ext2_i(inode);
  int addr_per_block;

  /* only allowed on regular files and directories */
  if (!inode || !(S_ISREG(inode->i_mode) || S_ISDIR(inode->i_mode)))
    return;

  /* compute number of addressed per block */
  addr_per_block = inode->i_sb->s_blocksize / 4;

  /* free blocks */
  ext2_free_direct_blocks(inode);
  ext2_free_indirect_blocks(inode, EXT2_IND_BLOCK, &ext2_inode->i_data[EXT2_IND_BLOCK]);
  ext2_free_dindirect_blocks(inode, EXT2_NDIR_BLOCKS + addr_per_block, &ext2_inode->i_data[EXT2_DIND_BLOCK]);
  ext2_free_tindirect_blocks(inode, EXT2_NDIR_BLOCKS + addr_per_block + addr_per_block * addr_per_block,
                             &ext2_inode->i_data[EXT2_TIND_BLOCK]);

  /* mark inode dirty */
  inode->i_dirt = 1;
}
