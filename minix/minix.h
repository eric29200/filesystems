#ifndef _MINIX_H_
#define _MINIX_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/vfs.h>

#include "../vfs/vfs.h"

#define MINIX_V1				0x0001		/* Minix V1 */
#define MINIX_V2				0x0002		/* Minix V2 */
#define MINIX_V3				0x0003		/* Minix V3 */

#define MINIX1_MAGIC1				0x137F		/* Minix V1, 14 char names */
#define MINIX1_MAGIC2				0x138F		/* Minix V1, 30 char names */
#define MINIX2_MAGIC1				0x2468		/* Minix V2, 14 char names */
#define MINIX2_MAGIC2				0x2478		/* Minix V2, 14 char names */
#define MINIX3_MAGIC				0x4D5A		/* Minix V3, 60 char names */

#define MINIX_VALID_FS				0x0001
#define MINIX_ERROR_FS				0x0002

#define MINIX_BLOCK_SIZE_BITS	 		10
#define MINIX_BLOCK_SIZE			(1 << MINIX_BLOCK_SIZE_BITS)	/* = 1024 bytes */

#define MINIX_ROOT_INODE			1

/*
 * Minix in memory super block.
 */
struct minix_sb_info {
	uint32_t			s_ninodes;			/* number of inodes */
	uint32_t			s_nzones;			/* number of zones */
	uint16_t			s_imap_blocks;			/* number of inodes bitmap blocks */
	uint16_t			s_zmap_blocks;			/* number of zones bitmap blocks */
	uint16_t			s_firstdatazone;		/* first data zone */
	uint16_t			s_log_zone_size;		/* always 0 */
	uint16_t			s_version;			/* file system version */
	uint16_t			s_state;			/* file system state */
	int				s_name_len;			/* file name length */
	int				s_dirsize;			/* directory entry size */
	uint32_t			s_max_size;			/* maximum size of file */
	struct buffer_head *		s_sbh;				/* super block buffer */
	struct buffer_head **		s_imap;				/* inodes bitmap buffers */
	struct buffer_head **		s_zmap;				/* zones bitmap buffers */
};

/*
 * Minix V1/V2 super block.
 */
struct minix1_super_block {
	uint16_t			s_ninodes;			/* number of inodes */
	uint16_t			s_nzones;			/* number of zones */
	uint16_t			s_imap_blocks;			/* number of inodes bitmap blocks */
	uint16_t			s_zmap_blocks;			/* number of zones bitmap blocks */
	uint16_t			s_firstdatazone;		/* first data zone */
	uint16_t			s_log_zone_size;		/* always 0 */
	uint32_t			s_max_size;			/* maximum size of file */
	uint16_t			s_magic;			/* magic numer */
	uint16_t			s_state;			/* file system state */
	uint32_t			s_zones;			/* number of zones */
};

/*
 * Minix V3 super block.
 */
struct minix3_super_block {
	uint32_t			s_ninodes;			/* number of inodes */
	uint16_t			s_pad0;				/* padding */
	uint16_t			s_imap_blocks;			/* number of inodes bitmap blocks */
	uint16_t			s_zmap_blocks;			/* number of zones bitmap blocks */
	uint16_t			s_firstdatazone;		/* first data zone */
	uint16_t			s_log_zone_size;		/* always 0 */
	uint16_t			s_pad1;				/* padding */
	uint32_t			s_max_size;			/* maximum size of file */
	uint32_t			s_zones;			/* number of zones */
	uint16_t			s_magic;			/* magic number */
	uint16_t			s_pad2;				/* padding */
	uint16_t			s_blocksize;			/* block size */
	uint8_t			 	s_disk_version;			/* file system version */
};

/*
 * Minix in memory inode.
 */
struct minix_inode_info {
	uint32_t			i_zone[10];			/* data zones */
	struct inode			vfs_inode;			/* VFS inode */
};

/*
 * Minix V1 inode.
 */
