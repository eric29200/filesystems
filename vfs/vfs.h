#ifndef _VFS_H_
#define _VFS_H_

#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/vfs.h>

#define VFS_MINIXFS_TYPE            1

#define VFS_NR_INODE                1024
#define VFS_NR_BUFFER               1024

/*
 * Block buffer.
 */
struct buffer_head_t {
  uint32_t                  b_block;
  char                      *b_data;
  char                      b_ref;
  char                      b_dirt;
  struct super_block_t      *b_sb;
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
  uint16_t                  i_mode;
  uint16_t                  i_nlinks;
  uint16_t                  i_uid;
  uint16_t                  i_gid;
  uint32_t                  i_size;
  uint32_t                  i_atime;
  uint32_t                  i_mtime;
  uint32_t                  i_ctime;
  uint32_t                  i_zone[10];
  ino_t                     i_ino;
  struct super_block_t      *i_sb;
  char                      i_ref;
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
  uint16_t                  f_mode;
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
struct file_t *vfs_open(struct inode_t *root, const char *pathname, int flags, mode_t mode);
int vfs_close(struct file_t *filp);
ssize_t vfs_read(struct file_t *filp, char *buf, int count);
ssize_t vfs_write(struct file_t *filp, const char *buf, int count);
off_t vfs_lseek(struct file_t *filp, off_t offset, int whence);
int vfs_getdents64(struct file_t *filp, void *dirp, size_t count);

#endif
