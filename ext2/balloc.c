#include "ext2.h"

/*
 * Get a group descriptor.
 */
struct ext2_group_desc_t *ext2_get_group_desc(struct super_block_t *sb, uint32_t block_group, struct buffer_head_t **bh)
{
  struct ext2_sb_info_t *sbi = ext2_sb(sb);
  struct ext2_group_desc_t *desc;
  uint32_t group_desc, offset;

  /* check block group */
  if (block_group >= sbi->s_groups_count)
    return NULL;

  /* compute group descriptor block */
  group_desc = block_group / sbi->s_desc_per_block;
  offset = block_group % sbi->s_desc_per_block;
  if (!sbi->s_group_desc[group_desc])
    return NULL;

  /* group block buffer of group descriptor */
  desc = (struct ext2_group_desc_t *) sbi->s_group_desc[group_desc]->b_data;
  if (bh)
    *bh = sbi->s_group_desc[group_desc];

  return desc + offset;
}
