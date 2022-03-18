#ifndef _VFS_H_
#define _VFS_H_

#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/vfs.h>

#include "../lib/list.h"
#include "../lib/htable.h"

#define VFS_MINIX_TYPE                        1
#define VFS_BFS_TYPE                          2
#define VFS_EXT2_TYPE                         3
#define VFS_ISOFS_TYPE                        4
#define VFS_MEMFS_TYPE                        5
#define VFS_FTPFS_TYPE                        6

#define VFS_BUFFER_HTABLE_BITS                12
#define VFS_NR_BUFFER                         (1 << VFS_BUFFER_HTABLE_BITS)

#define container_of(ptr, type, member)       ({void *__mptr = (void *)(ptr);                   \
                                              ((type *)(__mptr - offsetof(type, member))); })

#define BITMAP_SET(map, i)                    ((map)[(i) / 8] |= (0x1 << ((i) % 8)))
#define BITMAP_CLR(map, i)                    ((map)[(i) / 8] &= ~(0x1 << ((i) % 8)))


/*
 * Block buffer.
 */
struct buffer_head_t {
  uint32_t                  b_block;          /* block number */
  char                      *b_data;          /* data buffer */
  size_t                    b_size;           /* buffer block size */
  int                       b_ref;            /* reference counter */
  char                      b_dirt;           /* dirty flag */
  char                      b_uptodate;       /* up to date flag */
  struct super_block_t      *b_sb;            /* super block of device */
  struct list_head_t        b_list;           /* global blocks linked list */
  struct htable_link_t      b_htable;         /* global blocks hash table */
};

/*
 * Generic super block.
 */
struct super_block_t {
  char                      *s_dev;               /* device path */
  int                       s_fd;                 /* device file descriptor */
  uint16_t                  s_blocksize;          /* block size in byte */
  uint8_t                   s_blocksize_bits;     /* block size in bit (log2) */
  uint16_t                  s_magic;              /* magic number */
  void                      *s_fs_info;           /* specific file system informations */
  struct inode_t            *s_root_inode;        /* root inode */
  struct super_operations_t *s_op;                /* super block operations */
};

/*
 * Generic inode.
 */
struct inode_t {
  mode_t                    i_mode;               /* file mode */
  uint16_t                  i_nlinks;             /* number of links to this file */
  uid_t                     i_uid;                /* user id */
  gid_t                     i_gid;                /* group id */
  ssize_t                   i_size;               /* file size in byte */
  uint32_t                  i_blocks;             /* number of blocks */
  struct timespec           i_atime;              /* last access time */
  struct timespec           i_mtime;              /* last modification time */
  struct timespec           i_ctime;              /* creation time */
  ino_t                     i_ino;                /* inode number */
  struct super_block_t      *i_sb;                /* super block */
  int                       i_ref;                /* reference counter */
  char                      i_dirt;               /* dirty flag */
  struct inode_operations_t *i_op;                /* inode operations */
  struct list_head_t        i_list;               /* global inodes linked list */
};

/*
 * Generic directory entry.
 */
struct dirent64_t {
  uint64_t                  d_inode;              /* inode number */
  int64_t                   d_off;                /* offset to next directory entry */
  uint16_t                  d_reclen;             /* length of this struct */
  uint8_t                   d_type;               /* file type */
  char                      d_name[];             /* file name */
};

/*
 * Generic file.
 */
struct file_t {
  mode_t                    f_mode;               /* file mode */
  int                       f_flags;              /* file flags */
  size_t                    f_pos;                /* file position */
  int                       f_ref;                /* reference counter */
  struct inode_t            *f_inode;             /* inode */
  struct file_operations_t  *f_op;                /* file operations */
};

/*
 * Super block operations.
 */
struct super_operations_t {
  struct inode_t *(*alloc_inode)(struct super_block_t *);
  void (*put_inode)(struct inode_t *);
  void (*delete_inode)(struct inode_t *);
  int (*read_inode)(struct inode_t *);
  int (*write_inode)(struct inode_t *);
  void (*put_super)(struct super_block_t *);
  int (*statfs)(struct super_block_t *, struct statfs *);
};

