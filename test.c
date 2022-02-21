#include <stdio.h>

#include "vfs/vfs.h"

int main()
{
  struct super_block_t *sb;
  
  /* mount file system */
  sb = vfs_mount("./disk.img", VFS_MINIXFS_TYPE);
  if (!sb)
    return 1;
  
  printf("%d\n", vfs_create(sb->root_inode, "/test", 0644));
  printf("%d\n", vfs_create(sb->root_inode, "/tesa", 0644));
  printf("%d\n", vfs_unlink(sb->root_inode, "/tesa"));
  printf("%d\n", vfs_mkdir(sb->root_inode, "/dir", 0755));
  printf("%d\n", vfs_create(sb->root_inode, "/tesb", 0644));
  printf("**%d\n", vfs_rename(sb->root_inode, "/tesb", "/tesc"));
  
  /* unmount file system */
  vfs_umount(sb);
  
  return 0;
}
