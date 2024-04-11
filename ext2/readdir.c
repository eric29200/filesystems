#include <string.h>
#include <errno.h>

#include "ext2.h"

/*
 * Get directory entries.
 */
int ext2_getdents64(struct file *filp, void *dirp, size_t count)
{
	struct super_block *sb = filp->f_inode->i_sb;
	struct inode *inode = filp->f_inode;
	struct buffer_head *bh = NULL;
	struct ext2_dir_entry *de;
	struct dirent64 *dirent;
	uint32_t offset, block;
	int entries_size = 0;

	/* get start offset */
	offset = filp->f_pos & (sb->s_blocksize - 1);
	dirent = (struct dirent64 *) dirp;

	/* read block by block */
	while (filp->f_pos < inode->i_size) {
		/* read next block */
		block = filp->f_pos >> sb->s_blocksize_bits;
		bh = ext2_bread(inode, block, 0);
		if (!bh) {
			filp->f_pos += sb->s_blocksize - offset;
			continue;
		}

		/* read all entries in block */
		while (filp->f_pos < inode->i_size && offset < sb->s_blocksize) {
			/* check next entry */
			de = (struct ext2_dir_entry *) (bh->b_data + offset);
			if (le16toh(de->d_rec_len) <= 0) {
				brelse(bh);
				return entries_size;
			}

			/* skip null entry */
			if (le32toh(de->d_inode) == 0) {
				offset += le16toh(de->d_rec_len);
				filp->f_pos += le16toh(de->d_rec_len);
				continue;
			}

			/* not enough space to fill in next dir entry : break */
			if (count < sizeof(struct dirent64) + de->d_name_len + 1) {
				brelse(bh);
				return entries_size;
			}

			/* fill in dirent */
			dirent->d_inode = le32toh(de->d_inode);
			dirent->d_off = 0;
			dirent->d_reclen = sizeof(struct dirent64) + de->d_name_len + 1;
			dirent->d_type = 0;
			memcpy(dirent->d_name, de->d_name, de->d_name_len);
			dirent->d_name[de->d_name_len] = 0;

			/* update offset */
			offset += le16toh(de->d_rec_len);

			/* go to next entry */
			count -= dirent->d_reclen;
			entries_size += dirent->d_reclen;
			dirent = (struct dirent64 *) ((char *) dirent + dirent->d_reclen);

			/* update file position */
			filp->f_pos += le16toh(de->d_rec_len);
		}

		/* reset offset and release block buffer */
		offset = 0;
		brelse(bh);
	}

	return entries_size;
}
