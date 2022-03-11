#include <stdlib.h>
#include <errno.h>

#include "memfs.h"

/*
 * Test file names equality.
 */
static inline int memfs_name_match(const char *name, size_t len, struct memfs_dir_entry_t *de)
{
  /* check dir entry */
  if (!de || !de->d_inode || len > MEMFS_NAME_LEN)
    return 0;

  return len == de->d_name_len && !memcmp(name, de->d_name, len);
}

/*
 * Find a MemFS entry in a directory.
 */
static int memfs_find_entry(struct inode_t *dir, const char *name, size_t name_len, struct memfs_dir_entry_t **res_de)
{
  struct memfs_dir_entry_t *de;
  uint32_t pos;

  /* read all entries */
  for (pos = 0; pos < dir->i_size;) {
    /* check next entry */
    de = (struct memfs_dir_entry_t *) (memfs_i(dir)->i_data + pos);
    if (de->d_rec_len <= 0)
      return -ENOENT;

    /* skip null entry */
    if (de->d_inode == 0) {
      pos += de->d_rec_len;
      continue;
    }

    /* check name */
    if (memfs_name_match(name, name_len, de)) {
      *res_de = de;
      return 0;
    }

    /* update position */
    pos += de->d_rec_len;
  }

  return -ENOENT;
}

/*
 * Add an entry in a directory.
 */
int memfs_add_entry(struct inode_t *dir, const char *name, size_t name_len, ino_t ino)
{
  struct memfs_dir_entry_t *de;
  uint16_t rec_len;
  uint32_t pos;

  /* truncate name if needed */
  if (name_len > MEMFS_NAME_LEN)
    name_len = MEMFS_NAME_LEN;

  /* compute record length */
  rec_len = MEMFS_DIR_REC_LEN(name_len);

  /* try to find a free entry */
  for (pos = 0; pos < dir->i_size;) {
    /* check next entry */
    de = (struct memfs_dir_entry_t *) (memfs_i(dir)->i_data + pos);
    if (de->d_rec_len <= 0)
      return -ENOENT;

    /* free entry with enough space */
    if (de->d_inode == 0 && de->d_rec_len >= rec_len)
      goto found_entry;

    /* go to next entry */
    pos += de->d_rec_len;
    de = (struct memfs_dir_entry_t *) ((char *) de + de->d_rec_len);
  }

  /* grow directory */
  memfs_i(dir)->i_data = (char *) realloc(memfs_i(dir)->i_data, dir->i_size + rec_len);
  if (!memfs_i(dir)->i_data)
    return -ENOMEM;

  /* go to new entry */
  de = (struct memfs_dir_entry_t *) (memfs_i(dir)->i_data + dir->i_size);
  dir->i_size += rec_len;
  dir->i_dirt = 1;
  goto found_entry;

  return -EINVAL;
found_entry:
  /* set new entry */
  de->d_rec_len = rec_len;
  de->d_inode = ino;
  de->d_name_len = name_len;
  de->d_file_type = 0;
  memcpy(de->d_name, name, name_len);

  /* update parent directory */
  dir->i_mtime = dir->i_ctime = current_time();
  dir->i_dirt = 1;

  return 0;
}

/*
 * Lookup for a file in a directory.
 */
