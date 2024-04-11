#ifndef _VFS_H_
#define _VFS_H_

#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/vfs.h>

#include "../lib/list.h"
#include "../lib/htable.h"

#define VFS_MINIX_TYPE					1
#define VFS_BFS_TYPE					2
#define VFS_EXT2_TYPE					3
#define VFS_ISOFS_TYPE					4
#define VFS_MEMFS_TYPE					5
#define VFS_FTPFS_TYPE					6
#define VFS_TARFS_TYPE					7

#define VFS_BUFFER_HTABLE_BITS				12
#define VFS_NR_BUFFER					(1 << VFS_BUFFER_HTABLE_BITS)

#define VFS_INODE_HTABLE_BITS				12
#define VFS_NR_INODE					(1 << VFS_INODE_HTABLE_BITS)

#define container_of(ptr, type, member)			({void *__mptr = (void *)(ptr);				\
							((type *)(__mptr - offsetof(type, member))); })

#define BITMAP_SET(map, i)				((map)[(i) / 8] |= (0x1 << ((i) % 8)))
#define BITMAP_CLR(map, i)				((map)[(i) / 8] &= ~(0x1 << ((i) % 8)))

#define ALIGN_UP(x, size)				(((x) + (size) - 1) & (~((size) - 1)))

/*
 * Block buffer.
 */
struct buffer_head {
	uint32_t				b_block;		/* block number */
	char *					b_data;			/* data buffer */
	size_t					b_size;			/* buffer block size */
	int					b_ref;			/* reference counter */
	char					b_dirt;			/* dirty flag */
	char					b_uptodate;		/* up to date flag */
	struct super_block *			b_sb;			/* super block of device */
	struct list_head			b_list;			/* global blocks linked list */
	struct htable_link			b_htable;		/* global blocks hash table */
};

/*
 * Generic super block.
 */
struct super_block {
	char *					s_dev;			/* device path */
	int					s_fd;			/* device file descriptor */
	uint16_t				s_blocksize;		/* block size in byte */
	uint8_t					s_blocksize_bits;	/* block size in bit (log2) */
	uint16_t				s_magic;		/* magic number */
	void *					s_fs_info;		/* specific file system informations */
	struct inode *				s_root_inode;		/* root inode */
	struct super_operations *		s_op;			/* super block operations */
};

/*
 * Generic inode.
 */
struct inode {
	mode_t					i_mode;			/* file mode */
	uint16_t				i_nlinks;		/* number of links to this file */
	uid_t					i_uid;			/* user id */
	gid_t					i_gid;			/* group id */
	ssize_t					i_size;			/* file size in byte */
	uint32_t				i_blocks;		/* number of blocks */
	struct timespec				i_atime;		/* last access time */
	struct timespec				i_mtime;		/* last modification time */
	struct timespec				i_ctime;		/* creation time */
	ino_t					i_ino;			/* inode number */
	struct super_block *			i_sb;			/* super block */
	int					i_ref;			/* reference counter */
	char					i_dirt;			/* dirty flag */
	struct inode_operations *		i_op;			/* inode operations */
	struct htable_link			i_htable;		/* global inodes hash table */
};

/*
 * Generic directory entry.
 */
struct dirent64 {
	uint64_t				d_inode;		/* inode number */
	int64_t					d_off;			/* offset to next directory entry */
	uint16_t				d_reclen;		/* length of this struct */
	uint8_t					d_type;			/* file type */
	char					d_name[];		/* file name */
};

/*
 * Generic file.
 */
struct file {
	mode_t					f_mode;			/* file mode */
	int					f_flags;		/* file flags */
	size_t					f_pos;			/* file position */
	int					f_ref;			/* reference counter */
	void *					f_private;		/* private data */
	struct inode *				f_inode;		/* inode */
	struct file_operations *		f_op;			/* file operations */
};

/*
 * Super block operations.
 */
