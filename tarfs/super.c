#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>

#include "tarfs.h"

#define TARFS_ALLOC_SIZE          1024

/*
 * TarFS super block operations.
 */
struct super_operations_t tarfs_sops = {
  .alloc_inode        = tarfs_alloc_inode,
  .put_inode          = tarfs_put_inode,
  .read_inode         = tarfs_read_inode,
  .put_super          = tarfs_put_super,
  .statfs             = tarfs_statfs,
};

/*
 * Convert TAR type to POSIX.
 */
static mode_t tar_type_to_posix(int typeflag)
{
  switch(typeflag) {
    case TAR_REGTYPE:
    case TAR_AREGTYPE:
      return S_IFREG;
    case TAR_DIRTYPE:
      return S_IFDIR;
    case TAR_SYMTYPE:
      return S_IFLNK;
    case TAR_CHRTYPE:
      return S_IFCHR;
    case TAR_BLKTYPE:
      return S_IFBLK;
    case TAR_FIFOTYPE:
      return S_IFIFO;
    default:
      return 0;
  }
}

/*
 * Get or create a TAR entry.
 */
static struct tar_entry_t *tar_get_or_create_entry(struct super_block_t *sb, struct tar_entry_t *parent,
                                                   const char *name, struct tar_header_t *tar_header, off_t offset)
{
  struct tarfs_sb_info_t *sbi = tarfs_sb(sb);
  struct tar_entry_t *entry;
  struct list_head_t *pos;

  /* check if entry already exist */
  if (parent) {
    list_for_each(pos, &parent->children) {
      entry = list_entry(pos, struct tar_entry_t, list);
      if (strcmp(entry->name, name) == 0)
        return entry;
    }
  }

  /* create new entry */
  entry = (struct tar_entry_t *) malloc(sizeof(struct tar_entry_t));
  if (!entry)
    return NULL;

  /* set new entry name */
  entry->name = strdup(name);
  if (!entry->name) {
    free(entry);
    return NULL;
  }

  /* parse TAR header */
  if (tar_header) {
    entry->data_off = offset + TARFS_BLOCK_SIZE;
    entry->data_len = strtol(tar_header->size, NULL, 8);
    entry->mode = strtol(tar_header->mode, NULL, 8) | tar_type_to_posix(tar_header->typeflag);
    entry->uid = strtol(tar_header->uid, NULL, 8);
    entry->gid = strtol(tar_header->gid, NULL, 8);
    entry->mtime.tv_sec = strtol(tar_header->mtime, NULL, 8);
    entry->mtime.tv_nsec = 0;
    entry->atime.tv_sec = strtol(tar_header->atime, NULL, 8);
    entry->atime.tv_nsec = 0;
    entry->ctime.tv_sec = strtol(tar_header->atime, NULL, 8);
    entry->ctime.tv_nsec = 0;
  } else {
    entry->data_off = 0;
    entry->data_len = 0;
    entry->mode = S_IFDIR | 0755;
    entry->uid = getuid();
    entry->gid = getgid();
    entry->atime = entry->mtime = entry->ctime = current_time();
  }

  /* set inode number */
  entry->ino = sbi->s_ninodes++;
  INIT_LIST_HEAD(&entry->children);
  INIT_LIST_HEAD(&entry->list);

  /* add to parent */
  if (parent) {
    entry->parent = parent;
    list_add(&entry->list, &parent->children);
  } else {
    entry->parent = NULL;
  }

  return entry;
}

/*
 * Parse a TAR entry.
 */
static struct tar_entry_t *tar_parse_entry(struct super_block_t *sb, struct tar_header_t *tar_header, off_t offset)
{
  struct tar_entry_t *entry = NULL, *parent;
  char full_name[BUFSIZ], *start, *end;
  size_t prefix_len, name_len;

  /* check magic string */
  if (memcmp(tar_header->magic, TARFS_MAGIC_STR, sizeof(tar_header->magic)))
    return NULL;

  /* compute prefix and name lengths */
  prefix_len = strnlen(tar_header->prefix, sizeof(tar_header->prefix));
  name_len = strnlen(tar_header->name, sizeof(tar_header->name));

  /* concat prefix and name */
  memcpy(full_name, tar_header->prefix, prefix_len);
  memcpy(full_name + prefix_len, tar_header->name, name_len);
  full_name[prefix_len + name_len] = 0;

  /* remove last '/' */
  if (full_name[prefix_len + name_len - 1] == '/')
    full_name[prefix_len + name_len - 1] = 0;

  /* parse full name */
  for (start = full_name, parent = tarfs_sb(sb)->s_root_entry;;) {
    /* skip '/' */
    for (; *start == '/' && *start; start++);

    /* find next folder in path */
    end = strchr(start, '/');
    if (end)
      *end = 0;

    /* create a new entry */
    entry = tar_get_or_create_entry(sb, parent, start, end ? NULL : tar_header, offset);

    /* last folder : exit */
    if (!end || !entry)
      break;

    /* go to next folder */
    start = end + 1;
    parent = entry;
  }

