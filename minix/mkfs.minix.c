#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "minix.h"

#define DEFAULTS_FS_VERSION				1
#define MINIX_MAX_INODES				65535
#define BITS_PER_BLOCK					(MINIX_BLOCK_SIZE << 3)

#define UPPER(size, n)					((size + ((n) - 1)) / (n))
#define MINIX_INODES_PER_BLOCK				((MINIX_BLOCK_SIZE) / sizeof(struct minix1_inode))
#define MINIX2_INODES_PER_BLOCK		 		((MINIX_BLOCK_SIZE) / sizeof(struct minix2_inode))

/* global variables */
static int fs_version = DEFAULTS_FS_VERSION;
static int fs_namelen = 30;
static int fs_dirsize = 0;
static int fs_inodes = 0;
static char *fs_dev_name = NULL;
static int fs_dev_fd = -1;
static uint64_t fs_blocks = 0;
static int fs_itable_blocks;
static int fs_imap_blocks;
static int fs_zmap_blocks;
static int fs_firstdatazone;
static char sb_block[MINIX_BLOCK_SIZE];
static char root_block[MINIX_BLOCK_SIZE];
static char *imap = NULL;
static char *zmap = NULL;
static char *itable = NULL;

/*
 * Print usage.
 */
static void usage(char *prog_name)
{
	printf("%s [options] <device> [blocks]\n", prog_name);
	printf("\n");
	printf("Options :\n");
	printf(" -1				use minix version 1\n");
	printf(" -2				use minix version 2\n");
	printf(" -3				use minix version 3\n");
	printf(" -n, --namelength <num>		maximum length of filenames\n");
	printf(" -i, --inodes <num>		number of inodes\n");
	exit(0);
}

/*
 * Parse user options.
 */
static int parse_options(int argc, char **argv)
{
	static const struct option opts[] = {
		{ "namelength",		required_argument,	NULL,	'n' },
		{ "inodes",		required_argument,	NULL,	'i' },
		{ "help",		no_argument,		NULL,	'h' },
		{ NULL,			0,			NULL,	0 }
	};
	int c;

	/* parse options */
	while ((c = getopt_long(argc, argv, "1v23n:i:h", opts, NULL)) != -1) {
		switch (c) {
			case '1':
				fs_version = 1;
				break;
			case '2':
				fs_version = 2;
				break;
			case '3':
				fs_namelen = 60;
				fs_version = 3;
				break;
			case 'n':
				fs_namelen = atoi(optarg);
				break;
			case 'i':
				fs_inodes = atoi(optarg);
				break;
			case 'h':
				usage(argv[0]);
				break;
			default:
				usage(argv[0]);
				break;
		}
	}

	/* get device name */
	argc -= optind;
	argv += optind;
	if (argc > 0) {
		fs_dev_name = argv[0];
		argc--;
		argv++;
	}

	/* get number of blocks */
	if (argc > 0)
		fs_blocks = atoll(argv[0]);

	/* no specified device */
	if (!fs_dev_name) {
		fprintf(stderr, "no device specified\n");
		return -EINVAL;
	}

	/* check user options */
	switch (fs_version) {
		case 1:
		case 2:
			if (fs_namelen == 14 || fs_namelen == 30) {
				fs_dirsize = fs_namelen + 2;
			} else {
				fprintf(stderr, "unsupported name length : %d\n", fs_namelen);
				return -EINVAL;
			}

			break;
		case 3:
			if (fs_namelen == 60) {
				fs_dirsize = fs_namelen + 4;
			} else {
				fprintf(stderr, "unsupported name length : %d\n", fs_namelen);
				return -EINVAL;
			}
			break;
		default:
			fprintf(stderr, "unsupported minix version : %d\n", fs_version);
			return -EINVAL;
	}

	return 0;
}

/*
 * Write a block on disk.
 */