/*
 * Inode operations.
 */
struct inode_operations_t {
  struct file_operations_t *fops;
  int (*lookup)(struct inode_t *, const char *, size_t, struct inode_t **);
  int (*create)(struct inode_t *, const char *, size_t, mode_t, struct inode_t **);
  int (*follow_link)(struct inode_t *, struct inode_t *, struct inode_t **);
  ssize_t (*readlink)(struct inode_t *, char *, size_t);
  int (*link)(struct inode_t *, struct inode_t *, const char *, size_t);
  int (*unlink)(struct inode_t *, const char *, size_t);
  int (*symlink)(struct inode_t *, const char *, size_t, const char *);
  int (*mkdir)(struct inode_t *, const char *, size_t, mode_t);
  int (*rmdir)(struct inode_t *, const char *, size_t);
  int (*rename)(struct inode_t *, const char *, size_t, struct inode_t *, const char *, size_t);
  void (*truncate)(struct inode_t *);
};

/*
 * File operations.
 */
struct file_operations_t {
  int (*open)(struct file_t *);
  int (*close)(struct file_t *);
  int (*read)(struct file_t *, char *, int);
  int (*write)(struct file_t *, const char *, int);
  int (*getdents64)(struct file_t *, void *, size_t);
};

/* VFS block buffer protoypes */
struct buffer_head_t *sb_bread(struct super_block_t *sb, uint32_t block);
int bwrite(struct buffer_head_t *bh);
void brelse(struct buffer_head_t *bh);

/* VFS inode prototypes */
struct inode_t *vfs_get_empty_inode(struct super_block_t *sb);
struct inode_t *vfs_iget(struct super_block_t *sb, ino_t ino);
void vfs_iput(struct inode_t *inode);

/* VFS name resolution prototypes */
struct inode_t *vfs_namei(struct inode_t *root, struct inode_t *base, const char *pathname, int follow_links);
int vfs_open_namei(struct inode_t *root, const char *pathname, int flags, mode_t mode, struct inode_t **res_inode);

/* VFS system calls */
int vfs_init();
int vfs_binit();
struct super_block_t *vfs_mount(const char *dev, int fs_type);
int vfs_umount(struct super_block_t *sb);
int vfs_statfs(struct super_block_t *sb, struct statfs *buf);
int vfs_create(struct inode_t *root, const char *pathname, mode_t mode);
int vfs_unlink(struct inode_t *root, const char *pathname);
int vfs_mkdir(struct inode_t *root, const char *pathname, mode_t mode);
int vfs_rmdir(struct inode_t *root, const char *pathname);
int vfs_link(struct inode_t *root, const char *oldpath, const char *new_path);
int vfs_symlink(struct inode_t *root, const char *target, const char *linkpath);
int vfs_rename(struct inode_t *root, const char *oldpath, const char *newpath);
ssize_t vfs_readlink(struct inode_t *root, const char *pathname, char *buf, size_t bufsize);
int vfs_stat(struct inode_t *root, const char *filename, struct stat *statbuf);
int vfs_access(struct inode_t *root, const char *pathname, int flags);
int vfs_chmod(struct inode_t *root, const char *pathname, mode_t mode);
int vfs_chown(struct inode_t *root, const char *pathname, uid_t uid, gid_t gid);
int vfs_utimens(struct inode_t *root, const char *pathname, const struct timespec times[2], int flags);
struct file_t *vfs_open(struct inode_t *root, const char *pathname, int flags, mode_t mode);
int vfs_close(struct file_t *filp);
ssize_t vfs_read(struct file_t *filp, char *buf, int count);
ssize_t vfs_write(struct file_t *filp, const char *buf, int count);
off_t vfs_lseek(struct file_t *filp, off_t offset, int whence);
int vfs_getdents64(struct file_t *filp, void *dirp, size_t count);
int vfs_truncate(struct inode_t *root, const char *pathname, off_t length);

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
