#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>

#include "isofs.h"

/*
 * ISOFS file operations.
 */
struct file_operations_t isofs_file_fops = {
  .read               = isofs_file_read,
};

/*
 * ISOFS directory operations.
 */
struct file_operations_t isofs_dir_fops = {
  .getdents64         = isofs_getdents64,
};

/*
 * ISOFS file inode operations.
 */
struct inode_operations_t isofs_file_iops = {
  .fops               = &isofs_file_fops,
  .follow_link        = NULL,
  .readlink           = NULL,
};

/*
 * ISOFS directory inode operations.
 */
struct inode_operations_t isofs_dir_iops = {
  .fops               = &isofs_dir_fops,
  .lookup             = isofs_lookup,
};

/*
 * Allocate a ISOFS inode.
 */
struct inode_t *isofs_alloc_inode(struct super_block_t *sb)
{
  struct isofs_inode_info_t *isofs_inode;

  /* allocate ISOFS inode */
  isofs_inode = (struct isofs_inode_info_t *) malloc(sizeof(struct isofs_inode_info_t));
  if (!isofs_inode)
    return NULL;

  return &isofs_inode->vfs_inode;
}

/*
 * Read a ISOFS inode.
 */
int isofs_read_inode(struct inode_t *inode)
{
  struct iso_directory_record_t *raw_inode;
  int offset, frag1, err = 0;
  char raw_inode_mem[4096];
  struct buffer_head_t *bh;
  uint32_t block;

  /* read inode block */
  block = inode->i_ino >> inode->i_sb->s_blocksize_bits;
  bh = sb_bread(inode->i_sb, block);
  if (!bh)
    return -EIO;

  /* get raw inode (= directory record) */
  raw_inode = (struct iso_directory_record_t *) (bh->b_data + (inode->i_ino & (inode->i_sb->s_blocksize - 1)));

  /* if raw inode is on 2 blocks, copy it in memory */
  if ((inode->i_ino & (inode->i_sb->s_blocksize - 1)) + raw_inode->length[0] > inode->i_sb->s_blocksize) {
    /* copy first fragment */
    offset = (inode->i_ino & (inode->i_sb->s_blocksize - 1));
    frag1 = inode->i_sb->s_blocksize - offset;
    memcpy(raw_inode_mem, bh->b_data + offset, frag1);

    /* read next block */
    brelse(bh);
    bh = sb_bread(inode->i_sb, ++block);
    if (!bh)
      return -EIO;

    /* copy 2nd fragment */
    memcpy(raw_inode_mem + frag1, bh->b_data, raw_inode->length[0] - frag1);
    raw_inode = (struct iso_directory_record_t *) &raw_inode_mem[0];
  }

  /* set inode */
  inode->i_mode = S_IRWXU | S_IRWXG | S_IRWXO;
  if (raw_inode->flags[0] & 2)
    inode->i_mode |= S_IFDIR;
  else
    inode->i_mode |= S_IFREG;
  inode->i_nlinks = 1;
  inode->i_uid = getuid();
  inode->i_gid = getgid();
  inode->i_size = isofs_num733(raw_inode->size);
  inode->i_atime.tv_sec = inode->i_mtime.tv_sec = inode->i_ctime.tv_sec = isofs_date(raw_inode->date);
  inode->i_atime.tv_nsec = inode->i_mtime.tv_nsec = inode->i_ctime.tv_nsec = 0;
  isofs_i(inode)->i_first_extent = (isofs_num733(raw_inode->extent) + isofs_num711(raw_inode->ext_attr_length))
                                   << isofs_sb(inode->i_sb)->s_log_zone_size;
  isofs_i(inode)->i_backlink = 0xFFFFFFFF;

  /* set operations */
  if (S_ISREG(inode->i_mode)) {
    inode->i_op = &isofs_file_iops;
  } else if (S_ISDIR(inode->i_mode)) {
    inode->i_op = &isofs_dir_iops;
  } else {
    err = -EINVAL;
  }

  /* release block buffer */
  brelse(bh);

  return err;
}

/*
 * Release a ISOFS inode.
 */
void isofs_put_inode(struct inode_t *inode)
{
  if (!inode)
    return;

  /* free inode */
  free(isofs_i(inode));
}