struct minix1_inode {
	uint16_t			i_mode;				/* file mode */
	uint16_t			i_uid;				/* user id */
	uint32_t			i_size;				/* file size in bytes */
	uint32_t			i_time;				/* timestamp */
	uint8_t			 	i_gid;				/* group id */
	uint8_t			 	i_nlinks;			/* number of links to this file */
	uint16_t			i_zone[9];			/* data zones */
};

/*
 * Minix V2/V3 inode.
 */
struct minix2_inode {
	uint16_t			i_mode;				/* file mode */
	uint16_t			i_nlinks;			/* number of links to this file */
	uint16_t			i_uid;				/* user id */
	uint16_t			i_gid;				/* group id */
	uint32_t			i_size;				/* file size */
	uint32_t			i_atime;			/* last access time */
	uint32_t			i_mtime;			/* last modification time */
	uint32_t			i_ctime;			/* creation time */
	uint32_t			i_zone[10];			/* data zones */
};

/*
 * Minix V1/V2 directory entry.
 */
struct minix1_dir_entry {
	uint16_t			d_inode;			/* inode number */
	char				d_name[0];			/* file name */
};

/*
 * Minix V3 directory entry.
 */
struct minix3_dir_entry {
	uint32_t			d_inode;			/* inode number */
	char				d_name[0];			/* file name */
};

/* Minix file system operations */
extern struct super_operations minix_sops;
extern struct inode_operations minix_file_iops;
extern struct inode_operations minix_dir_iops;
extern struct file_operations minix_file_fops;
extern struct file_operations minix_dir_fops;

/* Minix super operations */
int minix_read_super(struct super_block *sb, void *data);
void minix_put_super(struct super_block *sb);
int minix_statfs(struct super_block *sb, struct statfs *buf);

/* Minix bitmap prototypes */
struct inode *minix_new_inode(struct super_block *sb);
uint32_t minix_new_block(struct super_block *sb);
int minix_free_inode(struct inode *inode);
int minix_free_block(struct super_block *sb, uint32_t block);
uint32_t minix_count_free_inodes(struct super_block *sb);
uint32_t minix_count_free_blocks(struct super_block *sb);

/* Minix inode prototypes */
struct buffer_head *minix_bread(struct inode *inode, uint32_t block, int create);
struct inode *minix_alloc_inode(struct super_block *sb);
void minix_put_inode(struct inode *inode);
void minix_delete_inode(struct inode *inode);
int minix_read_inode(struct inode *inode);
int minix_write_inode(struct inode *inode);

/* Minix names resolution prototypes */
int minix_lookup(struct inode *dir, const char *name, size_t name_len, struct inode **res_inode);
int minix_create(struct inode *dir, const char *name, size_t name_len, mode_t mode, struct inode **res_inode);
int minix_mkdir(struct inode *dir, const char *name, size_t name_len, mode_t mode);
int minix_unlink(struct inode *dir, const char *name, size_t name_len);
int minix_rmdir(struct inode *dir, const char *name, size_t name_len);
int minix_link(struct inode *old_inode, struct inode *dir, const char *name, size_t name_len);
int minix_symlink(struct inode *dir, const char *name, size_t name_len, const char *target);
int minix_rename(struct inode *old_dir, const char *old_name, size_t old_name_len, struct inode *new_dir, const char *new_name, size_t new_name_len);

/* Minix truncate prototypes */
void minix_truncate(struct inode *inode);

/* Minix symlink prototypes */
int minix_follow_link(struct inode *dir, struct inode *inode, struct inode **res_inode);
ssize_t minix_readlink(struct inode *inode, char *buf, size_t bufsize);

/* Minix file prototypes */
int minix_file_read(struct file *filp, char *buf, int count);
int minix_file_write(struct file *filp, const char *buf, int count);
int minix_getdents64(struct file *filp, void *dirp, size_t count);

/*
 * Get Minix in memory super block from generic super block.
 */
static inline struct minix_sb_info *minix_sb(struct super_block *sb)
{
	return sb->s_fs_info;
}

/*
 * Get Minix in memory inode from generic inode.
 */
static inline struct minix_inode_info *minix_i(struct inode *inode)
{
	return container_of(inode, struct minix_inode_info, vfs_inode);
}

#endif
