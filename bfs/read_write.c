#include <string.h>
#include <errno.h>
#include <sys/fcntl.h>

#include "bfs.h"

/*
 * Read a BFS file.
 */
int bfs_file_read(struct file *filp, char *buf, int count)
{
	struct super_block *sb;
	struct buffer_head *bh;
	struct inode *inode;
	int pos, nb_chars, left;

	/* get inode */
	inode = filp->f_inode;
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
		bh = bfs_bread(inode, filp->f_pos / sb->s_blocksize, 0);
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

/*
 * Write a BFS file.
 */
int bfs_file_write(struct file *filp, const char *buf, int count)
{
	struct super_block *sb;
	struct buffer_head *bh;
	struct inode *inode;
	int pos, nb_chars, left;

	/* get inode */
	inode = filp->f_inode;
	sb = inode->i_sb;

	/* handle append flag */
	if (filp->f_flags & O_APPEND)
		filp->f_pos = inode->i_size;

	/* read block by block */
	for (left = count; left > 0;) {
		/* read block */
		bh = bfs_bread(inode, filp->f_pos / sb->s_blocksize, 1);
		if (!bh)
			goto out;

		/* find position and numbers of chars to read */
		pos = filp->f_pos % sb->s_blocksize;
		nb_chars = sb->s_blocksize - pos <= left ? sb->s_blocksize - pos : left;

		/* copy to buffer */
		memcpy(bh->b_data + pos, buf, nb_chars);

		/* release block */
		bh->b_dirt = 1;
		brelse(bh);

		/* update sizes */
		filp->f_pos += nb_chars;
		buf += nb_chars;
		left -= nb_chars;

		/* end of file : grow it and mark inode dirty */
		if (filp->f_pos > inode->i_size) {
			inode->i_size = filp->f_pos;
			inode->i_dirt = 1;
		}
	}

out:
	return count - left;
}

/*
 * Move BFS block.
 */
static int bfs_move_block(struct super_block *sb, uint32_t from, uint32_t to)
{
	struct buffer_head *bh_from, *bh_to;

	/* read source block */
	bh_from = sb_bread(sb, from);
	if (!bh_from)
		return -EIO;

	/* read destination block */
	bh_to = sb_bread(sb, to);
	if (!bh_to) {
		brelse(bh_from);
		return -EIO;
	}

	/* copy from buffer */
	memcpy(bh_to->b_data, bh_from->b_data, bh_from->b_size);
	bh_to->b_dirt = 1;

	/* reset from buffer */
	memset(bh_from->b_data, 0, bh_from->b_size);
	bh_from->b_dirt = 1;

	/* release block buffers */
	brelse(bh_from);
	brelse(bh_to);

	return 0;
}

/*
 * Move BFS blocks.
 */
static int bfs_move_blocks(struct super_block *sb, uint32_t start, uint32_t end, uint32_t to)
{
	uint32_t i;

	/* move block by block */
	for (i = start; i <= end; i++)
		if (bfs_move_block(sb, i, to + i))
			return -EIO;

	return 0;
}

/*
 * Read a BFS inode block.
 */
struct buffer_head *bfs_bread(struct inode *inode, uint32_t block, int create)
{
	struct bfs_inode_info *bfs_inode = bfs_i(inode);
	struct super_block *sb = inode->i_sb;
	struct bfs_sb_info *sbi = bfs_sb(sb);
	uint32_t phys, old_blocks;

	/* get physical address and compute old blocks */
	phys = bfs_inode->i_sblock + block;
	old_blocks = bfs_inode->i_eblock - bfs_inode->i_sblock + 1;

	/* asked block belongs to file */
	if (!create) {
		if (phys <= bfs_inode->i_eblock)
			return sb_bread(sb, phys);

		return NULL;
	}

	/* file is not empty and asked block belongs to file */
	if (bfs_inode->i_sblock && phys <= bfs_inode->i_eblock)
		return sb_bread(sb, phys);

	/* check if there is enough space on file system */
	if (phys >= sbi->s_blocks)
		return NULL;

	/* if the last data block for this file is the last on file system, just extend the file */
	if (bfs_inode->i_eblock == sbi->s_lf_eblk) {
		sbi->s_freeb -= phys - bfs_inode->i_eblock;
		bfs_inode->i_eblock = phys;
		sbi->s_lf_eblk = phys;
		inode->i_dirt = 1;
		return sb_bread(sb, phys);
	}

	/* move the file to the end of file system : check if there is enough space */
	phys = sbi->s_lf_eblk + 1;
	if (phys + block >= sbi->s_blocks)
		return NULL;

	/* move all file blocks */
	if (bfs_inode->i_sblock)
		if (bfs_move_blocks(sb, bfs_inode->i_sblock, bfs_inode->i_eblock, phys))
			return NULL;

	/* update inode */
	bfs_inode->i_sblock = phys;
	phys += block;
	bfs_inode->i_eblock = phys;
	inode->i_dirt = 1;

	/* update super block */
	sbi->s_lf_eblk = phys;
	sbi->s_freeb -= bfs_inode->i_eblock - bfs_inode->i_sblock + 1 - old_blocks;

	return sb_bread(sb, phys);
}
