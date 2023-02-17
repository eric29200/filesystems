#include <string.h>
#include <errno.h>

#include "minix.h"

/*
 * Get directory entries.
 */
int minix_getdents64(struct file_t *filp, void *dirp, size_t count)
{
	struct minix_sb_info_t *sbi = minix_sb(filp->f_inode->i_sb);
	struct minix1_dir_entry_t *de1;
	struct minix3_dir_entry_t *de3;
	struct dirent64_t *dirent;
	int entries_size;
	size_t name_len;
	char *name;
	ino_t ino;
	void *de;

	/* allocate directory entry */
	de = malloc(sbi->s_dirsize);
	if (!de)
		return -ENOMEM;

	/* set Minix directory entries pointer */
	de1 = de;
	de3 = de;

	/* for each entry */
	for (entries_size = 0, dirent = (struct dirent64_t *) dirp;;) {
		/* read minix dir entry */
		if (minix_file_read(filp, (char *) de, sbi->s_dirsize) != sbi->s_dirsize)
			return entries_size;

		/* get inode number and file name */
		if (sbi->s_version == MINIX_V3) {
			ino = de3->d_inode;
			name = de3->d_name;
		} else {
			ino = de1->d_inode;
			name = de1->d_name;
		}

		/* skip null entries */
		if (ino == 0)
			continue;

		/* not enough space to fill in next dir entry : break */
		name_len = strlen(name);
		if (count < sizeof(struct dirent64_t) + name_len + 1) {
			filp->f_pos -= sbi->s_dirsize;
			return entries_size;
		}

		/* fill in dirent */
		dirent->d_inode = ino;
		dirent->d_off = 0;
		dirent->d_reclen = sizeof(struct dirent64_t) + name_len + 1;
		dirent->d_type = 0;
		memcpy(dirent->d_name, name, name_len);
		dirent->d_name[name_len] = 0;

		/* go to next entry */
		count -= dirent->d_reclen;
		entries_size += dirent->d_reclen;
		dirent = (struct dirent64_t *) ((char *) dirent + dirent->d_reclen);
	}

	/* free directory entry */
	free(de);

	return entries_size;
}
