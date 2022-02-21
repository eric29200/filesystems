#include <errno.h>

#include "vfs.h"

/*
 * Get directory entries.
 */
int vfs_getdents64(struct file_t *filp, void *dirp, size_t count)
{
  /* check file */
  if (!filp)
    return -EINVAL;

  /* getdents not implemented */
  if (!filp->f_op || !filp->f_op->getdents64)
    return -EPERM;

  return filp->f_op->getdents64(filp, dirp, count);
}