struct super_operations {
	struct inode *(*alloc_inode)(struct super_block *);
	void (*put_inode)(struct inode *);
	void (*delete_inode)(struct inode *);
	int (*read_inode)(struct inode *);
	int (*write_inode)(struct inode *);
	void (*put_super)(struct super_block *);
	int (*statfs)(struct super_block *, struct statfs *);
};

/*
 * Inode operations.
 */
struct inode_operations {
	struct file_operations *fops;
	int (*lookup)(struct inode *, const char *, size_t, struct inode **);
	int (*create)(struct inode *, const char *, size_t, mode_t, struct inode **);
	int (*follow_link)(struct inode *, struct inode *, struct inode **);
	ssize_t (*readlink)(struct inode *, char *, size_t);
	int (*link)(struct inode *, struct inode *, const char *, size_t);
	int (*unlink)(struct inode *, const char *, size_t);
	int (*symlink)(struct inode *, const char *, size_t, const char *);
	int (*mkdir)(struct inode *, const char *, size_t, mode_t);
	int (*rmdir)(struct inode *, const char *, size_t);
	int (*rename)(struct inode *, const char *, size_t, struct inode *, const char *, size_t);
	void (*truncate)(struct inode *);
};

/*
 * File operations.
 */
struct file_operations {
	int (*open)(struct file *);
	int (*close)(struct file *);
	int (*read)(struct file *, char *, int);
	int (*write)(struct file *, const char *, int);
	int (*getdents64)(struct file *, void *, size_t);
};

/* VFS block buffer protoypes */
struct buffer_head *sb_bread(struct super_block *sb, uint32_t block);
int bwrite(struct buffer_head *bh);
void brelse(struct buffer_head *bh);

/* VFS inode prototypes */
struct inode *vfs_get_empty_inode(struct super_block *sb);
struct inode *vfs_iget(struct super_block *sb, ino_t ino);
void vfs_iput(struct inode *inode);
void vfs_ihash(struct inode *inode);

/* VFS name resolution prototypes */
struct inode *vfs_namei(struct inode *root, struct inode *base, const char *pathname, int follow_links);
int vfs_open_namei(struct inode *root, const char *pathname, int flags, mode_t mode, struct inode **res_inode);

/* VFS system calls */
int vfs_init();
int vfs_binit();
int vfs_iinit();
struct super_block *vfs_mount(const char *dev, int fs_type, void *data);
int vfs_umount(struct super_block *sb);
int vfs_statfs(struct super_block *sb, struct statfs *buf);
int vfs_create(struct inode *root, const char *pathname, mode_t mode);
int vfs_unlink(struct inode *root, const char *pathname);
int vfs_mkdir(struct inode *root, const char *pathname, mode_t mode);
int vfs_rmdir(struct inode *root, const char *pathname);
int vfs_link(struct inode *root, const char *oldpath, const char *new_path);
int vfs_symlink(struct inode *root, const char *target, const char *linkpath);
int vfs_rename(struct inode *root, const char *oldpath, const char *newpath);
ssize_t vfs_readlink(struct inode *root, const char *pathname, char *buf, size_t bufsize);
int vfs_stat(struct inode *root, const char *filename, struct stat *statbuf);
int vfs_access(struct inode *root, const char *pathname, int flags);
int vfs_chmod(struct inode *root, const char *pathname, mode_t mode);
int vfs_chown(struct inode *root, const char *pathname, uid_t uid, gid_t gid);
int vfs_utimens(struct inode *root, const char *pathname, const struct timespec times[2], int flags);
struct file *vfs_open(struct inode *root, const char *pathname, int flags, mode_t mode);
int vfs_close(struct file *filp);
ssize_t vfs_read(struct file *filp, char *buf, int count);
ssize_t vfs_write(struct file *filp, const char *buf, int count);
off_t vfs_lseek(struct file *filp, off_t offset, int whence);
int vfs_getdents64(struct file *filp, void *dirp, size_t count);
int vfs_truncate(struct inode *root, const char *pathname, off_t length);

/*
 * Get current time.
 */
static inline struct timespec current_time()
{
	struct timespec now;
	clock_gettime(CLOCK_REALTIME, &now);
	return now;
}

#endif
