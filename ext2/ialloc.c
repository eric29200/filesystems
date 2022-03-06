#include <unistd.h>
#include <error.h>
#include <sys/types.h>

#include "ext2.h"

/*
 * Read the inodes bitmap of a block group.
 */
static struct buffer_head_t *ext2_read_inode_bitmap(struct super_block_t *sb, uint32_t block_group)
{
  struct ext2_group_desc_t *gdp;

  /* get group descriptor */
  gdp = ext2_get_group_desc(sb, block_group, NULL);
  if (!gdp)
    return NULL;

  /* load inodes bitmap */
  return sb_bread(sb, le32toh(gdp->bg_inode_bitmap));
}

/*
 * Create a new Ext2 inode.
 */
struct inode_t *ext2_new_inode(struct inode_t *dir)
{
  struct ext2_sb_info_t *sbi = ext2_sb(dir->i_sb);
  struct buffer_head_t *gdp_bh, *bitmap_bh;
  struct ext2_group_desc_t *gdp;
  struct inode_t *inode;
  uint32_t group_no;
  int bgi, i;

  /* get an empty inode */
  inode = vfs_get_empty_inode(dir->i_sb);
  if (!inode)
    return NULL;

  /* try to find a group with free inodes (start with directory block group) */
  group_no = ext2_i(dir)->i_block_group;
  for (bgi = 0; bgi < sbi->s_groups_count; bgi++, group_no++) {
    /* rewind to first group if needed */
    if (group_no >= sbi->s_groups_count)
      group_no = 0;

    /* get group descriptor */
    gdp = ext2_get_group_desc(inode->i_sb, group_no, &gdp_bh);
    if (!gdp) {
      vfs_iput(inode);
      return NULL;
    }

    /* no free inodes in this group */
    if (!le16toh(gdp->bg_free_inodes_count))
      continue;

    /* get group inodes bitmap */
    bitmap_bh = ext2_read_inode_bitmap(inode->i_sb, group_no);
    if (!bitmap_bh) {
      vfs_iput(inode);
      return NULL;
    }

    /* get first free inode in bitmap */
    i = ext2_get_free_bitmap(inode->i_sb, bitmap_bh);
    if (i != -1 && i < sbi->s_inodes_per_group)
      goto allocated;

    /* release bitmap block */
    brelse(bitmap_bh);
  }

  /* release inode */
  vfs_iput(inode);
  return NULL;
allocated:
  /* set inode number */
  inode->i_ino = group_no * sbi->s_inodes_per_group + i + 1;
  if (inode->i_ino < sbi->s_first_ino || inode->i_ino > le32toh(sbi->s_es->s_inodes_count)) {
    brelse(bitmap_bh);
    vfs_iput(inode);
    return NULL;
  }

  /* set inode */
  inode->i_uid = getuid();
  inode->i_gid = getgid();
  inode->i_atime = inode->i_mtime = inode->i_ctime = current_time();
  inode->i_size = 0;
  inode->i_blocks = 0;
  inode->i_nlinks = 1;
  inode->i_op = NULL;
  inode->i_ref = 1;
  inode->i_dirt = 1;
  ext2_i(inode)->i_block_group = group_no;
  ext2_i(inode)->i_flags = ext2_i(dir)->i_flags;
  ext2_i(inode)->i_faddr = 0;
  ext2_i(inode)->i_frag_no = 0;
  ext2_i(inode)->i_frag_size = 0;
  ext2_i(inode)->i_file_acl = 0;
  ext2_i(inode)->i_dir_acl = 0;
  ext2_i(inode)->i_dtime = 0;
  ext2_i(inode)->i_generation = ext2_i(dir)->i_generation;

  /* set block in bitmap */
  EXT2_BITMAP_SET(bitmap_bh, i);

  /* release inodes bitmap */
  bitmap_bh->b_dirt = 1;
  brelse(bitmap_bh);

  /* update group descriptor */
  gdp->bg_free_inodes_count = htole16(le16toh(gdp->bg_free_inodes_count) - 1);
  gdp_bh->b_dirt = 1;
  bwrite(gdp_bh);

  /* update super block */
  sbi->s_es->s_free_inodes_count = htole32(le32toh(sbi->s_es->s_free_inodes_count) - 1);
  sbi->s_sbh->b_dirt = 1;
  bwrite(sbi->s_sbh);

  /* mark inode dirty */
  inode->i_dirt = 1;

  return inode;
}
