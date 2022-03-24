#include <stdlib.h>
#include <errno.h>

#include "ftpfs.h"

/*
 * Test file names equality.
 */
static inline int ftpfs_name_match(const char *name1, size_t len1, const char *name2)
{
  /* check overflow */
  if (len1 > FTPFS_NAME_LEN)
    return 0;

  return strncmp(name1, name2, len1) == 0 && (len1 == FTPFS_NAME_LEN || name2[len1] == 0);
}

/*
 * Find an entry in a directory.
 */
static int ftpfs_find_entry(struct inode_t *dir, const char *name, size_t name_len, struct ftpfs_fattr_t *res_fattr)
{
  struct ftpfs_inode_info_t *ftpfs_dir = ftpfs_i(dir);
  char *start, *end, *line;
  int err;

  /* check file name length */
  if (!name_len || name_len > FTPFS_NAME_LEN)
    return -ENOENT;

  /* get directory list from server if needed */
  err = ftpfs_load_inode_data(dir, NULL);
  if (err)
    return err;

  /* no data : exit */
  if (!ftpfs_dir->i_cache.data)
    return -ENOENT;

  /* walk through all entries */
  start = ftpfs_dir->i_cache.data;
  while ((end = strchr(start, '\n')) != NULL) {
    /* handle carriage return */
    if (end > start && *(end - 1) == '\r')
      end--;

    /* allocate line */
    line = (char *) malloc(end - start + 1);
    if (!line)
      return -ENOMEM;

    /* set line */
    strncpy(line, start, end - start);
    line[end - start] = 0;

    /* parse line */
    if (ftp_parse_dir_line(line, res_fattr))
      goto next_line;

    /* found */
    if (ftpfs_name_match(name, name_len, res_fattr->name)) {
      free(line);
      return 0;
    }

next_line:
    /* free line */
    free(line);

    /* go to next line */
    start = *end == '\r' ? end + 2 : end + 1;
  }

  return -ENOENT;
}

/*
 * Lookup for a file in a directory.
 */
int ftpfs_lookup(struct inode_t *dir, const char *name, size_t name_len, struct inode_t **res_inode)
{
  struct ftpfs_fattr_t fattr;
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
  err = ftpfs_find_entry(dir, name, name_len, &fattr);
  if (err) {
    vfs_iput(dir);
    return err;
  }

  /* get inode */
  *res_inode = ftpfs_iget(dir->i_sb, dir, &fattr);
  if (!res_inode) {
    vfs_iput(dir);
    return -ENOSPC;
  }

  vfs_iput(dir);
  return 0;
}

/*
 * Create a file in a directory.
 */
int ftpfs_create(struct inode_t *dir, const char *name, size_t name_len, mode_t mode, struct inode_t **res_inode)
{
  struct ftpfs_fattr_t fattr;
  char *full_path = NULL;
  struct inode_t *tmp;
  int err;

  /* check directory */
  *res_inode = NULL;
  if (!dir)
    return -ENOENT;

  /* check if file already exists */
  dir->i_ref++;
  if (ftpfs_lookup(dir, name, name_len, &tmp) == 0) {
    vfs_iput(tmp);
    vfs_iput(dir);
    return -EEXIST;
  }

  /* build full path */
  memcpy(fattr.name, name, name_len);
  fattr.name[name_len] = 0;
  full_path = ftpfs_build_path(dir, &fattr);
  if (!full_path) {
    vfs_iput(dir);
    return -ENOMEM;
  }

  /* create file */
  err = ftp_create(dir->i_sb->s_fd, &ftpfs_sb(dir->i_sb)->s_addr, full_path);
  if (err) {
    free(full_path);
    vfs_iput(dir);
    return -ENOSPC;
  }

  /* reload directory */
  err = ftpfs_reload_inode_data(dir, NULL);
  if (err) {
    free(full_path);
    vfs_iput(dir);
    return -ENOSPC;
  }

  /* get inode */
  *res_inode = ftpfs_iget(dir->i_sb, dir, &fattr);
  if (!*res_inode) {
    free(full_path);
    vfs_iput(dir);
    return -ENOSPC;
  }

  /* release directory */
  free(full_path);
  vfs_iput(dir);

  return 0;
}

/*
 * Unlink (remove) a file.
 */
int ftpfs_unlink(struct inode_t *dir, const char *name, size_t name_len)
{
  struct ftpfs_fattr_t fattr;
  struct inode_t *inode;
  char *full_path;
  int err;

  /* adjust name length */
  if (name_len > FTPFS_NAME_LEN - 1)
    name_len = FTPFS_NAME_LEN - 1;

  /* build full path */
  memcpy(fattr.name, name, name_len);
  fattr.name[name_len] = 0;
  full_path = ftpfs_build_path(dir, &fattr);
  if (!full_path) {
    vfs_iput(dir);
    return -ENOMEM;
  }

  /* get inode */
  inode = ftpfs_iget(dir->i_sb, dir, &fattr);
  if (!inode) {
    err = -ENOENT;
    goto out;
  }

  /* remove file */
  err = ftp_rm(dir->i_sb->s_fd, full_path);
  if (err) {
    err = -EPERM;
    goto out;
  }

  /* update inode */
  inode->i_nlinks--;

  /* update inode data */
  err = ftpfs_reload_inode_data(dir, NULL);

out:
  /* free path */
  free(full_path);

  /* release directory */
  vfs_iput(inode);
  vfs_iput(dir);

  return err;
}