  return entry;
}

/*
 * Parse a TAR archive.
 */
static int tar_open(struct super_block_t *sb)
{
  struct tarfs_sb_info_t *sbi = tarfs_sb(sb);
  struct tar_entry_t *entry;
  struct buffer_head_t *bh;
  off_t offset;

  /* create root entry */
  sbi->s_root_entry = tar_get_or_create_entry(sb, NULL, "/", NULL, 0);
  if (!sbi->s_root_entry)
    return -ENOSPC;

  /* parse each entry */
  for (offset = 0;;) {
    /* read next block/entry */
    bh = sb_bread(sb, offset / sb->s_blocksize);
    if (!bh)
      break;

    /* parse entry */
    entry = tar_parse_entry(sb, (struct tar_header_t *) bh->b_data, offset);
    if (!entry) {
      brelse(bh);
      break;
    }

    /* release block buffer */
    brelse(bh);

    /* update offset */
    offset = ALIGN_UP(entry->data_off + entry->data_len, TARFS_BLOCK_SIZE);
  }

  return 0;
}

/*
 * Free a TAR entry and its children.
 */
static void tar_free(struct tar_entry_t *entry)
{
  struct list_head_t *pos, *n;

  if (!entry)
    return;

  /* free name */
  if (entry->name)
    free(entry->name);

  /* free children */
  list_for_each_safe(pos, n, &entry->children)
    tar_free(list_entry(pos, struct tar_entry_t, list));
}

/*
 * Index a TAR entry by its inode number.
 */
static void tar_index(struct super_block_t *sb, struct tar_entry_t *entry)
{
  struct list_head_t *pos;

  if (!entry)
    return;

  /* index entry */
  tarfs_sb(sb)->s_tar_entries[entry->ino] = entry;

  /* index children */
  list_for_each(pos, &entry->children)
    tar_index(sb, list_entry(pos, struct tar_entry_t, list));
}

/*
 * Read a TarFS super block.
 */
int tarfs_read_super(struct super_block_t *sb, void *data)
{
  struct tarfs_sb_info_t *sbi;
  size_t i;
  int err;

  /* allocate TarFS super block */
  sb->s_fs_info = sbi = (struct tarfs_sb_info_t *) malloc(sizeof(struct tarfs_sb_info_t));
  if (!sbi)
    return -ENOMEM;

  /* set super block */
  sb->s_blocksize_bits = TARFS_BLOCK_SIZE_BITS;
  sb->s_blocksize = TARFS_BLOCK_SIZE;
  sb->s_magic = TARFS_MAGIC;
  sb->s_root_inode = NULL;
  sb->s_op = &tarfs_sops;
  sbi->s_ninodes = 0;
  sbi->s_root_entry = NULL;
  sbi->s_tar_entries = NULL;

  /* parse TAR archive */
  err = tar_open(sb);
  if (err)
    goto err_bad_sb;

  /* create TAR entries index */
  sbi->s_tar_entries = (struct tar_entry_t **) malloc(sizeof(struct tar_entry_t *) * sbi->s_ninodes);
  if (!sbi->s_tar_entries) {
    err = -ENOMEM;
    goto err_index;
  }

  /* reset entries */
  for (i = 0; i < sbi->s_ninodes; i++)
    sbi->s_tar_entries[i] = NULL;

  /* index TAR entries */
  tar_index(sb, sbi->s_root_entry);

  /* get root inode */
  sb->s_root_inode = vfs_iget(sb, TARFS_ROOT_INO);
  if (!sb->s_root_inode) {
    err = -ENOSPC;
    goto err_root_inode;
  }

  return 0;
err_root_inode:
  fprintf(stderr, "TARFS : can't get root inode\n");
  goto err_release_tar;
err_index:
  fprintf(stderr, "TARFS : can't index TAR entries\n");
err_release_tar:
  tar_free(sbi->s_root_entry);
  goto err;
err_bad_sb:
  fprintf(stderr, "TARFS : can't read super block\n");
err:
  free(sbi);
  return err;
}

/*
 * Unmount a TarFS File System.
 */
void tarfs_put_super(struct super_block_t *sb)
{
  struct tarfs_sb_info_t *sbi = tarfs_sb(sb);

  /* release root inode */
  vfs_iput(sb->s_root_inode);

  /* free all entries */
  tar_free(sbi->s_root_entry);
  if (sbi->s_tar_entries)
    free(sbi->s_tar_entries);

  /* free in memory super block */
  free(sbi);
}

/*
 * Get TarFS File system status.
 */
int tarfs_statfs(struct super_block_t *sb, struct statfs *buf)
{
  struct tarfs_sb_info_t *sbi = tarfs_sb(sb);

  buf->f_type = sb->s_magic;
  buf->f_bsize = sb->s_blocksize;
  buf->f_blocks = 0;
  buf->f_bfree = 0;
  buf->f_bavail = buf->f_bfree;
  buf->f_files = sbi->s_ninodes;
  buf->f_ffree = 0;
  buf->f_namelen = 0;

  return 0;
}
