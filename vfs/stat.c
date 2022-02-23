#include <errno.h>

#include "vfs.h"

/*
 * Get file status.
 */
int vfs_stat(struct inode_t *root, const char *filename, struct stat *statbuf)
{
  struct inode_t *inode;

  /* get inode */
  inode = vfs_namei(root, NULL, filename, 0);
  if (!inode)
    return -ENOENT;

  /* copy status */
  statbuf->st_ino = inode->i_ino;
  statbuf->st_mode = inode->i_mode;
  statbuf->st_nlink = inode->i_nlinks;
  statbuf->st_uid = inode->i_uid;
  statbuf->st_gid = inode->i_gid;
  statbuf->st_size = inode->i_size;
  statbuf->st_atime = inode->i_atime.tv_sec;
  statbuf->st_mtime = inode->i_mtime.tv_sec;
  statbuf->st_ctime = inode->i_ctime.tv_sec;

  /* release inode */
  vfs_iput(inode);

  return 0;
}