int memfs_lookup(struct inode_t *dir, const char *name, size_t name_len, struct inode_t **res_inode)
{
  struct memfs_dir_entry_t *de;
  ino_t ino;
  int err;

  /* check dir */
  if (!dir)
    return -ENOENT;

  /* dir must be a directory */
  if (!S_ISDIR(dir->i_mode)) {
    vfs_iput(dir);
    return -ENOENT;
  }

  /* find entry */
  err = memfs_find_entry(dir, name, name_len, &de);
  if (err) {
    vfs_iput(dir);
    return -ENOENT;
  }

  /* get inode number */
  ino = de->d_inode;

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
 * Create a file in a directory.
 */
int memfs_create(struct inode_t *dir, const char *name, size_t name_len, mode_t mode, struct inode_t **res_inode)
{
  struct inode_t *inode, *tmp;
  int err;

  /* check directory */
  *res_inode = NULL;
  if (!dir)
    return -ENOENT;

  /* check if file already exists */
  dir->i_ref++;
  if (memfs_lookup(dir, name, name_len, &tmp) == 0) {
    vfs_iput(tmp);
    vfs_iput(dir);
    return -EEXIST;
  }

  /* create a new inode */
  inode = memfs_new_inode(dir->i_sb, S_IFREG | mode);
  if (!inode) {
    vfs_iput(dir);
    return -ENOSPC;
  }

  /* mark inode dirty */
  inode->i_dirt = 1;

  /* add new entry to dir */
  err = memfs_add_entry(dir, name, name_len, inode->i_ino);
  if (err) {
    inode->i_nlinks--;
    vfs_iput(inode);
    vfs_iput(dir);
    return err;
  }

  /* set result inode */
  *res_inode = inode;

  /* release directory */
  vfs_iput(dir);

  return 0;
}

/*
 * Unlink (remove) a file.
 */
int memfs_unlink(struct inode_t *dir, const char *name, size_t name_len)
{
  struct memfs_dir_entry_t *de;
  struct inode_t *inode;
  int err;

  /* get directory entry */
  err = memfs_find_entry(dir, name, name_len, &de);
  if (err) {
    vfs_iput(dir);
    return -ENOENT;
  }

  /* get inode */
  inode = vfs_iget(dir->i_sb, de->d_inode);
  if (!inode) {
    vfs_iput(dir);
    return -ENOENT;
  }

  /* remove regular files only */
  if (S_ISDIR(inode->i_mode)) {
    err = -EPERM;
    goto out;
  }

  /* delete entry */
  de->d_inode = 0;

  /* update directory */
  dir->i_ctime = dir->i_mtime = current_time();
  dir->i_dirt = 1;

  /* update inode */
  inode->i_ctime = dir->i_ctime;
  inode->i_nlinks--;
  inode->i_dirt = 1;

out:
  vfs_iput(inode);
  vfs_iput(dir);
  return err;
}

/*
 * Rename a file.
 */
int memfs_rename(struct inode_t *old_dir, const char *old_name, size_t old_name_len,
                 struct inode_t *new_dir, const char *new_name, size_t new_name_len)
{
  struct inode_t *old_inode = NULL, *new_inode = NULL;
  struct memfs_dir_entry_t *old_de, *new_de;
  int err;

  /* find old entry */
  err = memfs_find_entry(old_dir, old_name, old_name_len, &old_de);
  if (err)
    goto out;

  /* get old inode */
  old_inode = vfs_iget(old_dir->i_sb, old_de->d_inode);
  if (!old_inode) {
    err = -ENOSPC;
    goto out;
  }

  /* find new entry (if exists) or add new one */
  err = memfs_find_entry(new_dir, new_name, new_name_len, &new_de);
  if (!err) {
    /* get new inode */
    new_inode = vfs_iget(new_dir->i_sb, new_de->d_inode);
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
    new_de->d_inode = old_inode->i_ino;

    /* update new inode */
    new_inode->i_nlinks--;
    new_inode->i_atime = new_inode->i_mtime = current_time();
    new_inode->i_dirt = 1;
  } else {
    /* add new entry */
    err = memfs_add_entry(new_dir, new_name, new_name_len, old_inode->i_ino);
    if (err)
      goto out;
  }

  /* remove old directory entry */
  old_de->d_inode = 0;

  /* update old and new directories */
  old_dir->i_atime = old_dir->i_mtime = current_time();
  old_dir->i_dirt = 1;
  new_dir->i_atime = new_dir->i_mtime = current_time();
  new_dir->i_dirt = 1;

  err = 0;
out:
  /* release inodes */
  vfs_iput(old_inode);
  vfs_iput(new_inode);
  vfs_iput(old_dir);
  vfs_iput(new_dir);
  return err;
}
