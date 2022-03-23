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
  err = ftp_rmdir(dir->i_sb->s_fd, full_path);
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
