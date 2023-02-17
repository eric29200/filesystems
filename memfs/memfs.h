#ifndef _MEMFS_H_
#define _MEMFS_H_

#include "../vfs/vfs.h"

#define MEMFS_MAGIC				0xABAB
#define MEMFS_NAME_LEN				255
#define MEMFS_ROOT_INODE			1

#define MEMFS_DIR_REC_LEN(name_len)		(8 + (name_len))

/*
 * MemFS in memory super block.
 */
struct memfs_sb_info_t {
	ino_t					s_inodes_cpt;			/* Inodes counter */
	uint64_t				s_ninodes;			/* Total number of inodes */
};

/*
 * MemFS in memory inode.
 */
struct memfs_inode_info_t {
	char *					i_data;				/* Pointers to data blocks */
	struct inode_t				vfs_inode;			/* VFS inode */
};

/*
 * MemFS directory entry.
 */
struct memfs_dir_entry_t {
	uint32_t				d_inode;			/* Inode number */
	uint16_t				d_rec_len;			/* Directory entry length */
	uint8_t					d_name_len;			/* Name length */
	uint8_t					d_file_type;			/* File type */
	char					d_name[MEMFS_NAME_LEN];		/* File name */
};


/* MemFS file system operations */
extern struct super_operations_t memfs_sops;
extern struct inode_operations_t memfs_file_iops;
extern struct inode_operations_t memfs_dir_iops;
extern struct inode_operations_t memfs_symlink_iops;
extern struct file_operations_t memfs_file_fops;
extern struct file_operations_t memfs_dir_fops;

/* MemFS super operations */
int memfs_read_super(struct super_block_t *sb, void *data);
void memfs_put_super(struct super_block_t *sb);
int memfs_statfs(struct super_block_t *sb, struct statfs *buf);

/* MemFS inode prototypes */
struct inode_t *memfs_new_inode(struct super_block_t *sb, mode_t mode);
struct inode_t *memfs_alloc_inode(struct super_block_t *sb);
void memfs_put_inode(struct inode_t *inode);
void memfs_delete_inode(struct inode_t *inode);

/* MemFS name resolution prototypes */
int memfs_add_entry(struct inode_t *dir, const char *name, size_t name_len, ino_t ino);
int memfs_lookup(struct inode_t *dir, const char *name, size_t name_len, struct inode_t **res_inode);
int memfs_create(struct inode_t *dir, const char *name, size_t name_len, mode_t mode, struct inode_t **res_inode);
int memfs_mkdir(struct inode_t *dir, const char *name, size_t name_len, mode_t mode);
int memfs_rmdir(struct inode_t *dir, const char *name, size_t name_len);
int memfs_link(struct inode_t *old_inode, struct inode_t *dir, const char *name, size_t name_len);
int memfs_unlink(struct inode_t *dir, const char *name, size_t name_len);
int memfs_symlink(struct inode_t *dir, const char *name, size_t name_len, const char *target);
int memfs_rename(struct inode_t *old_dir, const char *old_name, size_t old_name_len, struct inode_t *new_dir, const char *new_name, size_t new_name_len);

/* MemFS symlink prototypes */
int memfs_follow_link(struct inode_t *dir, struct inode_t *inode, struct inode_t **res_inode);
ssize_t memfs_readlink(struct inode_t *inode, char *buf, size_t bufsize);

/* MemFS file prototypes */
int memfs_file_read(struct file_t *filp, char *buf, int count);
int memfs_file_write(struct file_t *filp, const char *buf, int count);
int memfs_getdents64(struct file_t *filp, void *dirp, size_t count);

/* MemFS truncate prototypes */
void memfs_truncate(struct inode_t *inode);

/*
 * Get MemFS in memory super block from generic super block.
 */
static inline struct memfs_sb_info_t *memfs_sb(struct super_block_t *sb)
{
	return sb->s_fs_info;
}

/*
 * Get MemFS in memory inode from generic inode.
 */
static inline struct memfs_inode_info_t *memfs_i(struct inode_t *inode)
{
	return container_of(inode, struct memfs_inode_info_t, vfs_inode);
}

#endif
