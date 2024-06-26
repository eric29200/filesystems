#include <unistd.h>
#include <errno.h>
#include <sys/types.h>

#include "ext2.h"

/*
 * Read the inodes bitmap of a block group.
 */
static struct buffer_head *ext2_read_inode_bitmap(struct super_block *sb, uint32_t block_group)
{
	struct ext2_group_desc *gdp;

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
struct inode *ext2_new_inode(struct inode *dir, mode_t mode)
{
	struct ext2_sb_info *sbi = ext2_sb(dir->i_sb);
	struct buffer_head *gdp_bh, *bitmap_bh;
	struct ext2_group_desc *gdp;
	struct inode *inode;
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
	inode->i_mode = mode;
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
	if (S_ISDIR(inode->i_mode))
		gdp->bg_used_dirs_count = htole16(le16toh(gdp->bg_used_dirs_count) + 1);
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

/*
 * Free a Ext2 inode.
 */
int ext2_free_inode(struct inode *inode)
{
	struct buffer_head *bitmap_bh, *gdp_bh;
	struct ext2_group_desc *gdp;
	struct ext2_sb_info *sbi;
	uint32_t block_group, bit;

	/* check inode */
	if (!inode)
		return 0;

	/* check if inode is still referenced */
	if (inode->i_ref > 1) {
		fprintf(stderr, "Ext2 : trying to free inode %ld with ref=%d\n", inode->i_ino, inode->i_ref);
		return -EINVAL;
	}

	/* get super block */
	sbi = ext2_sb(inode->i_sb);

	/* check if inode is not reserved */
	if (inode->i_ino < sbi->s_first_ino || inode->i_ino > le32toh(sbi->s_es->s_inodes_count)) {
		fprintf(stderr, "Ext2 : trying to free inode reserved or non existent inode %ld\n", inode->i_ino);
		return -EINVAL;
	}

	/* get block group */
	block_group = (inode->i_ino - 1) / sbi->s_inodes_per_group;
	bit = (inode->i_ino - 1) % sbi->s_inodes_per_group;

	/* get inode bitmap */
	bitmap_bh = ext2_read_inode_bitmap(inode->i_sb, block_group);
	if (!bitmap_bh)
		return -EIO;

	/* clear inode in bitmap */
	EXT2_BITMAP_CLR(bitmap_bh, bit);
	bitmap_bh->b_dirt = 1;
	brelse(bitmap_bh);

	/* update group descriptor */
	gdp = ext2_get_group_desc(inode->i_sb, block_group, &gdp_bh);
	gdp->bg_free_inodes_count = htole16(le16toh(gdp->bg_free_inodes_count) + 1);
	if (S_ISDIR(inode->i_mode))
		gdp->bg_used_dirs_count = htole16(le16toh(gdp->bg_used_dirs_count) - 1);
	gdp_bh->b_dirt = 1;
	bwrite(gdp_bh);

	/* update super block */
	sbi->s_es->s_free_inodes_count = htole32(le32toh(sbi->s_es->s_free_inodes_count) + 1);
	sbi->s_sbh->b_dirt = 1;
	bwrite(sbi->s_sbh);

	return 0;
}
