#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include "vfs.h"

/*
 * Get an empty file.
 */
static struct file_t *vfs_get_empty_file()
{
  struct file_t *filp;
  
  /* allocate new file */
  filp = (struct file_t *) malloc(sizeof(struct file_t));
  if (!filp)
    return NULL;
  
  /* memzero file */
  memset(filp, 0, sizeof(struct file_t));
  
  /* set reference */
  filp->f_ref = 1;
  
  return filp;
}

/*
 * Open a file.
 */
struct file_t *vfs_open(struct inode_t *root, const char *pathname, int flags, mode_t mode)
{
  struct inode_t *inode;
  struct file_t *filp;
  int ret;
  
  /* get an empty file */
  filp = vfs_get_empty_file();
  if (!filp)
    return NULL;

  /* open file */
  ret = vfs_open_namei(root, pathname, flags, mode, &inode);
  if (ret) {
    free(filp);
    return NULL;
  }

  /* set file */
  filp->f_mode = inode->i_mode;
  filp->f_inode = inode;
  filp->f_flags = flags;
  filp->f_pos = 0;
  filp->f_op = inode->i_op->fops;

  /* specific open function */
  if (filp->f_op && filp->f_op->open)
    filp->f_op->open(filp);

  return filp;
}

/*
 * Close a file.
 */
int vfs_close(struct file_t *filp)
{
  /* check file */
  if (!filp)
    return -EINVAL;

  /* release file if not used anymore */
  filp->f_ref--;
  if (filp->f_ref <= 0) {
    /* specific close operation */
    if (filp->f_op && filp->f_op->close)
      filp->f_op->close(filp);

    /* release inode */
    vfs_iput(filp->f_inode);
    
    /* free file */
    free(filp);
  }

  return 0;
}

/*
 * Change mode of a file.
 */
int vfs_chmod(struct inode_t *root, const char *pathname, mode_t mode)
{
  struct inode_t *inode;

  /* get inode */
  inode = vfs_namei(root, NULL, pathname, 1);
  if (!inode)
    return -ENOENT;

  /* adjust mode */
  if (mode == (mode_t) -1)
    mode = inode->i_mode;

  /* change mode */
  inode->i_mode = mode;

  /* release inode */
  inode->i_dirt = 1;
  vfs_iput(inode);

  return 0;
}

/*
 * Change file's owner.
 */
int vfs_chown(struct inode_t *root, const char *pathname, uid_t uid, gid_t gid)
{
  struct inode_t *inode;

  /* get inode */
  inode = vfs_namei(root, NULL, pathname, 1);
  if (!inode)
    return -ENOENT;

  /* change uid and gid */
  inode->i_uid = uid;
  inode->i_gid = gid;

  /* release inode */
  inode->i_dirt = 1;
  vfs_iput(inode);

  return 0;
}
