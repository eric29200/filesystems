#include <stdlib.h>
#include <errno.h>

#include "tarfs.h"

#define TARFS_ALLOC_SIZE          1024

/*
 * TarFS super block operations.
 */
struct super_operations_t tarfs_sops = {
  .alloc_inode        = tarfs_alloc_inode,
  .put_inode          = tarfs_put_inode,
  .read_inode         = tarfs_read_inode,
  .put_super          = tarfs_put_super,
  .statfs             = tarfs_statfs,
};

/*
 * Read a TarFS super block.
 */
int tarfs_read_super(struct super_block_t *sb, void *data)
{
  struct tarfs_sb_info_t *sbi;
  size_t i;
  int err;

  /* allocate TarFS super block */
  sb->s_fs_info = sbi = (struct tarfs_sb_info_t *) malloc(sizeof(struct tarfs_sb_info_t));
  if (!sbi)
    return -ENOMEM;

  /* set super block */
  sb->s_blocksize_bits = TARFS_BLOCK_SIZE_BITS;
  sb->s_blocksize = TARFS_BLOCK_SIZE;
  sb->s_magic = TARFS_MAGIC;
  sb->s_root_inode = NULL;
  sb->s_op = &tarfs_sops;
  sbi->s_ninodes = 0;
  sbi->s_root_entry = NULL;
  sbi->s_tar_entries = NULL;

  /* parse TAR archive */
  err = tar_create(sb);
  if (err)
    goto err_bad_sb;

  /* create TAR entries index */
  sbi->s_tar_entries = (struct tar_entry_t **) malloc(sizeof(struct tar_entry_t *) * sbi->s_ninodes);
  if (!sbi->s_tar_entries) {
    err = -ENOMEM;
    goto err_index;
  }

  /* reset entries */
  for (i = 0; i < sbi->s_ninodes; i++)
    sbi->s_tar_entries[i] = NULL;

  /* index TAR entries */
  tar_index(sb, sbi->s_root_entry);

  /* get root inode */
  sb->s_root_inode = vfs_iget(sb, TARFS_ROOT_INO);
  if (!sb->s_root_inode) {
    err = -ENOSPC;
    goto err_root_inode;
  }

  return 0;
err_root_inode:
  fprintf(stderr, "TARFS : can't get root inode\n");
  goto err_release_tar;
err_index:
  fprintf(stderr, "TARFS : can't index TAR entries\n");
err_release_tar:
  tar_free(sbi->s_root_entry);
  goto err;
err_bad_sb:
  fprintf(stderr, "TARFS : can't read super block\n");
err:
  free(sbi);
  return err;
}

/*
 * Unmount a TarFS File System.
 */
void tarfs_put_super(struct super_block_t *sb)
{
  struct tarfs_sb_info_t *sbi = tarfs_sb(sb);

  /* release root inode */
  vfs_iput(sb->s_root_inode);

  /* free all entries */
  tar_free(sbi->s_root_entry);
  if (sbi->s_tar_entries)
    free(sbi->s_tar_entries);

  /* free in memory super block */
  free(sbi);
}

/*
 * Get TarFS File system status.
 */
int tarfs_statfs(struct super_block_t *sb, struct statfs *buf)
{
  struct tarfs_sb_info_t *sbi = tarfs_sb(sb);

  buf->f_type = sb->s_magic;
  buf->f_bsize = sb->s_blocksize;
  buf->f_blocks = 0;
  buf->f_bfree = 0;
  buf->f_bavail = buf->f_bfree;
  buf->f_files = sbi->s_ninodes;
  buf->f_ffree = 0;
  buf->f_namelen = 0;

  return 0;
}
