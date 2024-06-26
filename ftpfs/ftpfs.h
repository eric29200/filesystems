#ifndef _FTPFS_H_
#define _FTPFS_H_

#include <netdb.h>

#include "../vfs/vfs.h"

#define FTPFS_MAGIC				0xFAFA
#define FTPFS_PATH_LEN				BUFSIZ
#define FTPFS_NAME_LEN				1024

#define FTPFS_INODE_HTABLE_BITS			12
#define FTPFS_INODE_HTABLE_SIZE			(1 << FTPFS_INODE_HTABLE_BITS)

#define FTPFS_USER_DEFAULT			"anonymous"
#define FTPFS_PASWD_DEFAULT			"anonymous"

/*
 * FTP parameters.
 */
struct ftp_param {
	char					user[FTPFS_NAME_LEN];			/* FTP user */
	char					passwd[FTPFS_NAME_LEN];			/* FTP password */
};

/*
 * FTP buffer.
 */
struct ftp_buffer {
	char *					data;					/* buffer */
	size_t					len;					/* buffer length */
	size_t					capacity;				/* buffer capacity */
};

/*
 * FTPFS file attributes.
 */
struct ftpfs_fattr {
	char					name[FTPFS_NAME_LEN];			/* file name */
	char					link[FTPFS_NAME_LEN];			/* link target */
	struct stat				statbuf;				/* stat buf */
};

/*
 * FTPFS in memory super block.
 */
struct ftpfs_sb_info {
	struct sockaddr				s_addr;					/* FTP server address */
	struct list_head			s_inodes_cache_list;			/* inodes cache list */
	struct htable_link **			s_inodes_cache_htable;			/* inodes cache hash table */
	size_t					s_inodes_cache_size;			/* inodes cache size */
};

/*
 * FTPFS in memory inode.
 */
struct ftpfs_inode_info {
	char *					i_path;					/* inode path */
	struct ftp_buffer			i_cache;				/* cached data */
	struct inode				vfs_inode;				/* VFS inode */
	struct list_head			i_list;					/* FTPFS inodes linked list */
};

/* FTPFS file system operations */
extern struct super_operations ftpfs_sops;
extern struct inode_operations ftpfs_file_iops;
extern struct inode_operations ftpfs_dir_iops;
extern struct inode_operations ftpfs_symlink_iops;
extern struct file_operations ftpfs_file_fops;
extern struct file_operations ftpfs_dir_fops;

/* FTP lib prototypes */
struct ftp_param *ftp_ask_parameters();
int ftp_connect(const char *hostname, const char *user, const char *passwd, struct sockaddr *addr);
int ftp_quit(int sockfd);
int ftp_list(int sockfd, struct sockaddr *addr, const char *dir, struct ftp_buffer *ftp_buf);
int ftp_retrieve(int sockfd, struct sockaddr *addr, const char *pathname, int fd_out);
int ftp_store(int sockfd, struct sockaddr *addr, const char *pathname, int fd_in);
int ftp_parse_dir_line(const char *line, struct ftpfs_fattr *fattr);
int ftp_rm(int sockfd, const char *pathname);
int ftp_mkdir(int sockfd, const char *pathname);
int ftp_rmdir(int sockfd, const char *pathname);
int ftp_rename(int sockfd, const char *old_pathname, const char *new_pathname);

/* FTPFS super prototypes */
int ftpfs_read_super(struct super_block *sb, void *data);
void ftpfs_put_super(struct super_block *sb);
int ftpfs_statfs(struct super_block *sb, struct statfs *buf);

/* FTPFS inode prototypes */
struct inode *ftpfs_iget(struct super_block *sb, struct inode *dir, struct ftpfs_fattr *fattr);
char *ftpfs_build_path(struct inode *dir, struct ftpfs_fattr *fattr);
int ftpfs_load_inode_data(struct inode *inode, struct ftpfs_fattr *fattr);
int ftpfs_reload_inode_data(struct inode *inode, struct ftpfs_fattr *fattr);
struct inode *ftpfs_alloc_inode(struct super_block *sb);
void ftpfs_put_inode(struct inode *inode);
void ftpfs_delete_inode(struct inode *inode);

/* FTPFS name resolution prototypes */
int ftpfs_lookup(struct inode *dir, const char *name, size_t name_len, struct inode **res_inode);
int ftpfs_create(struct inode *dir, const char *name, size_t name_len, mode_t mode, struct inode **res_inode);
int ftpfs_unlink(struct inode *dir, const char *name, size_t name_len);
int ftpfs_mkdir(struct inode *dir, const char *name, size_t name_len, mode_t mode);
int ftpfs_rmdir(struct inode *dir, const char *name, size_t name_len);
int ftpfs_rename(struct inode *old_dir, const char *old_name, size_t old_name_len, struct inode *new_dir, const char *new_name, size_t new_name_len);

/* FTPFS symlink prototypes */
int ftpfs_follow_link(struct inode *dir, struct inode *inode, struct inode **res_inode);
ssize_t ftpfs_readlink(struct inode *inode, char *buf, size_t bufsize);

/* FTPFS file prototypes */
int ftpfs_open(struct file *filp);
int ftpfs_close(struct file *filp);
int ftpfs_file_read(struct file *filp, char *buf, int count);
int ftpfs_file_write(struct file *filp, const char *buf, int count);
int ftpfs_getdents64(struct file *filp, void *dirp, size_t count);

/*
 * Get FTPFS in memory super block from generic super block.
 */
static inline struct ftpfs_sb_info *ftpfs_sb(struct super_block *sb)
{
	return sb->s_fs_info;
}

/*
 * Get FTPFS in memory inode from generic inode.
 */
static inline struct ftpfs_inode_info *ftpfs_i(struct inode *inode)
{
	return container_of(inode, struct ftpfs_inode_info, vfs_inode);
}

#endif
