#include <string.h>
#include <errno.h>
#include <sys/fcntl.h>

#include "isofs.h"

/*
 * Read a file.
 */
int isofs_file_read(struct file *filp, char *buf, int count)
{
	struct super_block *sb;
	struct buffer_head *bh;
	struct inode *inode;
	int pos, nb_chars, left;
	uint32_t block;

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
		block = (isofs_i(filp->f_inode)->i_first_extent >> filp->f_inode->i_sb->s_blocksize_bits) + (filp->f_pos >> sb->s_blocksize_bits);
		bh = sb_bread(filp->f_inode->i_sb, block);
		if (!bh)
			goto out;

		/* find position and numbers of chars to read */
		pos = filp->f_pos & (sb->s_blocksize - 1);
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
