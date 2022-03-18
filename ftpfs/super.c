#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>

#include "ftpfs.h"

/*
 * FTPFS super operations.
 */
struct super_operations_t ftpfs_sops = {
  .alloc_inode          = ftpfs_alloc_inode,
  .put_inode            = ftpfs_put_inode,
  .delete_inode         = NULL,
  .read_inode           = NULL,
  .write_inode          = NULL,
  .put_super            = ftpfs_put_super,
  .statfs               = ftpfs_statfs,
};

/*
 * Create root inode.
 */
static struct inode_t *ftpfs_create_root_inode(struct super_block_t *sb)
{
  struct inode_t *inode;
  struct stat statbuf;
  int err;

  /* set attributes */
  memset(&statbuf, 0, sizeof(struct stat));
  statbuf.st_mode = S_IFDIR | 0755;
  statbuf.st_nlink = 2;
  statbuf.st_size = 0;

  /* get inode */
  inode = ftpfs_iget(sb, NULL, NULL, 0, &statbuf);
  if (!inode)
    return NULL;

  /* list root directory */
  err = ftp_list(sb->s_fd, &ftpfs_sb(sb)->s_addr, ftpfs_i(inode)->i_path, &ftpfs_i(inode)->i_cache);
  if (err) {
    inode->i_ref = 0;
    vfs_iput(inode);
    return NULL;
  }

  return inode;
}

/*
 * Read a FTPFS super block.
 */
int ftpfs_read_super(struct super_block_t *sb)
{
  struct ftpfs_sb_info_t *sbi;
  int err = -ENOSPC;

  /* allocate FTPFS super block */
  sb->s_fs_info = sbi = (struct ftpfs_sb_info_t *) malloc(sizeof(struct ftpfs_sb_info_t));
  if (!sbi)
    return -ENOMEM;

  /* set super block */
  sb->s_blocksize_bits = 0;
  sb->s_blocksize = 0;
  sb->s_magic = FTPFS_MAGIC;
  sb->s_op = &ftpfs_sops;
  sb->s_root_inode = NULL;

  /* connect to server */
  sb->s_fd = ftp_connect(sb->s_dev, "anonymous", "anonymous", &sbi->s_addr);
  if (sb->s_fd < 0)
    goto err_connect;

  /* create root inode */
  sb->s_root_inode = ftpfs_create_root_inode(sb);
  if (!sb->s_root_inode)
    goto err_root_inode;

  return 0;
err_root_inode:
  fprintf(stderr, "FTPFS : can't get root inode\n");
  goto err;
err_connect:
  fprintf(stderr, "FTPFS : can't connect to server\n");
err:
  free(sbi);
  return err;
}

/*
 * Release a FTPFS super block.
 */
void ftpfs_put_super(struct super_block_t *sb)
{
  struct ftpfs_sb_info_t *sbi = ftpfs_sb(sb);

  /* release root inode */
  vfs_iput(sb->s_root_inode);

  /* exit FTP session */
  if (sb->s_fd > 0)
    ftp_quit(sb->s_fd);

  /* free in memory super block */
  free(sbi);
}

/*
 * Get FTPFS file system status.
 */
int ftpfs_statfs(struct super_block_t *sb, struct statfs *buf)
{
  buf->f_type = sb->s_magic;
  buf->f_bsize = sb->s_blocksize;
  buf->f_blocks = 0;
  buf->f_bfree = 0;
  buf->f_bavail = 0;
  buf->f_files = 0;
  buf->f_ffree = 0;
  buf->f_namelen = 0;

  return 0;
}
