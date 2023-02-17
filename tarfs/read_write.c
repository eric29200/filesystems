#include "tarfs.h"

/*
 * Read a file.
 */
int tarfs_file_read(struct file_t *filp, char *buf, int count)
{
	struct buffer_head_t *bh;
	int pos, nb_chars, left;
	uint32_t block;

	/* adjust size */
	if (filp->f_pos + count > filp->f_inode->i_size)
		count = filp->f_inode->i_size - filp->f_pos;

	/* no more data to read */
	if (count <= 0)
		return 0;

	/* read block by block */
	for (left = count; left > 0;) {
		/* compute data block */
		block = (tarfs_i(filp->f_inode)->entry->data_off + filp->f_pos) / filp->f_inode->i_sb->s_blocksize;

		/* read block buffer */
		bh = sb_bread(filp->f_inode->i_sb, block);
		if (!bh)
			goto out;

		/* find position and numbers of chars to read */
		pos = filp->f_pos % filp->f_inode->i_sb->s_blocksize;
		nb_chars = filp->f_inode->i_sb->s_blocksize - pos <= left ? filp->f_inode->i_sb->s_blocksize - pos : left;

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
	filp->f_inode->i_atime = current_time();
	filp->f_inode->i_dirt = 1;
	return count - left;
}
