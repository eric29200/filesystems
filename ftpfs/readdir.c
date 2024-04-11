#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "ftpfs.h"

/*
 * Get directory entries.
 */
int ftpfs_getdents64(struct file *filp, void *dirp, size_t count)
{
	struct ftpfs_inode_info *ftpfs_inode = ftpfs_i(filp->f_inode);
	struct ftpfs_fattr fattr;
	struct dirent64 *dirent;
	int err, entries_size = 0;
	char *start, *end, *line;
	size_t filename_len;

	/* get list from server if needed */
	err = ftpfs_load_inode_data(filp->f_inode, NULL);
	if (err)
		return err;

	/* no data : exit */
	if (!ftpfs_inode->i_cache.data)
		return -ENOENT;

	/* set start offset */
	dirent = (struct dirent64 *) dirp;
	start = ftpfs_inode->i_cache.data + filp->f_pos;

	/* add "." and ".." entries */
	if (filp->f_pos == 0) {
		/* check if input buffer is big enough */
		if (count < sizeof(struct dirent64) * 2 + 2 + 3)
			return -ENOSPC;

		/* add "." entry */
		dirent->d_inode = 0;
		dirent->d_off = 0;
		dirent->d_reclen = sizeof(struct dirent64) + 2;
		dirent->d_type = 0;
		dirent->d_name[0] = '.';
		dirent->d_name[1] = 0;
		count -= dirent->d_reclen;
		entries_size += dirent->d_reclen;
		dirent = (struct dirent64 *) ((char *) dirent + dirent->d_reclen);

		/* add ".." entry */
		dirent->d_inode = 0;
		dirent->d_off = 0;
		dirent->d_reclen = sizeof(struct dirent64) + 3;
		dirent->d_type = 0;
		dirent->d_name[0] = '.';
		dirent->d_name[1] = '.';
		dirent->d_name[2] = 0;
		count -= dirent->d_reclen;
		entries_size += dirent->d_reclen;
		dirent = (struct dirent64 *) ((char *) dirent + dirent->d_reclen);
	}

	/* parse directory listing */
	while ((end = strchr(start, '\n')) != NULL) {
		/* handle carriage return */
		if (end > start && *(end - 1) == '\r')
			end--;

		/* allocate line */
		line = (char *) malloc(end - start + 1);
		if (!line)
			return entries_size;

		/* set line */
		strncpy(line, start, end - start);
		line[end - start] = 0;

		/* parse line */
		if (ftp_parse_dir_line(line, &fattr))
			goto next_line;

		/* not enough space to fill in next dir entry : break */
		filename_len = strlen(fattr.name);
		if (count < sizeof(struct dirent64) + filename_len + 1) {
			free(line);
			return entries_size;
		}

		/* fill in dirent */
		dirent->d_inode = 0;
		dirent->d_off = 0;
		dirent->d_reclen = sizeof(struct dirent64) + filename_len + 1;
		dirent->d_type = 0;
		memcpy(dirent->d_name, fattr.name, filename_len);
		dirent->d_name[filename_len] = 0;

		/* go to next entry */
		count -= dirent->d_reclen;
		entries_size += dirent->d_reclen;
		dirent = (struct dirent64 *) ((char *) dirent + dirent->d_reclen);

next_line:
		/* free line */
		free(line);

		/* go to next line */
		start = *end == '\r' ? end + 2 : end + 1;

		/* update file position */
		filp->f_pos = start - ftpfs_inode->i_cache.data;
	}

	return entries_size;
}
