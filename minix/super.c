#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <math.h>

#include "minix.h"

/*
 * Minix super block operations.
 */
struct super_operations_t minix_sops = {
  .alloc_inode        = minix_alloc_inode,
  .put_inode          = minix_put_inode,
  .delete_inode       = minix_delete_inode,
  .read_inode         = minix_read_inode,
  .write_inode        = minix_write_inode,
  .put_super          = minix_put_super,
  .statfs             = minix_statfs,
};

/*
 * Read a minix super block.
 */
int minix_read_super(struct super_block_t *sb)
{
  struct minix1_super_block_t *msb1;
  struct minix3_super_block_t *msb3;
  struct minix_sb_info_t *sbi;
  int i, err = -ENOSPC;
  uint32_t block;
  
  /* allocate Minix file system */
  sb->s_fs_info = sbi = (struct minix_sb_info_t *) malloc(sizeof(struct minix_sb_info_t));
  if (!sbi)
    return -ENOMEM;
  
  /* set default block size */
  sb->s_blocksize = MINIX_BLOCK_SIZE;
  sb->s_blocksize_bits = MINIX_BLOCK_SIZE_BITS;
  
  /* read first block = super block */
  sbi->sb_bh = sb_bread(sb, 1);
  if (!sbi->sb_bh) {
    err = -EIO;
    goto err_bad_sb;
  }
  
  /* set Minix file system */
  msb1 = (struct minix1_super_block_t *) sbi->sb_bh->b_data;
  sbi->s_ninodes = msb1->s_ninodes;
  sbi->s_nzones = msb1->s_nzones;
  sbi->s_imap_blocks = msb1->s_imap_blocks;
  sbi->s_zmap_blocks = msb1->s_zmap_blocks;
  sbi->s_firstdatazone = msb1->s_firstdatazone;
  sbi->s_log_zone_size = msb1->s_log_zone_size;
  sbi->s_state = msb1->s_state;
  sbi->s_imap = NULL;
  sbi->s_zmap = NULL;
  sbi->s_max_size = msb1->s_max_size;
  sb->s_magic = msb1->s_magic;
  sb->root_inode = NULL;
  sb->s_op = &minix_sops;
  
  /* set Minix file system specific version */
  if (sb->s_magic == MINIX1_MAGIC1) {
    sbi->s_version = MINIX_V1;
    sbi->s_name_len = 14;
    sbi->s_dirsize = 16;
  } else if (sb->s_magic == MINIX1_MAGIC2) {
    sbi->s_version = MINIX_V1;
    sbi->s_name_len = 30;
    sbi->s_dirsize = 32;
  } else if (sb->s_magic == MINIX2_MAGIC1) {
    sbi->s_version = MINIX_V2;
    sbi->s_name_len = 14;
    sbi->s_dirsize = 16;
  } else if (sb->s_magic == MINIX2_MAGIC2) {
    sbi->s_version = MINIX_V2;
    sbi->s_name_len = 30;
    sbi->s_dirsize = 32;
  } else if (*((uint16_t *) (sbi->sb_bh->b_data + 24)) == MINIX3_MAGIC) {
      msb3 = (struct minix3_super_block_t *) sbi->sb_bh->b_data;
      sbi->s_ninodes = msb3->s_ninodes;
      sbi->s_nzones = msb3->s_zones;
      sbi->s_imap_blocks = msb3->s_imap_blocks;
      sbi->s_zmap_blocks = msb3->s_zmap_blocks;
      sbi->s_firstdatazone = msb3->s_firstdatazone;
      sbi->s_log_zone_size = msb3->s_log_zone_size;
      sbi->s_state = MINIX_VALID_FS;
      sbi->s_version = MINIX_V3;
      sbi->s_name_len = 60;
      sbi->s_dirsize = 64;
      sbi->s_max_size = msb3->s_max_size;
      sb->s_blocksize = msb3->s_blocksize;
      sb->s_blocksize_bits = log2(msb3->s_blocksize);
  } else {
    goto err_bad_magic;
  }

  /* allocate inodes bitmap */
  sbi->s_imap = (struct buffer_head_t **) malloc(sizeof(struct buffer_head_t *) * sbi->s_imap_blocks);
  if (!sbi->s_imap) {
    err = -ENOMEM;
    goto err_no_map;
  }
  
  /* reset inodes bitmap */
  for (i = 0; i < sbi->s_imap_blocks; i++)
    sbi->s_imap[i] = NULL;
  
  /* read inodes bitmap */
  for (i = 0, block = 2; i < sbi->s_imap_blocks; i++, block++) {
    sbi->s_imap[i] = sb_bread(sb, block);
    if (!sbi->s_imap[i]) {
      err = -EIO;
      goto err_map;
    }
  }
  
  /* allocate zones bitmap */
  sbi->s_zmap = (struct buffer_head_t **) malloc(sizeof(struct buffer_head_t *) * sbi->s_zmap_blocks);
  if (!sbi->s_zmap) {
    err = -ENOMEM;
    goto err_no_map;
  }
  
  /* reset zones bitmap */
  for (i = 0; i < sbi->s_zmap_blocks; i++)
    sbi->s_zmap[i] = NULL;
  
  /* read zones bitmap */
  for (i = 0; i < sbi->s_zmap_blocks; i++, block++) {
    sbi->s_zmap[i] = sb_bread(sb, block);
    if (!sbi->s_zmap[i]) {
      err = -EIO;
      goto err_map;
    }
  }
  
  /* get root inode */
  sb->root_inode = vfs_iget(sb, MINIX_ROOT_INODE);
  if (!sb->root_inode)
    goto err_root_inode;
  
  /* set root inode reference */
  sb->root_inode->i_ref = 1;
  
  return 0;
err_root_inode:
  fprintf(stderr, "Minix : can't read root inode\n");
  goto err_release_map;
err_map:
  fprintf(stderr, "Minix : can't read inodes/zones bitmaps\n");
  goto err_release_map;
err_no_map:
  fprintf(stderr, "Minix : can't allocate inodes/zones bitmaps\n");
err_release_map:
  if (sbi->s_imap) {
    for (i = 0; i < sbi->s_imap_blocks; i++)
      brelse(sbi->s_imap[i]);
    
    free(sbi->s_imap);
  }
  
  if (sbi->s_zmap) {
    for (i = 0; i < sbi->s_zmap_blocks; i++)
      brelse(sbi->s_zmap[i]);
    
    free(sbi->s_zmap);
  }
  goto err_release_sb;
err_bad_magic:
  fprintf(stderr, "Minix : wrong magic number\n");
err_release_sb:
  brelse(sbi->sb_bh);
  goto err;
err_bad_sb:
  fprintf(stderr, "Minix : can't read super block\n");
err:
  free(sbi);
  return err;
}

