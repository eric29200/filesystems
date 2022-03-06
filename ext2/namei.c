#include <errno.h>

#include "ext2.h"

/*
 * Test file names equality.
 */
static inline int ext2_name_match(const char *name, size_t len, struct ext2_dir_entry_t *de)
{
  /* check dir entry */
  if (!de || !de->d_inode || len > EXT2_NAME_LEN)
    return 0;

  return len == de->d_name_len && !memcmp(name, de->d_name, len);
}

/*
 * Find a Ext2 entry in a directory.
 */
static struct buffer_head_t *ext2_find_entry(struct inode_t *dir, const char *name, size_t name_len,
                                             struct ext2_dir_entry_t **res_de)
{
  struct buffer_head_t *bh = NULL;
  struct ext2_dir_entry_t *de;
  uint32_t offset, block, pos;

  /* read block by block */
  for (block = 0, offset = 0, pos = 0; pos < dir->i_size; block++) {
    /* read next block */
    bh = ext2_bread(dir, block, 0);
    if (!bh)
      continue;

    /* read all entries in block */
    while (offset < dir->i_size && offset < dir->i_sb->s_blocksize) {
      /* check next entry */
      de = (struct ext2_dir_entry_t *) (bh->b_data + offset);
      if (le16toh(de->d_rec_len) <= 0) {
        brelse(bh);
        return NULL;
      }

      /* check name */
      if (ext2_name_match(name, name_len, de)) {
        *res_de = de;
        return bh;
      }

      /* update offset */
      offset += le16toh(de->d_rec_len);
      pos += le16toh(de->d_rec_len);
    }

    /* reset offset and release block buffer */
    offset = 0;
    brelse(bh);
  }

  return NULL;
}

/*
 * Lookup for a file in a directory.
 */
int ext2_lookup(struct inode_t *dir, const char *name, size_t name_len, struct inode_t **res_inode)
{
  struct ext2_dir_entry_t *de;
  struct buffer_head_t *bh;
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
  bh = ext2_find_entry(dir, name, name_len, &de);
  if (!bh) {
    vfs_iput(dir);
    return -ENOENT;
  }

  /* get inode number */
  ino = de->d_inode;

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
