#include <string.h>
#include <errno.h>

#include "tarfs.h"

/*
 * Get directory entries.
 */
int tarfs_getdents64(struct file_t *filp, void *dirp, size_t count)
{
  struct dirent64_t *dirent = (struct dirent64_t *) dirp;
  struct tar_entry_t *tar_entry, *child;
  int entries_size = 0, i = 2;
  struct list_head_t *pos;
  size_t name_len;

  /* get tar entry */
  tar_entry = tarfs_i(filp->f_inode)->entry;
  if (!tar_entry)
    return -ENOENT;

  /* add ".." entry */
  if (filp->f_pos == 0) {
    /* check if input buffer is big enough */
    if (count < sizeof(struct dirent64_t) + 3)
      return -ENOSPC;

    /* add ".." entry */
    dirent->d_inode = tar_entry->parent ? tar_entry->parent->ino : tar_entry->ino;
    dirent->d_off = 0;
    dirent->d_reclen = sizeof(struct dirent64_t) + 3;
    dirent->d_type = 0;
    dirent->d_name[0] = '.';
    dirent->d_name[1] = '.';
    dirent->d_name[2] = 0;
    count -= dirent->d_reclen;
    entries_size += dirent->d_reclen;
    dirent = (struct dirent64_t *) ((char *) dirent + dirent->d_reclen);
    filp->f_pos = 1;
  }

  /* add "." entry */
  if (filp->f_pos == 1) {
    /* check if input buffer is big enough */
    if (count < sizeof(struct dirent64_t) + 2)
      return -ENOSPC;

    /* add "." entry */
    dirent->d_inode = tar_entry->ino;
    dirent->d_off = 0;
    dirent->d_reclen = sizeof(struct dirent64_t) + 2;
    dirent->d_type = 0;
    dirent->d_name[0] = '.';
    dirent->d_name[1] = 0;
    count -= dirent->d_reclen;
    entries_size += dirent->d_reclen;
    dirent = (struct dirent64_t *) ((char *) dirent + dirent->d_reclen);
    filp->f_pos = 2;
  }

  /* for each child */
  list_for_each(pos, &tar_entry->children) {
    child = list_entry(pos, struct tar_entry_t, list);

    /* skip entries before current positions */
    if (i++ < filp->f_pos)
      continue;

    /* not enough space to fill in next dir entry : break */
    name_len = strlen(child->name);
    if (count < sizeof(struct dirent64_t) + name_len + 1)
      return entries_size;

    /* fill in dirent */
    dirent->d_inode = child->ino;
    dirent->d_off = 0;
    dirent->d_reclen = sizeof(struct dirent64_t) + name_len + 1;
    dirent->d_type = 0;
    memcpy(dirent->d_name, child->name, name_len);
    dirent->d_name[name_len] = 0;

    /* go to next entry */
    count -= dirent->d_reclen;
    entries_size += dirent->d_reclen;
    dirent = (struct dirent64_t *) ((char *) dirent + dirent->d_reclen);

    /* update file position */
    filp->f_pos++;
  }

  return entries_size;
}
