#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "ftpfs.h"

/*
 * Open a file.
 */
int ftpfs_open(struct file_t *filp)
{
  struct ftpfs_inode_info_t *ftpfs_inode = ftpfs_i(filp->f_inode);
  struct super_block_t *sb = filp->f_inode->i_sb;
  char filename[FTPFS_NAME_LEN];
  int fd, err;

  /* directory or symbolic link : nothing to do */
  if (!S_ISREG(filp->f_inode->i_mode))
    return 0;

  /* create a temporary file */
  memset(filename, 0, FTPFS_NAME_LEN);
  strcpy(filename, "/tmp/fmounter-XXXXXX");
  fd = mkstemp(filename);
  if (fd < 0) {
    fprintf(stderr, "FTPFS : can't create temporary file\n");
    return -EIO;
  }

  /* unlink file -> file will be deleted on close */
  unlink(filename);

  /* retrieve file from FTP */
  err = ftp_retrieve(sb->s_fd, &ftpfs_sb(sb)->s_addr, ftpfs_inode->i_path, fd);
  if (err) {
    close(fd);
    return -ENOSPC;
  }

  /* allocate file descriptor */
  filp->f_private = malloc(sizeof(int));
  if (!filp->f_private) {
    close(fd);
    return -ENOMEM;
  }

  /* set file descriptor */
  *((int *) filp->f_private) = fd;

  /* rewind file descriptor */
  lseek(fd, 0, SEEK_SET);

  return 0;
}

/*
 * Close a file.
 */
int ftpfs_close(struct file_t *filp)
{
  int fd;

  if (filp->f_private) {
    /* get temporary file descriptor */
    fd = *((int *) filp->f_private);

    /* if inode is dirty, write it on FTP */
    if (filp->f_inode->i_dirt) {
      ftp_store(filp->f_inode->i_sb->s_fd, &ftpfs_sb(filp->f_inode->i_sb)->s_addr, ftpfs_i(filp->f_inode)->i_path, fd);
      filp->f_inode->i_dirt = 0;
    }

    /* close tmporary file */
    close(fd);
    free(filp->f_private);
  }

  return 0;
}
