#include <stdlib.h>
#include <errno.h>

#include "memfs.h"

/*
 * MemFS super operations.
 */
struct super_operations memfs_sops = {
	.alloc_inode			= memfs_alloc_inode,
	.put_inode			= memfs_put_inode,
	.delete_inode			= memfs_delete_inode,
	.put_super			= memfs_put_super,
	.statfs				= memfs_statfs,
};

/*
 * Read a MemFS super block.
 */
int memfs_read_super(struct super_block *sb, void *data)
{
	struct memfs_sb_info *sbi;
	int err = -EINVAL;

	/* allocate MemFS super block */
	sb->s_fs_info = sbi = (struct memfs_sb_info *) malloc(sizeof(struct memfs_sb_info));
	if (!sbi)
		return -ENOMEM;

	/* set super block */
	sb->s_blocksize = 1;
	sb->s_blocksize_bits = 0;
	sb->s_magic = MEMFS_MAGIC;
	sb->s_op = &memfs_sops;
	memfs_sb(sb)->s_inodes_cpt = MEMFS_ROOT_INODE;
	memfs_sb(sb)->s_ninodes = 0;

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
void memfs_put_super(struct super_block *sb)
{
	struct memfs_sb_info *sbi = memfs_sb(sb);

	/* release root inode */
	vfs_iput(sb->s_root_inode);

	/* free in memory super block */
	free(sbi);
}

/*
 * Get MemFS File system status.
 */
int memfs_statfs(struct super_block *sb, struct statfs *buf)
{
	/* set stat buffer */
	buf->f_type = sb->s_magic;
	buf->f_bsize = sb->s_blocksize;
	buf->f_blocks = 0;
	buf->f_bfree = 0;
	buf->f_bavail = 0;
	buf->f_files = memfs_sb(sb)->s_ninodes;
	buf->f_ffree = 0;
	buf->f_namelen = MEMFS_NAME_LEN;

	return 0;
}
