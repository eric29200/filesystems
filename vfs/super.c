#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "vfs.h"
#include "../minix/minix.h"
#include "../bfs/bfs.h"
#include "../ext2/ext2.h"
#include "../isofs/isofs.h"

/*
 * Mount a file system.
 */
struct super_block_t *vfs_mount(const char *dev, int fs_type)
{
  struct super_block_t *sb;
  int err;
  
  /* allocate a super block */
  sb = (struct super_block_t *) malloc(sizeof(struct super_block_t));
  if (!sb)
    return NULL;
  
  /* open device */
  sb->s_fd = -1;
  if (dev) {
    sb->s_fd = open(dev, O_RDWR);
    if (sb->s_fd < 0) {
      free(sb);
      fprintf(stderr, "VFS: can't open device %s\n", dev);
      return NULL;
    }
  }
  
  /* read super block on disk */
  switch (fs_type) {
    case VFS_MINIX_TYPE:
      err = minix_read_super(sb);
      break;
    case VFS_BFS_TYPE:
      err = bfs_read_super(sb);
      break;
    case VFS_EXT2_TYPE:
      err = ext2_read_super(sb);
      break;
    case VFS_ISOFS_TYPE:
      err = isofs_read_super(sb);
      break;
    default:
      fprintf(stderr, "VFS: file system type (fs_type = %d) not implemented\n", fs_type);
      err = -EINVAL;
      break;
  }
  
  /* failed to read super block */
  if (err) {
    close(sb->s_fd);
    free(sb);
    return NULL;
  }
  
  return sb;
}

/*
 * Unmount a file system.
 */
int vfs_umount(struct super_block_t *sb)
{
  /* check super block */
  if (!sb)
    return -EINVAL;
  
  /* put super block */
  if (sb->s_op && sb->s_op->put_super)
    sb->s_op->put_super(sb);
  
  /* close device */
  if (sb->s_fd > 0)
    close(sb->s_fd);
  
  /* free super block */
  free(sb);
  
  return 0;
}

/*
 * Get file system statistics.
 */
int vfs_statfs(struct super_block_t *sb, struct statfs *buf)
{
  /* check super block */
  if (!sb)
    return -EINVAL;

  /* statfs not implemented */
  if (!sb->s_op || !sb->s_op->statfs)
    return -ENOSYS;

  return sb->s_op->statfs(sb, buf);
}

/*
 * Init VFS.
 */
int vfs_init()
{
  int err;

  /* init block buffers */
  err = vfs_binit();
  if (err)
    return err;

  return 0;
}