/*
 * Make a directory.
 */
int ftpfs_mkdir(struct inode_t *dir, const char *name, size_t name_len, mode_t mode)
{
  struct ftpfs_fattr_t fattr;
  char *full_path;
  int err;

  /* adjust name length */
  if (name_len > FTPFS_NAME_LEN - 1)
    name_len = FTPFS_NAME_LEN - 1;

  /* build full path */
  memcpy(fattr.name, name, name_len);
  fattr.name[name_len] = 0;
  full_path = ftpfs_build_path(dir, &fattr);
  if (!full_path) {
    vfs_iput(dir);
    return -ENOMEM;
  }

  /* create directory */
  err = ftp_mkdir(dir->i_sb->s_fd, full_path);
  if (err) {
    err = -EPERM;
    goto out;
  }

  /* update inode data */
  err = ftpfs_reload_inode_data(dir, NULL);

out:
  /* free path */
  free(full_path);

  /* release directory */
  vfs_iput(dir);

  return err;
}

/*
 * Remove a directory.
 */
int ftpfs_rmdir(struct inode_t *dir, const char *name, size_t name_len)
{
  struct ftpfs_fattr_t fattr;
  struct inode_t *inode;
  char *full_path;
  int err;

  /* adjust name length */
  if (name_len > FTPFS_NAME_LEN - 1)
    name_len = FTPFS_NAME_LEN - 1;

  /* build full path */
  memcpy(fattr.name, name, name_len);
  fattr.name[name_len] = 0;
  full_path = ftpfs_build_path(dir, &fattr);
  if (!full_path) {
    vfs_iput(dir);
    return -ENOMEM;
  }

  /* get inode */
  inode = ftpfs_iget(dir->i_sb, dir, &fattr);
  if (!inode) {
    err = -ENOENT;
    goto out;
  }

  /* remove directory */
  err = ftp_rmdir(dir->i_sb->s_fd, full_path);
  if (err) {
    err = -EPERM;
    goto out;
  }

  /* update inode */
  inode->i_nlinks = 0;

  /* update inode data */
  err = ftpfs_reload_inode_data(dir, NULL);

out:
  /* free path */
  free(full_path);

  /* release directory */
  vfs_iput(inode);
  vfs_iput(dir);

  return err;
}

/*
 * Rename a file.
 */
int ftpfs_rename(struct inode_t *old_dir, const char *old_name, size_t old_name_len,
                 struct inode_t *new_dir, const char *new_name, size_t new_name_len)
{
  struct inode_t *old_inode = NULL, *new_inode = NULL;
  char *old_full_path = NULL, *new_full_path = NULL;
  struct ftpfs_fattr_t old_fattr, new_fattr;
  int err = 0;

  /* adjust names lengths */
  if (old_name_len > FTPFS_NAME_LEN - 1)
    old_name_len = FTPFS_NAME_LEN - 1;
  if (new_name_len > FTPFS_NAME_LEN - 1)
    new_name_len = FTPFS_NAME_LEN - 1;

  /* build old full path */
  memcpy(old_fattr.name, old_name, old_name_len);
  old_fattr.name[old_name_len] = 0;
  old_full_path = ftpfs_build_path(old_dir, &old_fattr);
  if (!old_full_path) {
    err = -ENOMEM;
    goto out;
  }

  /* build new full path */
  memcpy(new_fattr.name, new_name, new_name_len);
  new_fattr.name[new_name_len] = 0;
  new_full_path = ftpfs_build_path(new_dir, &new_fattr);
  if (!new_full_path) {
    err = -ENOMEM;
    goto out;
  }

  /* same path : exit */
  if (strcmp(old_full_path, new_full_path) == 0)
    goto out;

  /* get old inode */
  old_inode = ftpfs_iget(old_dir->i_sb, old_dir, &old_fattr);
  if (!old_inode) {
    err = -ENOENT;
    goto out;
  }

  /* FTP rename */
  err = ftp_rename(old_dir->i_sb->s_fd, old_full_path, new_full_path);
  if (err) {
    err = -ENOSPC;
    goto out;
  }

  /* reload old dir and new dir */
  err = ftpfs_reload_inode_data(old_dir, NULL);
  if (old_dir != new_dir)
    err = ftpfs_reload_inode_data(new_dir, NULL);

out:
  /* release inodes */
  vfs_iput(old_inode);
  vfs_iput(new_inode);

  /* free full paths */
  if (old_full_path)
    free(old_full_path);
  if (new_full_path)
    free(new_full_path);

  /* release directories */
  vfs_iput(old_dir);
  vfs_iput(new_dir);

  return err;
}
