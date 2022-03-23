#ifndef _FTPFS_H_
#define _FTPFS_H_

#include <netdb.h>

#include "../vfs/vfs.h"

#define FTPFS_MAGIC                   0xFAFA
#define FTPFS_PATH_LEN                BUFSIZ
#define FTPFS_NAME_LEN                1024

#define FTPFS_INODE_HTABLE_BITS       12
#define FTPFS_INODE_HTABLE_SIZE       (1 << FTPFS_INODE_HTABLE_BITS)

#define FTPFS_USER_DEFAULT            "anonymous"
#define FTPFS_PASWD_DEFAULT           "anonymous"

/*
 * FTP parameters.
 */
struct ftp_param_t {
  char                            user[FTPFS_NAME_LEN];         /* FTP user */
  char                            passwd[FTPFS_NAME_LEN];       /* FTP password */
};

/*
 * FTP buffer.
 */
struct ftp_buffer_t {
  char                            *data;                        /* buffer */
  size_t                          len;                          /* buffer length */
  size_t                          capacity;                     /* buffer capacity */
};

/*
 * FTPFS file attributes.
 */
struct ftpfs_fattr_t {
  char                            name[FTPFS_NAME_LEN];         /* file name */
  char                            link[FTPFS_NAME_LEN];         /* link target */
  struct stat                     statbuf;                      /* stat buf */
};

/*
 * FTPFS in memory super block.
 */
struct ftpfs_sb_info_t {
  struct sockaddr                 s_addr;                       /* FTP server address */
  struct list_head_t              s_inodes_cache_list;          /* inodes cache list */
  struct htable_link_t            **s_inodes_cache_htable;      /* inodes cache hash table */
  size_t                          s_inodes_cache_size;          /* inodes cache size */
};

/*
 * FTPFS in memory inode.
 */
struct ftpfs_inode_info_t {
  char                            *i_path;                      /* inode path */
  struct ftp_buffer_t             i_cache;                      /* cached data */
  struct inode_t                  vfs_inode;                    /* VFS inode */
  struct list_head_t              i_list;                       /* FTPFS inodes linked list */
};

/* FTPFS file system operations */
extern struct super_operations_t ftpfs_sops;
extern struct inode_operations_t ftpfs_file_iops;
extern struct inode_operations_t ftpfs_dir_iops;
extern struct inode_operations_t ftpfs_symlink_iops;
extern struct file_operations_t ftpfs_file_fops;
extern struct file_operations_t ftpfs_dir_fops;

/* FTP lib prototypes */
struct ftp_param_t *ftp_ask_parameters();
int ftp_connect(const char *hostname, const char *user, const char *passwd, struct sockaddr *addr);
int ftp_quit(int sockfd);
int ftp_list(int sockfd, struct sockaddr *addr, const char *dir, struct ftp_buffer_t *ftp_buf);
int ftp_retrieve(int sockfd, struct sockaddr *addr, const char *pathname, int fd_out);
int ftp_parse_dir_line(const char *line, struct ftpfs_fattr_t *fattr);

/* FTPFS super prototypes */
int ftpfs_read_super(struct super_block_t *sb, void *data);
void ftpfs_put_super(struct super_block_t *sb);
int ftpfs_statfs(struct super_block_t *sb, struct statfs *buf);

/* FTPFS inode prototypes */
struct inode_t *ftpfs_iget(struct super_block_t *sb, struct inode_t *dir, struct ftpfs_fattr_t *fattr);
int ftpfs_load_inode_data(struct inode_t *inode, struct ftpfs_fattr_t *fattr);
struct inode_t *ftpfs_alloc_inode(struct super_block_t *sb);
void ftpfs_put_inode(struct inode_t *inode);

/* FTPFS name resolution prototypes */
int ftpfs_lookup(struct inode_t *dir, const char *name, size_t name_len, struct inode_t **res_inode);

/* FTPFS symlink prototypes */
int ftpfs_follow_link(struct inode_t *dir, struct inode_t *inode, struct inode_t **res_inode);
ssize_t ftpfs_readlink(struct inode_t *inode, char *buf, size_t bufsize);

/* FTPFS file prototypes */
int ftpfs_open(struct file_t *filp);
int ftpfs_close(struct file_t *filp);
int ftpfs_file_read(struct file_t *filp, char *buf, int count);
int ftpfs_getdents64(struct file_t *filp, void *dirp, size_t count);

/*
 * Get FTPFS in memory super block from generic super block.
 */
static inline struct ftpfs_sb_info_t *ftpfs_sb(struct super_block_t *sb)
{
	return sb->s_fs_info;
}

/*
 * Get FTPFS in memory inode from generic inode.
 */
static inline struct ftpfs_inode_info_t *ftpfs_i(struct inode_t *inode)
{
  return container_of(inode, struct ftpfs_inode_info_t, vfs_inode);
}

#endif