static int write_block(int block, char *buffer)
{
	/* seek to block */
	if (lseek(fs_dev_fd, block * MINIX_BLOCK_SIZE, SEEK_SET) != block * MINIX_BLOCK_SIZE) {
		fprintf(stderr, "can't seek to block %d\n", block);
		return -ENOSPC;
	}

	/* write block on disk */
	if (write(fs_dev_fd, buffer, MINIX_BLOCK_SIZE) != MINIX_BLOCK_SIZE) {
		fprintf(stderr, "can't write block %d\n", block);
		return -ENOSPC;
	}

	return 0;
}


/*
 * Open device.
 */
int open_device()
{
	struct stat statbuf;
	off_t fs_dev_size;
	int err;

	/* get stats on device */
	err = stat(fs_dev_name, &statbuf);
	if (err) {
		err = errno;
		fprintf(stderr, "can't stat() device %s\n", fs_dev_name);
		return err;
	}

	/* get device size */
	fs_dev_size = statbuf.st_size;

	/* set and check number of blocks */
	if (!fs_blocks)
		fs_blocks = fs_dev_size / MINIX_BLOCK_SIZE;

	/* check number of blocks */
	if (fs_blocks > fs_dev_size / MINIX_BLOCK_SIZE) {
		fprintf(stderr, "%s : requested blocks > number of available blocks\n", fs_dev_name);
		return -EINVAL;
	} else if (fs_blocks < 10) {
		fprintf(stderr, "%s : number of blocks too small\n", fs_dev_name);
		return -EINVAL;
	}

	/* limit number of blocks */
	if (fs_version == 1 && fs_blocks > MINIX_MAX_INODES)
		fs_blocks = MINIX_MAX_INODES;
	if (fs_blocks > (4 + (MINIX_MAX_INODES - 4) * BITS_PER_BLOCK))
		fs_blocks = 4 + (MINIX_MAX_INODES - 4) * BITS_PER_BLOCK;

	/*
	 * Compute number of inodes :
	 * - 1 inode / 3 blocks for small device (> 2 GB)
	 * - 1 inode / 8 blocks for middle device (> 0.5 GB)
	 * - 1 inode / 16 blocks for large ones
	 */
	if (!fs_inodes) {
		if (fs_blocks > 2048 * 1024)
			fs_inodes = fs_blocks / 16;
		else if (fs_blocks > 512 * 1024)
			fs_inodes = fs_blocks / 8;
		else
			fs_inodes = fs_blocks / 3;
	}

	/* round up inodes number */
	if (fs_version == 1) {
		fs_inodes = (fs_inodes + MINIX_INODES_PER_BLOCK - 1) & ~(MINIX_INODES_PER_BLOCK - 1);
		fs_itable_blocks = UPPER(fs_inodes, MINIX_INODES_PER_BLOCK);
	} else {
		fs_inodes = (fs_inodes + MINIX2_INODES_PER_BLOCK - 1) & ~(MINIX2_INODES_PER_BLOCK - 1);
		fs_itable_blocks = UPPER(fs_inodes, MINIX2_INODES_PER_BLOCK);
	}

	/* compute imap and zmap blocks */
	fs_imap_blocks = UPPER(fs_inodes + 1, BITS_PER_BLOCK);
	fs_zmap_blocks = UPPER(fs_blocks - (1 + fs_imap_blocks + fs_itable_blocks), BITS_PER_BLOCK + 1);
	fs_firstdatazone = 2 + fs_imap_blocks + fs_zmap_blocks + fs_itable_blocks;

	/* open device */
	fs_dev_fd = open(fs_dev_name, O_RDWR);
	if (fs_dev_fd < 0) {
		err = errno;
		fprintf(stderr, "can't open() device %s\n", fs_dev_name);
		return err;
	}

	return 0;
}

/*
 * Set Super block.
 */
