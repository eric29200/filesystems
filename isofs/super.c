#include <stdlib.h>
#include <errno.h>
#include <math.h>

#include "isofs.h"

/*
 * IOSFS super operations.
 */
struct super_operations_t isofs_sops = {
	.alloc_inode			= isofs_alloc_inode,
	.put_inode			= isofs_put_inode,
	.read_inode			= isofs_read_inode,
	.put_super			= isofs_put_super,
	.statfs				= isofs_statfs,
};

/*
 * Read a ISOFS super block.
 */
int isofs_read_super(struct super_block_t *sb, void *data)
{
	struct iso_directory_record_t *root_dir;
	struct iso_primary_descriptor_t *pri;
	struct iso_volume_descriptor_t *vdp;
	struct isofs_sb_info_t *sbi;
	struct buffer_head_t *sbh;
	int block, err = -EINVAL;

	/* allocate ISOFS super block */
	sb->s_fs_info = sbi = (struct isofs_sb_info_t *) malloc(sizeof(struct isofs_sb_info_t));
	if (!sbi)
		return -ENOMEM;

	/* set block size */
	sb->s_blocksize = ISOFS_BLOCK_SIZE;
	sb->s_blocksize_bits = log2(sb->s_blocksize);

	/* try to find volume descriptor */
	for (block = 16, pri = NULL; block < 100; block++) {
		/* read next block */
		sbh = sb_bread(sb, block);
		if (!sbh)
			goto out_primary_vol;

		/* get volume descriptor */
		vdp = (struct iso_volume_descriptor_t *) sbh->b_data;

		/* check volume id */
		if (strncmp(vdp->id, "CD001", sizeof(vdp->id)) == 0) {
			/* check volume type */
			if (isofs_num711(vdp->type) != ISOFS_VD_PRIMARY)
				goto out_primary_vol;

			/* get primary descriptor */
			pri = (struct iso_primary_descriptor_t *) vdp;
			break;
		}

		/* release super block buffer */
		brelse(sbh);
	}

	/* no primary volume */
	if (!pri)
		goto out_primary_vol;

	/* check if file system is not multi volumes */
	if (isofs_num723(pri->volume_set_size) != 1)
		goto out_multivol;

	/* get root directory record */
	root_dir = (struct iso_directory_record_t *) pri->root_directory_record;

	/* set super block */
	sbi->s_nzones = isofs_num733(pri->volume_space_size);
	sbi->s_log_zone_size = log2(isofs_num723(pri->logical_block_size));
	sbi->s_max_size = isofs_num733(pri->volume_space_size);
	sbi->s_ninodes = 0;
	sbi->s_firstdatazone = (isofs_num733(root_dir->extent) + isofs_num711(root_dir->ext_attr_length)) << sbi->s_log_zone_size;
	sb->s_magic = ISOFS_MAGIC;
	sb->s_root_inode = NULL;
	sb->s_op = &isofs_sops;

	/* release super block buffer */
	brelse(sbh);

	/* get root inode */
	sb->s_root_inode = vfs_iget(sb, sbi->s_firstdatazone);
	if (!sb->s_root_inode)
		goto out_root_inode;

	return 0;
out_root_inode:
	fprintf(stderr, "ISOFS : can't get root inode\n");
	goto out;
out_multivol:
	fprintf(stderr, "ISOFS : multi volume disks not supported\n");
	goto out_release_sb;
out_primary_vol:
	fprintf(stderr, "ISOFS : can't find primary volume descriptor\n");
	goto out_release_sb;
out_release_sb:
	brelse(sbh);
out:
	free(sbi);
	return err;
}

/*
 * Release a ISOFS super block.
 */
void isofs_put_super(struct super_block_t *sb)
{
	struct isofs_sb_info_t *sbi = isofs_sb(sb);

	/* release root inode */
	vfs_iput(sb->s_root_inode);

	/* free in memory super block */
	free(sbi);
}

/*
 * Get ISOFS file system status.
 */
int isofs_statfs(struct super_block_t *sb, struct statfs *buf)
{
	struct isofs_sb_info_t *sbi = isofs_sb(sb);

	buf->f_type = sb->s_magic;
	buf->f_bsize = sb->s_blocksize;
	buf->f_blocks = sbi->s_nzones << (sbi->s_log_zone_size - sb->s_blocksize_bits);
	buf->f_bfree = 0;
	buf->f_bavail = 0;
	buf->f_files = sbi->s_ninodes;
	buf->f_ffree = 0;
	buf->f_namelen = ISOFS_MAX_NAME_LEN;

	return 0;
}
