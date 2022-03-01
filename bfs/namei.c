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
