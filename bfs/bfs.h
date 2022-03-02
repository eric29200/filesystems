#ifndef _BFS_H_
#define _BFS_H_

#include <endian.h>

#include "../vfs/vfs.h"

#define BFS_BLOCK_SIZE_BITS       9
#define BFS_BLOCK_SIZE            (1 << (BFS_BLOCK_SIZE_BITS))

#define BFS_MAGIC                 0x1BADFACE
#define BFS_ROOT_INO              2
#define BFS_NAME_LEN              14
#define BFS_VREG                  1L
#define BFS_VDIR                  2L

#define BFS_INODES_PER_BLOCK      ((BFS_BLOCK_SIZE) / sizeof(struct bfs_inode_t))
#define BFS_DIRENT_SIZE           (sizeof(struct bfs_dir_entry_t))
#define BFS_DIRS_PER_BLOCK        ((BFS_BLOCK_SIZE) / (BFS_DIRENT_SIZE))

#define BFS_FILE_BLOCKS(inode)    (le32toh((inode)->i_sblock) == 0 ? 0 : le32toh((inode)->i_eblock) + 1 - le32toh((inode)->i_sblock))
#define BFS_FILE_SIZE(inode)      (le32toh((inode)->i_sblock) == 0 ? 0 : le32toh((inode)->i_eoffset) + 1 - le32toh((inode)->i_sblock) * BFS_BLOCK_SIZE)

/*
 * BFS in memory super block.
 */
struct bfs_sb_info_t {
  uint32_t              s_blocks;         /* total number of blocks */
  uint32_t              s_freeb;          /* number of free blocks */
  uint32_t              s_freei;          /* number of free inodes */
  uint32_t              s_lf_eblk;        /* last file end block */
  uint32_t              s_lasti;          /* last inode number */
  char                  *s_imap;          /* inodes bitmap */
};

/*
 * BFS in memory inode.
 */
struct bfs_inode_info_t {
  uint32_t              i_dsk_ino;        /* on disk inode number */
  uint32_t              i_sblock;         /* start block of file */
  uint32_t              i_eblock;         /* end block of file */
  struct inode_t        vfs_inode;        /* VFS inode */
};

/*
 * BFS on disk super block.
 */
struct bfs_super_block_t {
	uint32_t              s_magic;          /* magic number */
	uint32_t              s_start;          /* start of data blocks (in bytes) */
	uint32_t              s_end;            /* end of data blocks (in bytes) */
	uint32_t              s_from;
	uint32_t              s_to;
	int32_t               s_bfrom;
	int32_t               s_bto;
	char                  s_fsname[6];      /* file system name */
	char                  s_volume[6];      /* volume name */
	uint32_t              s_padding[118];   /* padding */
};

/*
 * BFS on disk inode.
 */
struct bfs_inode_t {
  uint16_t              i_ino;            /* inode number */
	uint16_t              i_unused;         /* unused */
	uint32_t              i_sblock;         /* start block of file */
	uint32_t              i_eblock;         /* end block of file */
	uint32_t              i_eoffset;        /* end of file in byte */
	uint32_t              i_vtype;          /* file attributes */
	uint32_t              i_mode;           /* file mode */
	uint32_t              i_uid;            /* user id */
	uint32_t              i_gid;            /* group id */
	uint32_t              i_nlink;          /* number of links to this file */
	uint32_t              i_atime;          /* last access time */
	uint32_t              i_mtime;          /* last modification time */
	uint32_t              i_ctime;          /* creation time */
	uint32_t              i_padding[4];     /* padding */
};

/*
 * BFS directory entry.
 */
struct bfs_dir_entry_t {
	uint16_t              d_ino;                    /* inode number */
	char                  d_name[BFS_NAME_LEN];     /* file name */
};

/* BFS file system operations */
extern struct file_operations_t bfs_file_fops;
extern struct file_operations_t bfs_dir_fops;
extern struct super_operations_t bfs_sops;
extern struct inode_operations_t bfs_file_iops;
extern struct inode_operations_t bfs_dir_iops;

/* BFS super prototypes */
int bfs_read_super(struct super_block_t *sb);
void bfs_put_super(struct super_block_t *sb);
int bfs_statfs(struct super_block_t *sb, struct statfs *buf);

/* BFS bitmap prototypes */
struct inode_t *bfs_new_inode(struct super_block_t *sb);
int bfs_free_inode(struct inode_t *inode);

/* BFS inode prototypes */
struct inode_t *bfs_alloc_inode(struct super_block_t *sb);
void bfs_put_inode(struct inode_t *inode);
void bfs_delete_inode(struct inode_t *inode);
int bfs_read_inode(struct inode_t *inode);
int bfs_write_inode(struct inode_t *inode);

/* BFS name resolution prototypes */
int bfs_lookup(struct inode_t *dir, const char *name, size_t name_len, struct inode_t **res_inode);
int bfs_create(struct inode_t *dir, const char *name, size_t name_len, mode_t mode, struct inode_t **res_inode);
int bfs_link(struct inode_t *old_inode, struct inode_t *dir, const char *name, size_t name_len);
int bfs_unlink(struct inode_t *dir, const char *name, size_t name_len);
int bfs_rename(struct inode_t *old_dir, const char *old_name, size_t old_name_len,
               struct inode_t *new_dir, const char *new_name, size_t new_name_len);

/* BFS file prototypes */
struct buffer_head_t *bfs_bread(struct inode_t *inode, uint32_t block, int create);
int bfs_file_read(struct file_t *filp, char *buf, int count);
int bfs_file_write(struct file_t *filp, const char *buf, int count);
int bfs_getdents64(struct file_t *filp, void *dirp, size_t count);

/* BFS truncate prototypes */
int bfs_truncate(struct inode_t *inode);

/*
 * Get BFS in memory super block from generic super block.
 */
static inline struct bfs_sb_info_t *bfs_sb(struct super_block_t *sb)
{
	return sb->s_fs_info;
}

/*
 * Get BFS in memory inode from generic inode.
 */
static inline struct bfs_inode_info_t *bfs_i(struct inode_t *inode)
{
	return container_of(inode, struct bfs_inode_info_t, vfs_inode);
}

#endif
