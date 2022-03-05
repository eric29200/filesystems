#include <string.h>
#include <errno.h>

#include "ext2.h"

/*
 * Get directory entries.
 */
int ext2_getdents64(struct file_t *filp, void *dirp, size_t count)
{
  struct ext2_dir_entry_t de;
  struct dirent64_t *dirent;
  int entries_size;

  /* for each entry */
  for (entries_size = 0, dirent = (struct dirent64_t *) dirp;;) {
    /* read ext2 dir entry */
    if (ext2_file_read(filp, (char *) &de, EXT2_DIRENT_SIZE) != EXT2_DIRENT_SIZE)
      return entries_size;

    /* skip null entries */
    if (le32toh(de.d_inode) == 0)
      continue;

    /* not enough space to fill in next dir entry : break */
    if (count < sizeof(struct dirent64_t) + de.d_name_len + 1) {
      filp->f_pos -= EXT2_DIRENT_SIZE;
      return entries_size;
    }

    /* fill in dirent */
    dirent->d_inode = le32toh(de.d_inode);
    dirent->d_off = 0;
    dirent->d_reclen = sizeof(struct dirent64_t) + de.d_name_len + 1;
    dirent->d_type = 0;
    memcpy(dirent->d_name, de.d_name, de.d_name_len);
    dirent->d_name[de.d_name_len] = 0;

    /* go to next entry */
    count -= dirent->d_reclen;
    entries_size += dirent->d_reclen;
    dirent = (struct dirent64_t *) ((char *) dirent + dirent->d_reclen);

    /* update file position */
    filp->f_pos -= EXT2_DIRENT_SIZE - le16toh(de.d_rec_len);
  }

  return entries_size;
}