#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stddef.h>
#include <sys/types.h>

#include "vfs.h"

/* global buffer table */
static struct buffer_head_t *buffer_table = NULL;
static struct htable_link_t **buffer_htable = NULL;
static LIST_HEAD(lru_buffers);

/*
 * Get an empty buffer.
 */
static struct buffer_head_t *get_empty_buffer(struct super_block_t *sb)
{
	struct buffer_head_t *bh;
	struct list_head_t *pos;

	/* get first free entry from LRU list */
	list_for_each(pos, &lru_buffers) {
		bh = list_entry(pos, struct buffer_head_t, b_list);
		if (!bh->b_ref)
			goto found;
	}

	/* no free buffer : exit */
	return NULL;

found:
	/* write it on disk if needed */
	if (bh->b_dirt && bwrite(bh))
		fprintf(stderr, "VFS: can't write block %d on disk\n", bh->b_block);

	/* allocate data if needed */
	if (!bh->b_size)
		bh->b_data = (char *) malloc(sb->s_blocksize);
	else if (bh->b_size < sb->s_blocksize)
		bh->b_data = (char *) realloc(bh->b_data, sb->s_blocksize);

	/* check allocated data */
	if (!bh->b_data)
		return NULL;

	/* reset buffer */
	memset(bh->b_data, 0, bh->b_size);
	bh->b_ref = 1;
	bh->b_size = sb->s_blocksize;
	bh->b_sb = sb;

	return bh;
}

/*
 * Get a buffer (from cache or create one).
 */
struct buffer_head_t *getblk(struct super_block_t *sb, uint32_t block)
{
	struct htable_link_t *node;
	struct buffer_head_t *bh;

	/* try to find buffer in cache */
	node = htable_lookup32(buffer_htable, block, VFS_BUFFER_HTABLE_BITS);
	while (node) {
		bh = htable_entry(node, struct buffer_head_t, b_htable);
		if (bh->b_block == block && bh->b_sb == sb && bh->b_size == sb->s_blocksize) {
			bh->b_ref++;
			goto out;
		}

		node = node->next;
	}

	/* get an empty buffer */
	bh = get_empty_buffer(sb);
	if (!bh)
		return NULL;

	/* set buffer */
	bh->b_block = block;
	bh->b_uptodate = 0;

	/* hash the new buffer */
	htable_delete(&bh->b_htable);
	htable_insert32(buffer_htable, &bh->b_htable, block, VFS_BUFFER_HTABLE_BITS);
out:
	/* put it at the end of LRU list */
	list_del(&bh->b_list);
	list_add_tail(&bh->b_list, &lru_buffers);
	return bh;
}

/*
 * Read a block buffer.
 */
struct buffer_head_t *sb_bread(struct super_block_t *sb, uint32_t block)
{
	struct buffer_head_t *bh;

	/* get block buffer */
	bh = getblk(sb, block);
	if (!bh)
		return NULL;

	/* buffer up to date : just return it */
	if (bh->b_uptodate)
		return bh;

	/* seek to block */
	if (lseek(sb->s_fd, block * sb->s_blocksize, SEEK_SET) == -1)
		goto err;

	/* read block */
	if (read(sb->s_fd, bh->b_data, sb->s_blocksize) != sb->s_blocksize)
		goto err;

	bh->b_uptodate = 1;
	return bh;
err:
	bh->b_ref = 0;
	free(bh->b_data);
	return NULL;
}

/*
 * Write a block buffer on disk.
 */
int bwrite(struct buffer_head_t *bh)
{
	if (!bh)
		return -EINVAL;

	/* seek to block */
	if (lseek(bh->b_sb->s_fd, bh->b_block * bh->b_sb->s_blocksize, SEEK_SET) == -1)
		return -EIO;

	/* write block */
	if (write(bh->b_sb->s_fd, bh->b_data, bh->b_sb->s_blocksize) != bh->b_sb->s_blocksize)
		return -EIO;

	/* mark buffer clear */
	bh->b_dirt = 0;

	return 0;
}

/*
 * Release a block buffer.
 */
void brelse(struct buffer_head_t *bh)
{
	if (!bh) 
		return;

	/* write it on disk if needed */
	if (bh->b_dirt)
		bwrite(bh);

	/* update reference count */
	bh->b_ref--;
}

/*
 * Init block buffers.
 */
int vfs_binit()
{
	int i;

	/* allocate buffers */
	buffer_table = (struct buffer_head_t *) malloc(sizeof(struct buffer_head_t) * VFS_NR_BUFFER);
	if (!buffer_table)
		return -ENOMEM;

	/* memzero all buffers */
	memset(buffer_table, 0, sizeof(struct buffer_head_t) * VFS_NR_BUFFER);

	/* init Last Recently Used buffers list */
	INIT_LIST_HEAD(&lru_buffers);

	/* add all buffers to LRU list */
	for (i = 0; i < VFS_NR_BUFFER; i++)
		list_add(&buffer_table[i].b_list, &lru_buffers);

	/* allocate buffers hash table */
	buffer_htable = (struct htable_link_t **) malloc(sizeof(struct htable_link_t *) * VFS_NR_BUFFER);
	if (!buffer_htable) {
		free(buffer_table);
		return -ENOMEM;
	}

	/* init buffers hash table */
	htable_init(buffer_htable, VFS_BUFFER_HTABLE_BITS);

	return 0;
}
