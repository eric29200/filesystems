#include <string.h>

#include "vfs.h"

/* global inode table */
static struct inode_t inode_table[VFS_NR_INODE];

/*
 * Get an empty inode.
 */
struct inode_t *vfs_get_empty_inode(struct super_block_t *sb)
{
  struct inode_t *inode;
  int i;
  
  /* find a free inode */
  for (i = 0; i < VFS_NR_INODE; i++) {
    if (!inode_table[i].i_ref) {
      inode = &inode_table[i];
      break;
    }
  }
  
  /* no more inode */
  if (i >= VFS_NR_INODE)
    return NULL;
  
  /* reset inode */
  memset(inode, 0, sizeof(struct inode_t));
  
  /* set reference and super block */
  inode->i_sb = sb;
  inode->i_ref = 1;
  
  return inode;
}

/*
 * Get an inode.
 */
struct inode_t *vfs_iget(struct super_block_t *sb, ino_t ino)
{
  struct inode_t *inode;
  int err, i;
  
  /* try to find inode in table */
  for (i = 0; i < VFS_NR_INODE; i++) {
    if (inode_table[i].i_ino == ino && inode_table[i].i_sb == sb) {
      inode = &inode_table[i];
      inode->i_ref++;
      return inode;
    }
  }
  
  /* allocate generic inode */
  inode = vfs_get_empty_inode(sb);
  if (!inode)
    return NULL;
  
  /* set inode number */
  inode->i_ino = ino;
  
  /* read inode */
  err = sb->s_op->read_inode(inode);
  if (err) {
    vfs_iput(inode);
    return NULL;
  }
  
  return inode;
}


/*
 * Release an inode.
 */
void vfs_iput(struct inode_t *inode)
{
  if (!inode)
    return;
  
  /* update reference */
  inode->i_ref--;
  
  /* write inode in disk if needed */
  if (inode->i_dirt) {
    inode->i_sb->s_op->write_inode(inode);
    inode->i_dirt = 0;
  }
  
  /* removed inode : truncate and free it */
  if (!inode->i_nlinks && !inode->i_ref) {
    inode->i_sb->s_op->put_inode(inode);
    return;
  }
  
  /* free inode */
  if (!inode->i_ref)
    memset(inode, 0, sizeof(struct inode_t));
}

