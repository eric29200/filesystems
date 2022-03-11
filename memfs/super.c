#include <stdlib.h>
#include <errno.h>

#include "memfs.h"

/*
 * MemFS super operations.
 */
struct super_operations_t memfs_sops = {
  .alloc_inode        = memfs_alloc_inode,
  .put_inode          = memfs_put_inode,
  .delete_inode       = memfs_delete_inode,
  .read_inode         = NULL,
  .write_inode        = NULL,
  .put_super          = memfs_put_super,
  .statfs             = memfs_statfs,
};

/*
 * Read a MemFS super block.
 */
int memfs_read_super(struct super_block_t *sb)
{
  struct memfs_sb_info_t *sbi;
  int err = -EINVAL;

  /* allocate MemFS super block */
  sb->s_fs_info = sbi = (struct memfs_sb_info_t *) malloc(sizeof(struct memfs_sb_info_t));
  if (!sbi)
    return -ENOMEM;

  /* set super block */
  sb->s_blocksize = MEMFS_BLOCK_SIZE;
  sb->s_blocksize_bits = MEMFS_BLOCK_SIZE_BITS;
  sb->s_magic = MEMFS_MAGIC;
  sb->s_op = &memfs_sops;
  memfs_sb(sb)->s_inodes_cpt = MEMFS_ROOT_INODE;

  /* create root inode */
  sb->s_root_inode = memfs_new_inode(sb, S_IFDIR | S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
  if (!sb->s_root_inode)
    goto err_root_inode;

  /* add "." entry */
  err = memfs_add_entry(sb->s_root_inode, ".", 1, sb->s_root_inode->i_ino);
  if (err)
    goto err_root_entries;

  /* add ".." entry */
  err = memfs_add_entry(sb->s_root_inode, "..", 2, sb->s_root_inode->i_ino);
  if (err)
    goto err_root_entries;

  return 0;
err_root_entries:
  fprintf(stderr, "MemFS : can't create root entries\n");
  sb->s_root_inode->i_ref = 0;
  vfs_iput(sb->s_root_inode);
  goto err;
err_root_inode:
  fprintf(stderr, "MemFS : can't create root inode\n");
err:
  free(sbi);
  return err;
}

/*
 * Release a MemFS super block.
 */
void memfs_put_super(struct super_block_t *sb)
{
  /* TODO */
}

/*
 * Get MemFS File system status.
 */
int memfs_statfs(struct super_block_t *sb, struct statfs *buf)
{
  /* TODO */
  return 0;
}
