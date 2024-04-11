#ifndef _TARFS_H_
#define _TARFS_H_

#include "../vfs/vfs.h"

#define TARFS_BLOCK_SIZE_BITS			9
#define TARFS_BLOCK_SIZE			(1 << TARFS_BLOCK_SIZE_BITS)

#define TARFS_MAGIC_STR				"ustar "
#define TARFS_MAGIC				0xAFAF

#define TARFS_ROOT_INO				0

#define TAR_REGTYPE				'0'
#define TAR_AREGTYPE				'\0'
#define TAR_LNKTYPE				'1'
#define TAR_SYMTYPE				'2'
#define TAR_CHRTYPE				'3'
#define TAR_BLKTYPE				'4'
#define TAR_DIRTYPE				'5'
#define TAR_FIFOTYPE				'6'
#define TAR_CONTTYPE				'7'
#define TAR_LONGNAME				'L'
#define TAR_LONGLINK				'K'

/*
 * TAR header.
 */
struct tar_header {
	char 				name[100];
	char 				mode[8];
	char 				uid[8];
	char 				gid[8];
	char 				size[12];
	char 				mtime[12];
	char 				chksum[8];
	char 				typeflag;
	char 				linkname[100];
	char 				magic[6];
	char 				version[2];
	char 				uname[32];
	char 				gname[32];
	char			 	devmajor[8];
	char 				devminor[8];
	char 				prefix[131];
	char 				atime[12];
	char 				ctime[12];
};

/*
 * TAR entry.
 */
struct tar_entry {
	char *				name;
	char *				linkname;
	off_t				data_off;
	size_t				data_len;
	mode_t				mode;
	uid_t				uid;
	gid_t				gid;
	struct timespec			atime;
	struct timespec			mtime;
	struct timespec			ctime;
	ino_t				ino;
	struct list_head		children;
	struct list_head		list;
	struct tar_entry *		parent;
};

/*
 * TarFS in memory super block.
 */
struct tarfs_sb_info {
	struct tar_entry *		s_root_entry;		/* root TAR entry */
	struct tar_entry **		s_tar_entries;		/* TAR entries */
	ino_t				s_ninodes;		/* number of inodes */
};

/*
 * TarFS in memory inode.
 */
struct tarfs_inode_info {
	struct tar_entry *		entry;			/* TAR entry */
	struct inode			vfs_inode;		/* VFS inode */
};

/* TarFS file system operations */
extern struct super_operations tarfs_sops;
extern struct inode_operations tarfs_file_iops;
extern struct inode_operations tarfs_symlink_iops;
extern struct inode_operations tarfs_dir_iops;
extern struct file_operations tarfs_file_fops;
extern struct file_operations tarfs_dir_fops;

/* Tar lib prototypes */
int tar_create(struct super_block *sb);
void tar_free(struct tar_entry *entry);
void tar_index(struct super_block *sb, struct tar_entry *entry);

/* TarFS super operations */
int tarfs_read_super(struct super_block *sb, void *data);
void tarfs_put_super(struct super_block *sb);
int tarfs_statfs(struct super_block *sb, struct statfs *buf);

/* TarFS inode prototypes */
struct inode *tarfs_alloc_inode(struct super_block *sb);
int tarfs_read_inode(struct inode *inode);
void tarfs_put_inode(struct inode *inode);

/* TarFS symlink prototypes */
int tarfs_follow_link(struct inode *dir, struct inode *inode, struct inode **res_inode);
ssize_t tarfs_readlink(struct inode *inode, char *buf, size_t bufsize);

/* TarFS names resolution prototypes */
int tarfs_lookup(struct inode *dir, const char *name, size_t name_len, struct inode **res_inode);

/* TarFS file prototypes */
int tarfs_file_read(struct file *filp, char *buf, int count);
int tarfs_getdents64(struct file *filp, void *dirp, size_t count);

/*
 * Get TarFS in memory super block from generic super block.
 */
static inline struct tarfs_sb_info *tarfs_sb(struct super_block *sb)
{
	return sb->s_fs_info;
}

/*
 * Get TarFS in memory inode from generic inode.
 */
static inline struct tarfs_inode_info *tarfs_i(struct inode *inode)
{
	return container_of(inode, struct tarfs_inode_info, vfs_inode);
}

#endif
