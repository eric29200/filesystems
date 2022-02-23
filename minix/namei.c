#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "minixfs.h"

/*
 * Test file names equality.
 */
static inline int minixfs_name_match(const char *name1, size_t len1, const char *name2, size_t max_len)
{
  /* check overflow */
  if (len1 > max_len)
    return 0;

  return strncmp(name1, name2, len1) == 0 && (len1 == max_len || name2[len1] == 0);
}

/*
 * Find a Minix entry in a directory.
 */
static struct buffer_head_t *minixfs_find_entry(struct inode_t *dir, const char *name, size_t name_len, void **res_de)
{
  struct minix_sb_info_t *sbi = minixfs_sb(dir->i_sb);
  int nb_entries, nb_entries_per_block, i;
  struct buffer_head_t *bh = NULL;
  struct minix1_dir_entry_t *de1;
  struct minix3_dir_entry_t *de3;
  char *de, *de_name;
  
  /* check file name length */
  if (!name_len || name_len > sbi->s_name_len)
    return NULL;
  
  /* compute number of entries in directory */
  nb_entries = dir->i_size / sbi->s_dirsize;
  nb_entries_per_block = dir->i_sb->s_blocksize / sbi->s_dirsize;
  
  /* walk through all entries */
  for (i = 0; i < nb_entries; i++) {
    /* read next block if needed */
    if (i % nb_entries_per_block == 0) {
      /* release previous block */
      brelse(bh);
      
      /* read next block */
      bh = minixfs_bread(dir, i / nb_entries_per_block, 0);
      if (!bh)
        return NULL;
    }
                                     
    /* get directory entry */
    de = bh->b_data + i * sbi->s_dirsize;
    if (sbi->s_version == MINIXFS_V3) {
      de3 = (struct minix3_dir_entry_t *) de;
      de_name = de3->d_name;
    } else {
      de1 = (struct minix1_dir_entry_t *) de;
      de_name = de1->d_name;
    }
    
    /* name match */
    if (minixfs_name_match(name, name_len, de_name, sbi->s_dirsize)) {
      *res_de = de;
      return bh;
    }
  }
  
  /* free block buffer */
  brelse(bh);
  return NULL;
}

/*
 * Add a Minix entry in a directory.
 */
static int minixfs_add_entry(struct inode_t *dir, const char *name, size_t name_len, struct inode_t *inode)
{
  struct minix_sb_info_t *sbi = minixfs_sb(dir->i_sb);
  int nb_entries, nb_entries_per_block, i;
  struct buffer_head_t *bh = NULL;
  struct minix1_dir_entry_t *de1;
  struct minix3_dir_entry_t *de3;
  char *de, *de_name;
  ino_t de_ino;

  /* check file name */
  if (!name_len || name_len > sbi->s_name_len)
    return -EINVAL;
  
  /* compute number of entries in directory */
  nb_entries = dir->i_size / sbi->s_dirsize;
  nb_entries_per_block = dir->i_sb->s_blocksize / sbi->s_dirsize;
  
  /* walk through all entries */
  for (i = 0; i <= nb_entries; i++) {
    /* read next block if needed */
    if (i % nb_entries_per_block == 0) {
      /* release previous block */
      brelse(bh);
      
      /* read next block */
      bh = minixfs_bread(dir, i / nb_entries_per_block, 1);
      if (!bh)
        return -EIO;
    }
                                      
    /* last entry : update directory size */
    if (i == nb_entries) {
      dir->i_size = (i + 1) * sbi->s_dirsize;
      dir->i_dirt = 1;
    }
                                     
    /* get directory entry */
    de = bh->b_data + i * sbi->s_dirsize;
    if (sbi->s_version == MINIXFS_V3) {
      de3 = (struct minix3_dir_entry_t *) de;
      de_ino = de3->d_inode;
      de_name = de3->d_name;
    } else {
      de1 = (struct minix1_dir_entry_t *) de;
      de_ino = de1->d_inode;
      de_name = de1->d_name;
    }
                                      
    /* found a free entry */
    if (!de_ino)
      goto found_free_entry;
  }
  
  /* free block buffer */
  brelse(bh);
  return -EINVAL;
found_free_entry:
  /* set new entry */
  memset(de_name, 0, sbi->s_name_len);
  memcpy(de_name, name, name_len);
  
  /* set new entry inode */
  if (sbi->s_version == MINIXFS_V3)
    de3->d_inode = inode->i_ino;
  else
    de1->d_inode = inode->i_ino;
  
  /* mark buffer dirty and release it */
  bh->b_dirt = 1;
  brelse(bh);
  
  /* update parent directory */
  dir->i_mtime = dir->i_ctime = time(NULL);
  dir->i_dirt = 1;
  
  return 0;
}

