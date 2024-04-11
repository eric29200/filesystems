#include <string.h>
#include <errno.h>

#include "memfs.h"

/*
 * Get directory entries.
 */
int memfs_getdents64(struct file *filp, void *dirp, size_t count)
{
	struct dirent64 *dirent = (struct dirent64 *) dirp;
	struct inode *inode = filp->f_inode;
	uint32_t offset = filp->f_pos;
	struct memfs_dir_entry *de;
	int entries_size = 0;

	/* read file */
	while (filp->f_pos < inode->i_size) {
		/* check next entry */
		de = (struct memfs_dir_entry *) (memfs_i(inode)->i_data + offset);
		if (de->d_rec_len <= 0)
			return entries_size;

		/* skip null entry */
		if (de->d_inode == 0) {
			offset += de->d_rec_len;
			filp->f_pos += de->d_rec_len;
			continue;
		}

		/* not enough space to fill in next dir entry : break */
		if (count < sizeof(struct dirent64) + de->d_name_len + 1)
			return entries_size;

		/* fill in dirent */
		dirent->d_inode = de->d_inode;
		dirent->d_off = 0;
		dirent->d_reclen = sizeof(struct dirent64) + de->d_name_len + 1;
		dirent->d_type = 0;
		memcpy(dirent->d_name, de->d_name, de->d_name_len);
		dirent->d_name[de->d_name_len] = 0;

		/* update offset */
		offset += de->d_rec_len;

		/* go to next entry */
		count -= dirent->d_reclen;
		entries_size += dirent->d_reclen;
		dirent = (struct dirent64 *) ((char *) dirent + dirent->d_reclen);

		/* update file position */
		filp->f_pos += de->d_rec_len;
	}

	return entries_size;
}
