#include <stdlib.h>
#include <errno.h>

#include "ftpfs.h"

/*
 * Find an entry in a directory.
 */
static int ftpfs_find_entry(struct inode_t *dir, const char *name, size_t name_len, struct stat *res_statbuf)
{
  char *start, *end, *line, filename[FTPFS_NAME_LEN], link[FTPFS_NAME_LEN];
  struct ftpfs_inode_info_t *ftpfs_dir = ftpfs_i(dir);
  struct stat statbuf;
  int err;

  /* check file name length */
  if (!name_len || name_len > FTPFS_NAME_LEN)
    return -ENOENT;

  /* get directory list from server if needed */
  if (!ftpfs_dir->i_cache.data) {
    err = ftp_list(dir->i_sb->s_fd, &ftpfs_sb(dir->i_sb)->s_addr, ftpfs_dir->i_path, &ftpfs_dir->i_cache);
    if (err)
      return -EIO;
  }

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
    memset(&statbuf, 0, sizeof(struct stat));
    if (ftp_parse_dir_line(line, filename, link, &statbuf))
      goto next_line;

    /* found */
    if (strncmp(filename, name, name_len) == 0) {
      memcpy(res_statbuf, &statbuf, sizeof(struct stat));
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
  struct stat statbuf;
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
  err = ftpfs_find_entry(dir, name, name_len, &statbuf);
  if (err) {
    vfs_iput(dir);
    return err;
  }

  /* get inode */
  *res_inode = ftpfs_iget(dir->i_sb, dir, name, name_len, &statbuf);
  if (!res_inode) {
    vfs_iput(dir);
    return -ENOSPC;
  }

  vfs_iput(dir);
  return 0;
}
