#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "vfs.h"

/* global inodes hash table */
static struct htable_link **inode_htable = NULL;

/*
 * Insert an inode in gobal inode htable.
 */
void vfs_ihash(struct inode *inode)
{
	htable_insert64(inode_htable, &inode->i_htable, inode->i_ino, VFS_INODE_HTABLE_BITS);
}

/*
 * Get an empty inode.
 */
struct inode *vfs_get_empty_inode(struct super_block *sb)
{
	struct inode *inode;

	/* allocation inode not implemented */
	if (!sb->s_op || !sb->s_op->alloc_inode)
		return NULL;

	/* allocate new inode */
	inode = sb->s_op->alloc_inode(sb);
	if (!inode)
		return NULL;

	/* reset inode */
	memset(inode, 0, sizeof(struct inode));

	/* set new inode */
	inode->i_sb = sb;
	inode->i_ref = 1;

	return inode;
}

/*
 * Get an inode.
 */
struct inode *vfs_iget(struct super_block *sb, ino_t ino)
{
	struct htable_link *node;
	struct inode *inode;
	int err;

	/* try to find inode in cache */
	node = htable_lookup64(inode_htable, ino, VFS_INODE_HTABLE_BITS);
	while (node) {
		inode = htable_entry(node, struct inode, i_htable);
		if (inode->i_sb == sb && inode->i_ino == ino) {
			inode->i_ref++;
			return inode;
		}

		node = node->next;
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
	htable_insert64(inode_htable, &inode->i_htable, ino, VFS_INODE_HTABLE_BITS);

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
void vfs_iput(struct inode *inode)
{
	struct super_operations *op = NULL;

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
		/* delete inode */
		if (!inode->i_nlinks && op && op->delete_inode)
			op->delete_inode(inode);

		/* put inode */
		if (op && op->put_inode)
			op->put_inode(inode);
	}
}

/*
 * Init inodes.
 */
int vfs_iinit()
{
	/* allocate inodes hash table */
	inode_htable = (struct htable_link **) malloc(sizeof(struct htable_link *) * VFS_NR_INODE);
	if (!inode_htable)
		return -ENOMEM;

	/* init inodes hash table */
	htable_init(inode_htable, VFS_INODE_HTABLE_BITS);

	return 0;
}
