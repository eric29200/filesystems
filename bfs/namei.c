#include <errno.h>

#include "bfs.h"

/*
 * Test file names equality.
 */
static inline int bfs_name_match(const char *name1, size_t len1, const char *name2)
{
	/* check overflow */
	if (len1 > BFS_NAME_LEN)
		return 0;

	return strncmp(name1, name2, len1) == 0 && (len1 == BFS_NAME_LEN || name2[len1] == 0);
}

/*
 * Find a BFS entry in a directory.
 */
static struct buffer_head *bfs_find_entry(struct inode *dir, const char *name, size_t name_len, struct bfs_dir_entry **res_de)
{
	struct buffer_head *bh = NULL;
	struct bfs_dir_entry *de;
	int nb_entries, i;

	/* check file name length */
	if (!name_len || name_len > BFS_NAME_LEN)
		return NULL;

	/* compute number of entries */
	nb_entries = dir->i_size / BFS_DIRENT_SIZE;

	/* walk through all entries */
	for (i = 0; i < nb_entries; i++) {
		/* read next block if needed */
		if (i % BFS_DIRS_PER_BLOCK == 0) {
			/* release previous block */
			brelse(bh);

			/* read next block */
			bh = sb_bread(dir->i_sb, bfs_i(dir)->i_sblock + i / BFS_DIRS_PER_BLOCK);
			if (!bh)
				return NULL;
		}

		/* get directory entry */
		de = (struct bfs_dir_entry *) (bh->b_data + (i % BFS_DIRS_PER_BLOCK) * BFS_DIRENT_SIZE);
		if (bfs_name_match(name, name_len, de->d_name)) {
			*res_de = de;
			return bh;
		}
	}

	/* release block buffer */
	brelse(bh);
	return NULL;
}

/*
 * Lookup for a file in a directory.
 */
int bfs_lookup(struct inode *dir, const char *name, size_t name_len, struct inode **res_inode)
{
	struct buffer_head *bh = NULL;
	struct bfs_dir_entry *de;
	ino_t ino;

	/* check dir */
	if (!dir)
		return -ENOENT;

	/* dir must be a directory */
	if (!S_ISDIR(dir->i_mode)) {
		vfs_iput(dir);
		return -ENOENT;
	}

	/* find entry */
	bh = bfs_find_entry(dir, name, name_len, &de);
	if (!bh) {
		vfs_iput(dir);
		return -ENOENT;
	}

	/* get inode number */
	ino = le16toh(de->d_ino);

	/* release block buffer */
	brelse(bh);

	/* get inode */
	*res_inode = vfs_iget(dir->i_sb, ino);
	if (!*res_inode) {
		vfs_iput(dir);
		return -EACCES;
	}

	vfs_iput(dir);
	return 0;
}

/*
 * Add a BFS entry in a directory.
 */
static int bfs_add_entry(struct inode *dir, const char *name, size_t name_len, struct inode *inode)
{
	struct bfs_inode_info *bfs_dir = bfs_i(dir);
	struct bfs_dir_entry *de;
	struct buffer_head *bh;
	int block, off;

	/* check file name */
	if (!name_len)
		return -EINVAL;
	if (name_len > BFS_NAME_LEN)
		return -ENAMETOOLONG;

	/* walk through all directories block */
	for (block = bfs_dir->i_sblock; block <= bfs_dir->i_eblock; block++) {
		/* read block */
		bh = sb_bread(dir->i_sb, block);
		if (!bh)
			return -ENOSPC;

		/* walk through all directory entries */
		for (off = 0; off < BFS_BLOCK_SIZE; off += BFS_DIRENT_SIZE) {
			de = (struct bfs_dir_entry *) (bh->b_data + off);

			/* free inode */
			if (!le16toh(de->d_ino)) {
				/* update directory size */
				if ((block - bfs_dir->i_sblock) * BFS_BLOCK_SIZE + off >= dir->i_size) {
					dir->i_size += BFS_DIRENT_SIZE;
					dir->i_ctime = current_time();
				}

				/* set new entry */
				memset(de->d_name, 0, BFS_NAME_LEN);
				memcpy(de->d_name, name, name_len);

				/* set new entry inode */
				de->d_ino = htole16(inode->i_ino);

				/* mark buffer dirty and release it */
				bh->b_dirt = 1;
				brelse(bh);

				/* update parent directory */
				dir->i_mtime = dir->i_ctime = current_time();
				dir->i_dirt = 1;

				return 0;
			}
		}

		/* release block */
		brelse(bh);
	}

	return -ENOSPC;
}

/*
 * Create a file in a directory.
 */
