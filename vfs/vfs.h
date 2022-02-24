#ifndef _VFS_H_
#define _VFS_H_

#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/vfs.h>

#include "../lib/list.h"
#include "../lib/htable.h"

#define VFS_MINIX_TYPE              1

#define VFS_BUFFER_HTABLE_BITS      12
#define VFS_NR_BUFFER               (1 << VFS_BUFFER_HTABLE_BITS)
#define VFS_NR_INODE                (1 << 12)

/*
 * Block buffer.
 */
struct buffer_head_t {
  uint32_t                  b_block;
  char                      *b_data;
  size_t                    b_size;
  int                       b_ref;
  char                      b_dirt;
  char                      b_uptodate;
  struct super_block_t      *b_sb;
  struct list_head_t        b_list;
  struct htable_link_t      b_htable;
};

/*
 * Generic super block.
 */
struct super_block_t {
  int                       s_fd;
  uint16_t                  s_blocksize;
  uint32_t                  s_max_size;
  uint16_t                  s_magic;
  void                      *s_fs_info;
  struct buffer_head_t      *sb_bh;
  struct inode_t            *root_inode;
  struct super_operations_t *s_op;
};

/*
 * Generic inode.
 */
struct inode_t {
  mode_t                    i_mode;
  uint16_t                  i_nlinks;
  uid_t                     i_uid;
  gid_t                     i_gid;
  ssize_t                   i_size;
  struct timespec           i_atime;
  struct timespec           i_mtime;
  struct timespec           i_ctime;
  uint32_t                  i_zone[10];
  ino_t                     i_ino;
  struct super_block_t      *i_sb;
  int                       i_ref;
  char                      i_dirt;
  struct inode_operations_t *i_op;
};

/*
 * Generic directory entry.
 */
struct dirent64_t {
  uint64_t                  d_inode;
  int64_t                   d_off;
  uint16_t                  d_reclen;
  uint8_t                   d_type;
  char                      d_name[];
};

/*
 * Generic file.
 */
struct file_t {
  mode_t                    f_mode;
  int                       f_flags;
  size_t                    f_pos;
  int                       f_ref;
  struct inode_t            *f_inode;
  struct file_operations_t  *f_op;
};

/*
 * Super block operations.
 */
struct super_operations_t {
  int (*read_inode)(struct inode_t *);
  int (*write_inode)(struct inode_t *);
  int (*put_inode)(struct inode_t *);
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