/*
 * Unmount a Minix File System.
 */
void minix_put_super(struct super_block_t *sb)
{
  struct minix_sb_info_t *sbi = minix_sb(sb);
  int i;
  
  /* release root inode */
  vfs_iput(sb->root_inode);
  
  /* release inodes bitmap */
  if (sbi->s_imap) {
    for (i = 0; i < sbi->s_imap_blocks; i++)
      brelse(sbi->s_imap[i]);
    
    free(sbi->s_imap);
  }
  
  /* release zones bitmap */
  if (sbi->s_zmap) {
    for (i = 0; i < sbi->s_zmap_blocks; i++)
      brelse(sbi->s_zmap[i]);
    
    free(sbi->s_zmap);
  }
  
  /* release super block */
  brelse(sbi->sb_bh);
  
  /* free in memory super block */
  free(sbi);
}

/*
 * Get Minix File system status.
 */
int minix_statfs(struct super_block_t *sb, struct statfs *buf)
{
  struct minix_sb_info_t *sbi = minix_sb(sb);

  buf->f_type = sb->s_magic;
  buf->f_bsize = sb->s_blocksize;
  buf->f_blocks = sbi->s_nzones - sbi->s_firstdatazone;
  buf->f_bfree = minix_count_free_blocks(sb);
  buf->f_bavail = buf->f_bfree;
  buf->f_files = sbi->s_ninodes;
  buf->f_ffree = minix_count_free_inodes(sb);
  buf->f_namelen = sbi->s_name_len;

  return 0;
}