int bfs_create(struct inode *dir, const char *name, size_t name_len, mode_t mode, struct inode **res_inode)
{
	struct inode *inode, *tmp;
	ino_t ino;
	int err;

	/* check directory */
	*res_inode = NULL;
	if (!dir)
		return -ENOENT;

	/* check if file already exists */
	dir->i_ref++;
	if (bfs_lookup(dir, name, name_len, &tmp) == 0) {
		vfs_iput(tmp);
		vfs_iput(dir);
		return -EEXIST;
	}

	/* create a new inode */
	inode = bfs_new_inode(dir->i_sb);
	if (!inode) {
		vfs_iput(dir);
		return -ENOSPC;
	}

	/* set inode */
	inode->i_op = &bfs_file_iops;
	inode->i_mode = S_IFREG | mode;
	inode->i_dirt = 1;

	/* add new entry to dir */
	err = bfs_add_entry(dir, name, name_len, inode);
	if (err) {
		inode->i_nlinks--;
		vfs_iput(inode);
		vfs_iput(dir);
		return err;
	}

	/* release inode (to write it on disk) */
	ino = inode->i_ino;
	vfs_iput(inode);

	/* read inode from disk */
	*res_inode = vfs_iget(dir->i_sb, ino);
	if (!*res_inode) {
		vfs_iput(dir);
		return -EACCES;
	}

	/* release directory */
	vfs_iput(dir);

	return 0;
}

/*
 * Make a new name for a BFS file (= hard link).
 */
int bfs_link(struct inode *old_inode, struct inode *dir, const char *name, size_t name_len)
{
	struct bfs_dir_entry *de;
	struct buffer_head *bh;
	int err;

	/* check if new file exists */
	bh = bfs_find_entry(dir, name, name_len, &de);
	if (bh) {
		brelse(bh);
		vfs_iput(old_inode);
		vfs_iput(dir);
		return -EEXIST;
	}

	/* add entry */
	err = bfs_add_entry(dir, name, name_len, old_inode);
	if (err) {
		vfs_iput(old_inode);
		vfs_iput(dir);
		return err;
	}

	/* update old inode */
	old_inode->i_ctime = current_time();
	old_inode->i_nlinks++;
	old_inode->i_dirt = 1;

	/* release inodes */
	vfs_iput(old_inode);
	vfs_iput(dir);

	return 0;
}

/*
 * Unlink (remove) a BFS file.
 */
int bfs_unlink(struct inode *dir, const char *name, size_t name_len)
{
	struct bfs_dir_entry *de;
	struct buffer_head *bh;
	struct inode *inode;

	/* get directory entry */
	bh = bfs_find_entry(dir, name, name_len, &de);
	if (!bh) {
		vfs_iput(dir);
		return -ENOENT;
	}

	/* get inode */
	inode = vfs_iget(dir->i_sb, le16toh(de->d_ino));
	if (!inode) {
		vfs_iput(dir);
		brelse(bh);
		return -ENOENT;
	}

	/* reset directory entry */
	memset(de, 0, BFS_DIRENT_SIZE);
	bh->b_dirt = 1;
	brelse(bh);

	/* update directory */
	dir->i_ctime = dir->i_mtime = current_time();
	dir->i_dirt = 1;

	/* update inode */
	inode->i_ctime = dir->i_ctime;
	inode->i_nlinks--;
	inode->i_dirt = 1;

	/* release inode */
	vfs_iput(inode);
	vfs_iput(dir);

	return 0;
}

/*
 * Rename a BFS file.
 */
int bfs_rename(struct inode *old_dir, const char *old_name, size_t old_name_len, struct inode *new_dir, const char *new_name, size_t new_name_len)
{
	struct inode *old_inode = NULL, *new_inode = NULL;
	struct buffer_head *old_bh = NULL, *new_bh = NULL;
	struct bfs_dir_entry *old_de, *new_de;
	int err;

	/* find old entry */
	old_bh = bfs_find_entry(old_dir, old_name, old_name_len, &old_de);
	if (!old_bh) {
		err = -ENOENT;
		goto out;
	}

	/* get old inode */
	old_inode = vfs_iget(old_dir->i_sb, le16toh(old_de->d_ino));
	if (!old_inode) {
		err = -ENOSPC;
		goto out;
	}

	/* find new entry (if exists) or add new one */
	new_bh = bfs_find_entry(new_dir, new_name, new_name_len, &new_de);
	if (new_bh) {
		/* get new inode */
		new_inode = vfs_iget(new_dir->i_sb, le16toh(new_de->d_ino));
		if (!new_inode) {
			err = -ENOSPC;
			goto out;
		}

		/* same inode : exit */
		if (old_inode->i_ino == new_inode->i_ino) {
			err = 0;
			goto out;
		}

		/* modify new directory entry inode */
		new_de->d_ino = htole16(old_inode->i_ino);

		/* update new inode */
		new_inode->i_nlinks--;
		new_inode->i_atime = new_inode->i_mtime = current_time();
		new_inode->i_dirt = 1;
	} else {
		/* add new entry */
		err = bfs_add_entry(new_dir, new_name, new_name_len, old_inode);
		if (err)
			goto out;
	}

	/* cancel old directory entry */
	old_de->d_ino = 0;
	memset(old_de->d_name, 0, BFS_NAME_LEN);
	old_bh->b_dirt = 1;

	/* update old and new directories */
	old_dir->i_atime = old_dir->i_mtime = current_time();
	old_dir->i_dirt = 1;
	new_dir->i_atime = new_dir->i_mtime = current_time();
	new_dir->i_dirt = 1;

	err = 0;
out:
	/* release buffers and inodes */
	brelse(old_bh);
	brelse(new_bh);
	vfs_iput(old_inode);
	vfs_iput(new_inode);
	vfs_iput(old_dir);
	vfs_iput(new_dir);

	return err;
}
