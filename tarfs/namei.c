#include <errno.h>

#include "tarfs.h"

/*
 * Find an entry in a directory.
 */
static struct tar_entry_t *tarfs_find_entry(struct inode_t *dir, const char *name, size_t name_len)
{
	struct tar_entry_t *tar_entry = tarfs_i(dir)->entry, *child;
	struct list_head_t *pos;

	/* for each child */
	list_for_each(pos, &tar_entry->children) {
		child = list_entry(pos, struct tar_entry_t, list);
		if (strlen(child->name) == name_len && memcmp(child->name, name, name_len) == 0)
			return child;
	}

	return NULL;
}

/*
 * Lookup for a file in a directory.
 */
int tarfs_lookup(struct inode_t *dir, const char *name, size_t name_len, struct inode_t **res_inode)
{
	struct tar_entry_t *entry;

	/* check dir */
	if (!dir)
		return -ENOENT;

	/* dir must be a directory */
	if (!S_ISDIR(dir->i_mode)) {
		vfs_iput(dir);
		return -ENOENT;
	}

	/* find entry */
	entry = tarfs_find_entry(dir, name, name_len);
	if (!entry) {
		vfs_iput(dir);
		return -ENOENT;
	}

	/* get inode */
	*res_inode = vfs_iget(dir->i_sb, entry->ino);
	if (!*res_inode) {
		vfs_iput(dir);
		return -EACCES;
	}

	vfs_iput(dir);
	return 0;
}