static int set_super_block()
{
	struct minix1_super_block *sb1;
	struct minix3_super_block *sb3;
	int i;

	/* reset super block */
	memset(sb_block, 0, MINIX_BLOCK_SIZE);

	/* get super block */
	sb1 = (struct minix1_super_block *) sb_block;
	sb3 = (struct minix3_super_block *) sb_block;

	/* set super block */
	switch (fs_version) {
		case 1:
			if (fs_namelen == 14)
				sb1->s_magic = MINIX1_MAGIC1;
			else
				sb1->s_magic = MINIX1_MAGIC2;
			sb1->s_log_zone_size = 0;
			sb1->s_max_size = (7 + 512 + 512 * 512) * MINIX_BLOCK_SIZE;
			sb1->s_nzones = fs_blocks;
			sb1->s_ninodes = fs_inodes;
			sb1->s_imap_blocks = fs_imap_blocks;
			sb1->s_zmap_blocks = fs_zmap_blocks;
			sb1->s_firstdatazone = fs_firstdatazone;
			sb1->s_state = MINIX_VALID_FS;
			break;
		case 2:
			if (fs_namelen == 14)
				sb1->s_magic = MINIX2_MAGIC1;
			else
				sb1->s_magic = MINIX2_MAGIC2;
			sb1->s_log_zone_size = 0;
			sb1->s_max_size = 0x7FFFFFFF;
			sb1->s_nzones = fs_blocks;
			sb1->s_ninodes = fs_inodes;
			sb1->s_imap_blocks = fs_imap_blocks;
			sb1->s_zmap_blocks = fs_zmap_blocks;
			sb1->s_firstdatazone = fs_firstdatazone;
			sb1->s_state = MINIX_VALID_FS;
			break;
		default:
			sb3->s_magic = MINIX3_MAGIC;
			sb3->s_log_zone_size = 0;
			sb3->s_blocksize = MINIX_BLOCK_SIZE;
			sb3->s_max_size = 2147483647L;
			sb3->s_zones = fs_blocks;
			sb3->s_ninodes = fs_inodes;
			sb3->s_imap_blocks = fs_imap_blocks;
			sb3->s_zmap_blocks = fs_zmap_blocks;
			sb3->s_firstdatazone = fs_firstdatazone;
			break;
	}

	/* allocate inodes bitmap */
	imap = (char *) malloc(fs_imap_blocks * MINIX_BLOCK_SIZE);
	if (!imap) {
		fprintf(stderr, "can't allocate inodes bitmap\n");
		return -ENOMEM;
	}

	/* allocate zones bitmap */
	zmap = (char *) malloc(fs_zmap_blocks * MINIX_BLOCK_SIZE);
	if (!zmap) {
		fprintf(stderr, "can't allocate zones bitmap\n");
		return -ENOMEM;
	}

	/* mark all inodes and all zones busy */
	memset(imap, 0xFF, fs_imap_blocks * MINIX_BLOCK_SIZE);
	memset(zmap, 0xFF, fs_zmap_blocks * MINIX_BLOCK_SIZE);

	/* mark data zones free */
	for (i = fs_firstdatazone; i < fs_blocks; i++)
		BITMAP_CLR(zmap, i);

	/* mark all inodes free (except first one : MINIX_ROOT_INODE = 1) */
	for (i = MINIX_ROOT_INODE; i <= fs_inodes; i++)
		BITMAP_CLR(imap, i);

	/* allocate inodes table */
	itable = (char *) malloc(fs_itable_blocks * MINIX_BLOCK_SIZE);
	if (!itable) {
		fprintf(stderr, "can't allocate inodes table\n");
		return -ENOMEM;
	}

	/* reset inodes table */
	memset(itable, 0, fs_itable_blocks * MINIX_BLOCK_SIZE);

	/* print informations */
	printf("%d inodes\n", fs_inodes);
	printf("%lu blocks\n", fs_blocks);
	printf("First datazone = %d\n", fs_firstdatazone);
	printf("Zone size = %d\n", MINIX_BLOCK_SIZE);
	printf("Max file size = %d\n", fs_version == 3 ? sb3->s_max_size : sb1->s_max_size);

	return 0;
}

/*
 * Create root inode v1.
 */