/*
 * Check if a Minix directory is empty (returns 1 if directory is empty).
 */
static int minixfs_empty_dir(struct inode_t *dir)
{
  struct minix_sb_info_t *sbi = minixfs_sb(dir->i_sb);
  int nb_entries, nb_entries_per_block, i;
  struct buffer_head_t *bh = NULL;
  ino_t ino;
  char *de;
  
  /* check if dir is a directory */
  if (S_ISREG(dir->i_mode))
    return 0;
  
  /* compute number of entries in directory */
  nb_entries = dir->i_size / sbi->s_dirsize;
  nb_entries_per_block = dir->i_sb->s_blocksize / sbi->s_dirsize;
  
  /* walk through all entries */
  for (i = 0; i < nb_entries; i++) {
    /* read next block if needed */
    if (i % nb_entries_per_block == 0) {
      /* release previous block */
      brelse(bh);
      
      /* read next block */
      bh = minixfs_bread(dir, i / nb_entries_per_block, 0);
      if (!bh)
        return 0;
    }
                                     
    /* skip first 2 entries "." and ".." */
    if (i < 2)
      continue;
                                     
    /* get inode number */
    de = bh->b_data + i * sbi->s_dirsize;
    if (sbi->s_version == MINIXFS_V3)
      ino = ((struct minix3_dir_entry_t *) de)->d_inode;
    else
      ino = ((struct minix1_dir_entry_t *) de)->d_inode;
    
    /* found an entry : directory is not empty */
    if (ino) {
      brelse(bh);
      return 0;  
    }
  }
  
  /* free block buffer */
  brelse(bh);
  return 1;
}

/*
 * Lookup for a file in a directory.
 */
