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

      /* skip null entry */
      if (le32toh(de->d_inode) == 0) {
        offset += le16toh(de->d_rec_len);
        pos += le16toh(de->d_rec_len);
        continue;
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
 * Add a Ext2 entry in a directory.
 */
static int ext2_add_entry(struct inode_t *dir, const char *name, size_t name_len, struct inode_t *inode)
{
  struct ext2_dir_entry_t *de, *de1;
  struct buffer_head_t *bh = NULL;
  uint16_t rec_len;
  uint32_t offset;

  /* truncate name if needed */
  if (name_len > EXT2_NAME_LEN)
    name_len = EXT2_NAME_LEN;

  /* compute record length */
  rec_len = EXT2_DIR_REC_LEN(name_len);

  /* read first block */
  bh = ext2_bread(dir, 0, 0);
  if (!bh)
    return -EIO;

  /* find a free entry */
  for (de = (struct ext2_dir_entry_t *) bh->b_data, offset = 0;;) {
    /* read next block */
    if ((char *) de >= bh->b_data + dir->i_sb->s_blocksize) {
      /* release previous block */
      brelse(bh);

      /* read next block */
      bh = ext2_bread(dir, offset / dir->i_sb->s_blocksize, 1);
      if (!bh)
        return -EIO;

      /* get first entry */
      de = (struct ext2_dir_entry_t *) bh->b_data;

      /* update directory size and create a new null entry */
      if (offset >= dir->i_size) {
        de->d_inode = 0;
        de->d_rec_len = htole16(dir->i_sb->s_blocksize);
        dir->i_size = offset + dir->i_sb->s_blocksize;
        dir->i_dirt = 1;
      }
    }

    /* check entry */
    if (le16toh(de->d_rec_len) <= 0) {
      brelse(bh);
      return -ENOENT;
    }

    /* free entry with enough space */
    if ((le32toh(de->d_inode) == 0 && le16toh(de->d_rec_len) >= rec_len)
        || (le16toh(de->d_rec_len) >= EXT2_DIR_REC_LEN(de->d_name_len) + rec_len)) {
      /* null entry : adjust record length */
      if (le32toh(de->d_inode)) {
        de1 = (struct ext2_dir_entry_t *) ((char *) de + EXT2_DIR_REC_LEN(de->d_name_len));
        de1->d_rec_len = htole16(le16toh(de->d_rec_len) - EXT2_DIR_REC_LEN(de->d_name_len));
        de->d_rec_len = htole16(EXT2_DIR_REC_LEN(de->d_name_len));
        de = de1;
      }

      goto found_entry;
    }

    /* go to next entry */
    offset += le16toh(de->d_rec_len);
    de = (struct ext2_dir_entry_t *) ((char *) de + le16toh(de->d_rec_len));
  }

  brelse(bh);
  return -EINVAL;
found_entry:
  /* set new entry */
  de->d_inode = htole32(inode->i_ino);
  de->d_name_len = name_len;
  de->d_file_type = 0;
  memcpy(de->d_name, name, name_len);

  /* mark buffer dirty and release it */
  bh->b_dirt = 1;
  brelse(bh);

  /* update parent directory */
  dir->i_mtime = dir->i_ctime = current_time();
  dir->i_dirt = 1;

  return 0;
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
  ino = le32toh(de->d_inode);

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
 * Create a file in a directory.
 */
int ext2_create(struct inode_t *dir, const char *name, size_t name_len, mode_t mode, struct inode_t **res_inode)
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
  if (ext2_lookup(dir, name, name_len, &tmp) == 0) {
    vfs_iput(tmp);
    vfs_iput(dir);
    return -EEXIST;
  }

  /* create a new inode */
  inode = ext2_new_inode(dir, S_IFREG | mode);
  if (!inode) {
    vfs_iput(dir);
    return -ENOSPC;
  }

  /* set inode */
  inode->i_op = &ext2_file_iops;
  inode->i_dirt = 1;

  /* add new entry to dir */
  err = ext2_add_entry(dir, name, name_len, inode);
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

/*
 * Make a Ext2 directory.
 */
int ext2_mkdir(struct inode_t *dir, const char *name, size_t name_len, mode_t mode)
{
  struct ext2_dir_entry_t *de;
  struct buffer_head_t *bh;
  struct inode_t *inode;
  int err;

  /* check if file exists */
  bh = ext2_find_entry(dir, name, name_len, &de);
  if (bh) {
    brelse(bh);
    vfs_iput(dir);
    return -EEXIST;
  }

  /* allocate a new inode */
  inode = ext2_new_inode(dir, S_IFDIR | mode);
  if (!inode) {
    vfs_iput(dir);
    return -ENOMEM;
  }

  /* set inode */
  inode->i_op = &ext2_dir_iops;
  inode->i_nlinks = 2;
  inode->i_size = inode->i_sb->s_blocksize;
  inode->i_dirt = 1;

  /* read first block */
  bh = ext2_bread(inode, 0, 1);
  if (!bh) {
    inode->i_nlinks = 0;
    vfs_iput(inode);
    vfs_iput(dir);
    return -ENOSPC;
  }

  /* add '.' entry */
  de = (struct ext2_dir_entry_t *) bh->b_data;
  de->d_inode = htole32(inode->i_ino);
  de->d_name_len = 1;
  de->d_rec_len = htole16(EXT2_DIR_REC_LEN(de->d_name_len));
  strcpy(de->d_name, ".");

  /* add '.' entry */
  de = (struct ext2_dir_entry_t *) ((char *) de + le16toh(de->d_rec_len));
  de->d_inode = htole32(inode->i_ino);
  de->d_name_len = 2;
  de->d_rec_len = htole16(EXT2_DIR_REC_LEN(de->d_name_len));
  strcpy(de->d_name, "..");

  /* release first block */
  bh->b_dirt = 1;
  brelse(bh);

  /* add entry to parent dir */
  err = ext2_add_entry(dir, name, name_len, inode);
  if (err) {
    inode->i_nlinks = 0;
    vfs_iput(inode);
    vfs_iput(dir);
    return err;
  }

  /* update directory links and mark it dirty */
  dir->i_nlinks++;
  dir->i_dirt = 1;

  /* release inode */
  vfs_iput(dir);
  vfs_iput(inode);

  return 0;
}

/*
 * Make a new name for a file (= hard link).
 */
int ext2_link(struct inode_t *old_inode, struct inode_t *dir, const char *name, size_t name_len)
{
  struct ext2_dir_entry_t *de;
  struct buffer_head_t *bh;
  int err;

  /* check if new file exists */
  bh = ext2_find_entry(dir, name, name_len, &de);
  if (bh) {
    brelse(bh);
    vfs_iput(old_inode);
    vfs_iput(dir);
    return -EEXIST;
  }

  /* add entry */
  err = ext2_add_entry(dir, name, name_len, old_inode);
  if (err) {
    vfs_iput(old_inode);
    vfs_iput(dir);
    return err;
  }

  /* update old inode */
  old_inode->i_nlinks++;
  old_inode->i_dirt = 1;

  /* release inodes */
  vfs_iput(old_inode);
  vfs_iput(dir);

  return 0;
}

/*
 * Unlink (remove) a file.
 */
int ext2_unlink(struct inode_t *dir, const char *name, size_t name_len)
{
  struct ext2_dir_entry_t *de;
  struct buffer_head_t *bh;
  struct inode_t *inode;
  ino_t ino;

  /* get directory entry */
  bh = ext2_find_entry(dir, name, name_len, &de);
  if (!bh) {
    vfs_iput(dir);
    return -ENOENT;
  }

  /* get inode number */
  ino = le32toh(de->d_inode);

  /* get inode */
  inode = vfs_iget(dir->i_sb, ino);
  if (!inode) {
    vfs_iput(dir);
    brelse(bh);
    return -ENOENT;
  }

  /* remove regular files only */
  if (S_ISDIR(inode->i_mode)) {
    vfs_iput(inode);
    vfs_iput(dir);
    brelse(bh);
    return -EPERM;
  }

  /* reset directory entry */
  de->d_inode = htole32(0);
  bh->b_dirt = 1;
  brelse(bh);

  /* update inode */
  inode->i_nlinks--;
  inode->i_dirt = 1;

  /* release inode */
  vfs_iput(inode);
  vfs_iput(dir);

  return 0;
}

/*
 * Create a symbolic link.
 */
int ext2_symlink(struct inode_t *dir, const char *name, size_t name_len, const char *target)
{
  struct ext2_dir_entry_t *de;
  struct buffer_head_t *bh;
  struct inode_t *inode;
  int err, i;

  /* create a new inode */
  inode = ext2_new_inode(dir, S_IFLNK);
  if (!inode) {
    vfs_iput(dir);
    return -ENOSPC;
  }

  /* set new inode */
  inode->i_op = &ext2_symlink_iops;
  inode->i_mode = S_IFLNK | 0777;
  inode->i_dirt = 1;

  /* read/create first block */
  bh = ext2_bread(inode, 0, 1);
  if(!bh) {
    inode->i_nlinks = 0;
    vfs_iput(inode);
    vfs_iput(dir);
    return -ENOSPC;
  }

  /* write file name on first block */
  for (i = 0; target[i] && i < inode->i_sb->s_blocksize - 1; i++)
    bh->b_data[i] = target[i];
  bh->b_data[i] = 0;
  bh->b_dirt = 1;
  brelse(bh);

  /* update inode size */
  inode->i_size = i;
  inode->i_dirt = 1;

  /* check if file exists */
  bh = ext2_find_entry(dir, name, name_len, &de);
  if (bh) {
    brelse(bh);
    inode->i_nlinks = 0;
    vfs_iput(inode);
    vfs_iput(dir);
    return -EEXIST;
  }

  /* add entry */
  err = ext2_add_entry(dir, name, name_len, inode);
  if (err) {
    vfs_iput(inode);
    vfs_iput(dir);
    return err;
  }

  /* release inode */
  vfs_iput(inode);
  vfs_iput(dir);

  return 0;
}