static int create_root_inode_v1()
{
	struct minix1_inode *inode = &((struct minix1_inode *) itable)[MINIX_ROOT_INODE - 1];
	int err;

	/* mark inode in bitmap */
	BITMAP_SET(imap, MINIX_ROOT_INODE);

	/* set root inode */
	inode->i_zone[0] = fs_firstdatazone;
	inode->i_nlinks = 2;
	inode->i_time = time(NULL);
	inode->i_size = 2 * fs_dirsize;
	inode->i_mode = S_IFDIR | 0755;
	inode->i_uid = getuid();
	if (inode->i_uid)
		inode->i_gid = getgid();

	/* write root block */
	err = write_block(inode->i_zone[0], root_block);
	if (err)
		return err;

	/* mark zone */
	BITMAP_SET(zmap, fs_firstdatazone);

	return 0;
}

/*
 * Create root inode v2.
 */
static int create_root_inode_v2()
{
	struct minix2_inode *inode = &((struct minix2_inode *) itable)[MINIX_ROOT_INODE - 1];
	int err;

	/* mark inode in bitmap */
	BITMAP_SET(imap, MINIX_ROOT_INODE);

	/* set root inode */
	inode->i_zone[0] = fs_firstdatazone;
	inode->i_nlinks = 2;
	inode->i_atime = inode->i_mtime = inode->i_ctime = time(NULL);
	inode->i_size = 2 * fs_dirsize;
	inode->i_mode = S_IFDIR | 0755;
	inode->i_uid = getuid();
	if (inode->i_uid)
		inode->i_gid = getgid();

	/* write root block */
	err = write_block(inode->i_zone[0], root_block);
	if (err)
		return err;

	/* mark zone */
	BITMAP_SET(zmap, fs_firstdatazone);

	return 0;
}

/*
 * Set root inode.
 */
static int set_root_inode()
{
	char *p = root_block;

	/* reset root block */
	memset(p, 0, MINIX_BLOCK_SIZE);

	/* add '.' and '..' entries */
	if (fs_version == 3) {
		*((uint32_t *) p) = MINIX_ROOT_INODE;
		strcpy(p + sizeof(uint32_t), ".");
		p += fs_dirsize;
		*((uint32_t *) p) = MINIX_ROOT_INODE;
		strcpy(p + sizeof(uint32_t), "..");
	} else {
		*((uint16_t *) p) = MINIX_ROOT_INODE;
		strcpy(p + sizeof(uint16_t), ".");
		p += fs_dirsize;
		*((uint16_t *) p) = MINIX_ROOT_INODE;
		strcpy(p + sizeof(uint16_t), ".");
	}

	/* create root inode */
	if (fs_version == 1)
		return create_root_inode_v1();
	return create_root_inode_v2();
}

/*
 * Write super block, inodes table and bitmaps on disk.
 */
int write_super_block_itable_bitmaps()
{
	int err, i, block;

	/* write super block */
	err = write_block(1, sb_block);
	if (err)
		return err;

	/* write inodes bitmap */
	for (i = 0, block = 2; i < fs_imap_blocks; i++, block++) {
		err = write_block(block, &imap[i * MINIX_BLOCK_SIZE]);
		if (err)
			return err;
	}

	/* write zones bitmap */
	for (i = 0; i < fs_zmap_blocks; i++, block++) {
		err = write_block(block, &zmap[i * MINIX_BLOCK_SIZE]);
		if (err)
			return err;
	}

	/* write inodes table */
	for (i = 0; i < fs_itable_blocks; i++, block++) {
		err = write_block(block, &itable[i * MINIX_BLOCK_SIZE]);
		if (err)
			return err;
	}

	return 0;
}

/*
 * Main.
 */
int main(int argc, char **argv)
{
	int err;

	/* parse options */
	err = parse_options(argc, argv);
	if (err)
		goto out;

	/* open device */
	err = open_device();
	if (err)
		goto out;

	/* set super block */
	err = set_super_block();
	if (err)
		goto out;

	/* set root inode */
	err = set_root_inode();
	if (err)
		goto out;

	/* write super blocks and bitmaps */
	err = write_super_block_itable_bitmaps();

out:
	/* free inodes map */
	if (imap)
		free(imap);

	/* free zones map */
	if (zmap)
		free(zmap);

	/* free inodes table */
	if (itable)
		free(itable);

	/* close device */
	if (fs_dev_fd > 0)
		close(fs_dev_fd);

	return err;
}