int minixfs_lookup(struct inode_t *dir, const char *name, size_t name_len, struct inode_t **res_inode)
{
  struct minix_sb_info_t *sbi;
  struct buffer_head_t *bh;
  ino_t ino;
  void *de;
  
  /* check dir */
  if (!dir)
    return -ENOENT;

  /* dir must be a directory */
  if (!S_ISDIR(dir->i_mode)) {
    vfs_iput(dir);
    return -ENOENT;
  }
  
  /* find entry */
  bh = minixfs_find_entry(dir, name, name_len, &de);
  if (!bh) {
    vfs_iput(dir);
    return -ENOENT;
  }
  
  /* get inode number */
  sbi = minixfs_sb(dir->i_sb);
  if (sbi->s_version == MINIXFS_V3)
    ino = ((struct minix3_dir_entry_t *) de)->d_inode;
  else
    ino = ((struct minix1_dir_entry_t *) de)->d_inode;
  
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
int minixfs_create(struct inode_t *dir, const char *name, size_t name_len, mode_t mode, struct inode_t **res_inode)
{
  struct inode_t *inode, *tmp;
  ino_t ino;
  int ret;
  
  /* check directory */
  *res_inode = NULL;
  if (!dir)
    return -ENOENT;
  
  /* check if file already exists */
  dir->i_ref++;
  if (minixfs_lookup(dir, name, name_len, &tmp) == 0) {
    vfs_iput(tmp);
    vfs_iput(dir);
    return -EEXIST;
  }
  
  /* create a new inode */
  inode = minixfs_new_inode(dir->i_sb);
  if (!inode) {
    vfs_iput(dir);
    return -ENOSPC;
  }
  
  /* set inode */
  inode->i_op = &minixfs_file_iops;
  inode->i_mode = S_IFREG | mode;
  inode->i_dirt = 1;
  
  /* add new entry to dir */
  ret = minixfs_add_entry(dir, name, name_len, inode);
  if (ret) {
    inode->i_nlinks--;
    vfs_iput(inode);
    vfs_iput(dir);
    return -ENOSPC;
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
 * Unlink (remove) a Minix file.
 */
int minixfs_unlink(struct inode_t *dir, const char *name, size_t name_len)
{
  struct minix_sb_info_t *sbi;
  struct buffer_head_t *bh;
  struct inode_t *inode;
  ino_t ino;
  void *de;
  
  /* get directory entry */
  bh = minixfs_find_entry(dir, name, name_len, &de);
  if (!bh) {
    vfs_iput(dir);
    return -ENOENT;
  }
  
  /* get inode number */
  sbi = minixfs_sb(dir->i_sb);
  if (sbi->s_version == MINIXFS_V3)
    ino = ((struct minix3_dir_entry_t *) de)->d_inode;
  else
    ino = ((struct minix1_dir_entry_t *) de)->d_inode;
  
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
  memset(de, 0, sbi->s_dirsize);
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
 * Make a Minix directory.
 */
int minixfs_mkdir(struct inode_t *dir, const char *name, size_t name_len, mode_t mode)
{
  struct minix1_dir_entry_t *de1;
  struct minix3_dir_entry_t *de3;
  struct minix_sb_info_t *sbi;
  struct buffer_head_t *bh;
  struct inode_t *inode;
  void *de;
  int err;
  
  /* check if file exists */
  bh = minixfs_find_entry(dir, name, name_len, &de);
  if (bh) {
    brelse(bh);
    vfs_iput(dir);
    return -EEXIST;
  }
  
  /* allocate a new inode */
  inode = minixfs_new_inode(dir->i_sb);
  if (!inode) {
    vfs_iput(dir);
    return -ENOMEM;
  }
  
  /* set inode */
  sbi = minixfs_sb(dir->i_sb);
  inode->i_op = &minixfs_dir_iops;
  inode->i_mode = S_IFDIR | mode;
  inode->i_nlinks = 2;
  inode->i_size = sbi->s_dirsize * 2;
  inode->i_dirt = 1;
  
  /* read first block */
  bh = minixfs_bread(inode, 0, 1);
  if (!bh) {
    inode->i_nlinks = 0;
    vfs_iput(inode);
    vfs_iput(dir);
    return -ENOSPC;
  }

  /* add '.' and '..' entries */
  if (sbi->s_version == MINIXFS_V3) {
    /* add '.' entry */
    de3 = (struct minix3_dir_entry_t *) bh->b_data;
    de3->d_inode = inode->i_ino;
    strcpy(de3->d_name, ".");
    
    /* add '..' entry */
    de3 = (struct minix3_dir_entry_t *) (bh->b_data + sbi->s_dirsize);
    de3->d_inode = inode->i_ino;
    strcpy(de3->d_name, "..");
  } else {
    /* add '.' entry */
    de1 = (struct minix1_dir_entry_t *) bh->b_data;
    de1->d_inode = inode->i_ino;
    strcpy(de1->d_name, ".");
    
    /* add '..' entry */
    de1 = (struct minix1_dir_entry_t *) (bh->b_data + sbi->s_dirsize);
    de1->d_inode = inode->i_ino;
    strcpy(de1->d_name, "..");
  }
  
  /* release first block */
  bh->b_dirt = 1;
  brelse(bh);
  
  /* add entry to parent dir */
  err = minixfs_add_entry(dir, name, name_len, inode);
  if (err) {
    inode->i_nlinks = 0;
    vfs_iput(inode);
    vfs_iput(dir);
    return -ENOSPC;
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
 * Remove a Minix directory.
 */
int minixfs_rmdir(struct inode_t *dir, const char *name, size_t name_len)
{
  struct minix_sb_info_t *sbi;
  struct buffer_head_t *bh;
  struct inode_t *inode;
  ino_t ino;
  void *de;
  
  /* check if file exists */
  bh = minixfs_find_entry(dir, name, name_len, &de);
  if (!bh) {
    vfs_iput(dir);
    return -ENOENT;
  }
  
  /* get inode number */
  sbi = minixfs_sb(dir->i_sb);
  if (sbi->s_version == MINIXFS_V3)
    ino = ((struct minix3_dir_entry_t *) de)->d_inode;
  else
    ino = ((struct minix1_dir_entry_t *) de)->d_inode;
  
  /* get inode */
  inode = vfs_iget(dir->i_sb, ino);
  if (!inode) {
    brelse(bh);
    vfs_iput(dir);
    return -ENOENT;
  }
  
  /* remove directories only and do not allow to remove '.' */
  if (!S_ISDIR(inode->i_mode) || inode->i_ino == dir->i_ino) {
    brelse(bh);    
    vfs_iput(inode);
    vfs_iput(dir);
    return -EPERM;
  }
  
  /* directory must be empty */
  if (!minixfs_empty_dir(inode)) {
    brelse(bh);    
    vfs_iput(inode);
    vfs_iput(dir);
    return -EPERM;
  }
  
  /* reset entry */
  memset(de, 0, sbi->s_dirsize);
  bh->b_dirt = 1;
  brelse(bh);
  
  /* update dir */
  dir->i_nlinks--;
  dir->i_dirt = 1;
  
  /* update inode */
  inode->i_nlinks = 0;
  inode->i_dirt = 1;
  
  /* release inode and directory */
  vfs_iput(inode);
  vfs_iput(dir);
  
  return 0;
}

/*
 * Make a new name for a Minix file (= hard link).
 */
int minixfs_link(struct inode_t *old_inode, struct inode_t *dir, const char *name, size_t name_len)
{
  struct buffer_head_t *bh;
  void *de;
  int ret;
  
  /* check if new file exists */
  bh = minixfs_find_entry(dir, name, name_len, &de);
  if (bh) {
    brelse(bh);
    vfs_iput(old_inode);
    vfs_iput(dir);
    return -EEXIST;
  }
  
  /* add entry */
  ret = minixfs_add_entry(dir, name, name_len, old_inode);
  if (ret) {
    vfs_iput(old_inode);
    vfs_iput(dir);
    return ret;
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
 * Create a Minix symbolic link.
 */
int minixfs_symlink(struct inode_t *dir, const char *name, size_t name_len, const char *target)
{
  struct buffer_head_t *bh;
  struct inode_t *inode;
  int ret, i;
  void *de;
  
  /* create a new inode */
  inode = minixfs_new_inode(dir->i_sb);
  if (!inode) {
    vfs_iput(dir);
    return -ENOSPC;
  }
  
  /* set new inode */
  inode->i_op = &minixfs_file_iops;
  inode->i_mode = S_IFLNK | 0777;
  inode->i_dirt = 1;
  
  /* read/create first block */
  bh = minixfs_bread(inode, 0, 1);
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
  bh = minixfs_find_entry(dir, name, name_len, &de);
  if (bh) {
    brelse(bh);
    inode->i_nlinks = 0;
    vfs_iput(inode);
    vfs_iput(dir);
    return -EEXIST;
  }
  
  /* add entry */
  ret = minixfs_add_entry(dir, name, name_len, inode);
  if (ret) {
    vfs_iput(inode);
    vfs_iput(dir);
    return ret;
  }
  
  /* release inode */
  vfs_iput(inode);
  vfs_iput(dir);
  
  return 0;
}

/*
 * Rename a Minix file.
 */
int minixfs_rename(struct inode_t *old_dir, const char *old_name, size_t old_name_len,
                   struct inode_t *new_dir, const char *new_name, size_t new_name_len)
{
  struct inode_t *old_inode = NULL, *new_inode = NULL;
  struct buffer_head_t *old_bh = NULL, *new_bh = NULL;
  struct minix_sb_info_t *sbi;
  ino_t old_ino, new_ino;
  void *old_de, *new_de;
  int ret;
  
  /* find old entry */
  old_bh = minixfs_find_entry(old_dir, old_name, old_name_len, &old_de);
  if (!old_bh) {
    ret = -ENOENT;
    goto out;
  }
  
  /* get old inode number */
  sbi = minixfs_sb(old_dir->i_sb);
  if (sbi->s_version == MINIXFS_V3)
    old_ino = ((struct minix3_dir_entry_t *) old_de)->d_inode;
  else
    old_ino = ((struct minix1_dir_entry_t *) old_de)->d_inode;
  
  /* get old inode */
  old_inode = vfs_iget(old_dir->i_sb, old_ino);
  if (!old_inode) {
    ret = -ENOSPC;
    goto out;
  }
  
  /* find new entry (if exists) or add new one */
  new_bh = minixfs_find_entry(new_dir, new_name, new_name_len, &new_de);
  if (new_bh) {
    /* get new inode number */
    sbi = minixfs_sb(new_dir->i_sb);
    if (sbi->s_version == MINIXFS_V3)
      new_ino = ((struct minix3_dir_entry_t *) new_de)->d_inode;
    else
      new_ino = ((struct minix1_dir_entry_t *) new_de)->d_inode;
    
    /* get new inode */
    new_inode = vfs_iget(new_dir->i_sb, new_ino);
    if (!new_inode) {
      ret = -ENOSPC;
      goto out;
    }
                
    /* same inode : exit */
    if (old_inode->i_ino == new_inode->i_ino) {
      ret = 0;
      goto out;
    }
                
    /* modify new directory entry inode */
    sbi = minixfs_sb(old_dir->i_sb);
    if (sbi->s_version == MINIXFS_V3)
      ((struct minix3_dir_entry_t *) new_de)->d_inode = old_inode->i_ino;
    else
      ((struct minix1_dir_entry_t *) new_de)->d_inode = old_inode->i_ino;
    
    /* update new inode */
    new_inode->i_nlinks--;
    new_inode->i_atime = new_inode->i_mtime = time(NULL);
    new_inode->i_dirt = 1;
  } else {
    /* add new entry */
    ret = minixfs_add_entry(new_dir, new_name, new_name_len, old_inode);
    if (ret)
      goto out;
  }
  
  /* cancel old directory entry */
  sbi = minixfs_sb(old_dir->i_sb);
  if (sbi->s_version == MINIXFS_V3) {
    ((struct minix3_dir_entry_t *) old_de)->d_inode = 0;
    memset(((struct minix3_dir_entry_t *) old_de)->d_name, 0, sbi->s_name_len);
  } else {
    ((struct minix1_dir_entry_t *) old_de)->d_inode = 0;
    memset(((struct minix1_dir_entry_t *) old_de)->d_name, 0, sbi->s_name_len);
  }
  old_bh->b_dirt = 1;
  
  /* update old and new directories */
  old_dir->i_atime = old_dir->i_mtime = time(NULL);
  old_dir->i_dirt = 1;
  new_dir->i_atime = new_dir->i_mtime = time(NULL);
  new_dir->i_dirt = 1;
  
  ret = 0;
out:
  /* release buffers and inodes */
  brelse(old_bh);
  brelse(new_bh);
  vfs_iput(old_inode);
  vfs_iput(new_inode);
  vfs_iput(old_dir);
  vfs_iput(new_dir);
  
  return ret;
}