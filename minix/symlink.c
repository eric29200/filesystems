#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>

#include "minix.h"

/*
 * Follow a Minix link (inode will be released).
 */
int minix_follow_link(struct inode_t *dir, struct inode_t *inode, struct inode_t **res_inode)
{
  struct buffer_head_t *bh;
  
  /* reset result inode */
  *res_inode = NULL;
  
  if (!inode)
    return -ENOENT;
  
  /* check if a inode is a link */
  if (!S_ISLNK(inode->i_mode)) {
    *res_inode = inode;
    return 0;
  }
  
  /* read first link block */
  bh = minix_bread(inode, 0, 0);
  if (!bh) {
    vfs_iput(inode);
    return -EIO;
  }
  
  /* release inode */
  vfs_iput(inode);
  
  /* resolve target inode */
  *res_inode = vfs_namei(NULL, dir, bh->b_data, 0);
  if (!*res_inode) {
    brelse(bh);
    return -EACCES;
  }
  
  /* release block buffer */
  brelse(bh);
  return 0;
}

/*
 * Read value of a Minix symbolic link.
 */
ssize_t minix_readlink(struct inode_t *inode, char *buf, size_t bufsize)
{
  struct buffer_head_t *bh;
  ssize_t len;
  
  /* inode must be a link */
  if (!S_ISLNK(inode->i_mode)) {
    vfs_iput(inode);
    return -EINVAL;
  }
  
  /* limit buffer size to block size */
  if (bufsize > inode->i_sb->s_blocksize)
    bufsize = inode->i_sb->s_blocksize;
  
  /* read first block */
  bh = minix_bread(inode, 0, 0);
  if (!bh) {
    vfs_iput(inode);
    return 0;
  }
  
  /* release inode */
  vfs_iput(inode);
  
  /* copy target */
  for (len = 0; len < bufsize; len++)
    buf[len] = bh->b_data[len];
  
  /* release buffer */
  brelse(bh);
  
  return len;
}
