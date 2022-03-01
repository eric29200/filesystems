#include <stdlib.h>
#include <string.h>

#include "vfs.h"

/* global inodes list */
static LIST_HEAD(inodes_list);

/*
 * Get an empty inode.
 */
struct inode_t *vfs_get_empty_inode(struct super_block_t *sb)
{
  struct inode_t *inode;

  /* allocation inode not implemented */
  if (!sb->s_op || !sb->s_op->alloc_inode)
    return NULL;

  /* allocate new inode */
  inode = sb->s_op->alloc_inode(sb);
  if (!inode)
    return NULL;
  
  /* reset inode */
  memset(inode, 0, sizeof(struct inode_t));

  /* set new inode */
  inode->i_sb = sb;
  inode->i_ref = 1;
  INIT_LIST_HEAD(&inode->i_list);

  /* add inode to global list */
  list_add(&inode->i_list, &inodes_list);
  
  return inode;
}

/*
 * Get an inode.
 */
struct inode_t *vfs_iget(struct super_block_t *sb, ino_t ino)
{
  struct list_head_t *pos;
  struct inode_t *inode;
  int err;

  /* try to find inode in table */
  list_for_each(pos, &inodes_list) {
    inode = list_entry(pos, struct inode_t, i_list);
    if (inode->i_ino == ino && inode->i_sb == sb) {
      inode->i_ref++;
      return inode;
    }
  }
  
  /* check if read_inode is implemented */
  if (!sb->s_op || !sb->s_op->read_inode)
    return NULL;

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
 * Destroy an inode.
 */
void vfs_destroy_inode(struct inode_t *inode)
{
  if (inode) {
    list_del(&inode->i_list);
    free(inode);
  }
}

/*
 * Release an inode.
 */
void vfs_iput(struct inode_t *inode)
{
  struct super_operations_t *op = NULL;

  if (!inode)
    return;
  
  /* update reference */
  inode->i_ref--;
  
  /* get super operations */
  if (inode->i_sb && inode->i_sb->s_op)
    op = inode->i_sb->s_op;

  /* write inode on disk if needed */
  if (inode->i_dirt && op && op->write_inode) {
    inode->i_sb->s_op->write_inode(inode);
    inode->i_dirt = 0;
  }
  
  /* put inode */
  if (!inode->i_ref) {
    /* remove inode from list */
    list_del(&inode->i_list);

    /* delete inode */
    if (!inode->i_nlinks && op && op->delete_inode)
      op->delete_inode(inode);

    /* put inode */
    if (op && op->put_inode)
      op->put_inode(inode);
  }
}

