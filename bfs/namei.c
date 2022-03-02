#include <errno.h>

#include "bfs.h"

/*
 * Test file names equality.
 */
static inline int bfs_name_match(const char *name1, size_t len1, const char *name2)
{
  /* check overflow */
  if (len1 > BFS_NAME_LEN)
    return 0;

  return strncmp(name1, name2, len1) == 0 && (len1 == BFS_NAME_LEN || name2[len1] == 0);
}

/*
 * Find a BFS entry in a directory.
 */
static struct buffer_head_t *bfs_find_entry(struct inode_t *dir, const char *name, size_t name_len,
                                            struct bfs_dir_entry_t **res_de)
{
  struct buffer_head_t *bh = NULL;
  struct bfs_dir_entry_t *de;
  int nb_entries, i;

  /* check file name length */
  if (!name_len || name_len > BFS_NAME_LEN)
    return NULL;

  /* compute number of entries */
  nb_entries = dir->i_size / BFS_DIRENT_SIZE;

  /* walk through all entries */
  for (i = 0; i < nb_entries; i++) {
    /* read next block if needed */
    if (i % BFS_DIRS_PER_BLOCK == 0) {
      /* release previous block */
      brelse(bh);

      /* read next block */
      bh = sb_bread(dir->i_sb, bfs_i(dir)->i_sblock + i / BFS_DIRS_PER_BLOCK);
      if (!bh)
        return NULL;
    }

    /* get directory entry */
    de = (struct bfs_dir_entry_t *) (bh->b_data + (i % BFS_DIRS_PER_BLOCK) * BFS_DIRENT_SIZE);
    if (bfs_name_match(name, name_len, de->d_name)) {
      *res_de = de;
      return bh;
    }
  }

  /* release block buffer */
  brelse(bh);
  return NULL;
}

/*
 * Lookup for a file in a directory.
 */
int bfs_lookup(struct inode_t *dir, const char *name, size_t name_len, struct inode_t **res_inode)
{
  struct buffer_head_t *bh = NULL;
  struct bfs_dir_entry_t *de;
  ino_t ino;

  /* check dir */
  if (!dir)
    return -ENOENT;

  /* dir must be a directory */
  if (!S_ISDIR(dir->i_mode)) {
    vfs_iput(dir);
    return -ENOENT;
  }

  /* find entry */
  bh = bfs_find_entry(dir, name, name_len, &de);
  if (!bh) {
    vfs_iput(dir);
    return -ENOENT;
  }

  /* get inode number */
  ino = le16toh(de->d_ino);

  /* release block buffer */
  brelse(bh);

  /* get inode */
  *res_inode = vfs_iget(dir->i_sb, ino);
  if (!*res_inode) {
    vfs_iput(dir);
    return -EACCES;
  }

  vfs_iput(dir);
  return 0;
}

/*
 * Add a BFS entry in a directory.
 */
static int bfs_add_entry(struct inode_t *dir, const char *name, size_t name_len, struct inode_t *inode)
{
  struct bfs_inode_info_t *bfs_dir = bfs_i(dir);
  struct bfs_dir_entry_t *de;
  struct buffer_head_t *bh;
  int block, off;

  /* check file name */
  if (!name_len)
    return -EINVAL;
  if (name_len > BFS_NAME_LEN)
    return -ENAMETOOLONG;

  /* walk through all directories block */
  for (block = bfs_dir->i_sblock; block <= bfs_dir->i_eblock; block++) {
    /* read block */
    bh = sb_bread(dir->i_sb, block);
    if (!bh)
      return -ENOSPC;

    /* walk through all directory entries */
    for (off = 0; off < BFS_BLOCK_SIZE; off += BFS_DIRENT_SIZE) {
      de = (struct bfs_dir_entry_t *) (bh->b_data + off);

      /* free inode */
      if (!de->d_ino) {
        /* update directory size */
        if ((block - bfs_dir->i_sblock) * BFS_BLOCK_SIZE + off >= dir->i_size) {
          dir->i_size += BFS_DIRENT_SIZE;
          dir->i_ctime = current_time();
        }

        /* set new entry */
        memset(de->d_name, 0, BFS_NAME_LEN);
        memcpy(de->d_name, name, name_len);

        /* set new entry inode */
        de->d_ino = inode->i_ino;

        /* mark buffer dirty and release it */
        bh->b_dirt = 1;
        brelse(bh);

        /* update parent directory */
        dir->i_mtime = dir->i_ctime = current_time();
        dir->i_dirt = 1;

        return 0;
      }
    }

    /* release block */
    brelse(bh);
  }

  return -ENOSPC;
}

/*
 * Create a file in a directory.
 */
int bfs_create(struct inode_t *dir, const char *name, size_t name_len, mode_t mode, struct inode_t **res_inode)
{
  struct inode_t *inode, *tmp;
  ino_t ino;
  int err;

  /* check directory */
  *res_inode = NULL;
  if (!dir)
    return -ENOENT;

  /* check if file already exists */
  dir->i_ref++;
  if (bfs_lookup(dir, name, name_len, &tmp) == 0) {
    vfs_iput(tmp);
    vfs_iput(dir);
    return -EEXIST;
  }

  /* create a new inode */
  inode = bfs_new_inode(dir->i_sb);
  if (!inode) {
    vfs_iput(dir);
    return -ENOSPC;
  }

  /* set inode */
  inode->i_op = &bfs_file_iops;
  inode->i_mode = S_IFREG | mode;
  inode->i_dirt = 1;

  /* add new entry to dir */
  err = bfs_add_entry(dir, name, name_len, inode);
  if (err) {
    inode->i_nlinks--;
    vfs_iput(inode);
    vfs_iput(dir);
    return err;
  }

  /* release inode (to write it on disk) */
  ino = inode->i_ino;
  vfs_iput(inode);

  /* read inode from disk */
  *res_inode = vfs_iget(dir->i_sb, ino);
  if (!*res_inode) {
    vfs_iput(dir);
    return -EACCES;
  }

  /* release directory */
  vfs_iput(dir);

  return 0;
}
