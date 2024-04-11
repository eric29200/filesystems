#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>

#include "ftpfs.h"

/*
 * FTPFS super operations.
 */
struct super_operations ftpfs_sops = {
	.alloc_inode		= ftpfs_alloc_inode,
	.put_inode		= ftpfs_put_inode,
	.delete_inode		= ftpfs_delete_inode,
	.read_inode		= NULL,
	.write_inode		= NULL,
	.put_super		= ftpfs_put_super,
	.statfs			= ftpfs_statfs,
};

/*
 * Create root inode.
 */
static struct inode *ftpfs_create_root_inode(struct super_block *sb)
{
	struct ftpfs_fattr fattr;

	/* set attributes */
	memset(fattr.name, 0, FTPFS_NAME_LEN);
	memset(fattr.link, 0, FTPFS_NAME_LEN);
	memset(&fattr.statbuf, 0, sizeof(struct stat));
	fattr.statbuf.st_mode = S_IFDIR | 0755;
	fattr.statbuf.st_nlink = 2;
	fattr.statbuf.st_size = 0;

	/* get inode */
	return ftpfs_iget(sb, NULL, &fattr);
}

/*
 * Read a FTPFS super block.
 */
int ftpfs_read_super(struct super_block *sb, void *data)
{
	struct ftp_param *params = (struct ftp_param *) data;
	struct ftpfs_sb_info *sbi;
	char *user, *passwd;
	int err = -ENOSPC;

	/* allocate FTPFS super block */
	sb->s_fs_info = sbi = (struct ftpfs_sb_info *) malloc(sizeof(struct ftpfs_sb_info));
	if (!sbi)
		return -ENOMEM;

	/* create inodes cache hash table */
	sbi->s_inodes_cache_htable = (struct htable_link **) malloc(sizeof(struct htable_link *) * FTPFS_INODE_HTABLE_SIZE);
	if (!sbi->s_inodes_cache_htable)
		goto err_inodes_cache;

	/* init inodes cache */
	INIT_LIST_HEAD(&sbi->s_inodes_cache_list);
	htable_init(sbi->s_inodes_cache_htable, FTPFS_INODE_HTABLE_BITS);
	sbi->s_inodes_cache_size = 0;

	/* set super block */
	sb->s_blocksize_bits = 0;
	sb->s_blocksize = 0;
	sb->s_magic = FTPFS_MAGIC;
	sb->s_op = &ftpfs_sops;
	sb->s_root_inode = NULL;

	/* get user */
	if (params)
		user = params->user;
	else
		user = FTPFS_USER_DEFAULT;

	/* get password */
	if (params)
		passwd = params->passwd;
	else
		passwd = FTPFS_PASWD_DEFAULT;

	/* connect to server */
	sb->s_fd = ftp_connect(sb->s_dev, user, passwd, &sbi->s_addr);
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
	goto err;
err_inodes_cache:
	fprintf(stderr, "FTPFS : can't create inodes cache\n");
err:
	free(sbi);
	return err;
}

/*
 * Release a FTPFS super block.
 */
void ftpfs_put_super(struct super_block *sb)
{
	struct ftpfs_sb_info *sbi = ftpfs_sb(sb);

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
int ftpfs_statfs(struct super_block *sb, struct statfs *buf)
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
