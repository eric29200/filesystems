#define FUSE_USE_VERSION  32
#include <fuse3/fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>

#include "vfs/vfs.h"

#define DIR_BUF_SIZE            4096

/*
 * VFS data.
 */
struct vfs_data_t {
  char *dev;                    /* device path */
  char *mnt_point;              /* mount point */
  int fs_type;                  /* file system type */
  struct super_block_t *sb;     /* mounted super block */
};

/*
 * Get file attributes/status.
 */
static int op_getattr(const char *pathname, struct stat *statbuf, struct fuse_file_info *fi)
{
  struct vfs_data_t *vfs_data;

  /* get VFS data */
  vfs_data = fuse_get_context()->private_data;

  /* stat file */
  return vfs_stat(vfs_data->sb->root_inode, pathname, statbuf);
}

/*
 * Read a link value.
 */
static int op_readlink(const char *pathname, char *buf, size_t bufsize)
{
  struct vfs_data_t *vfs_data;

  /* get VFS data */
  vfs_data = fuse_get_context()->private_data;

  /* read link */
  return vfs_readlink(vfs_data->sb->root_inode, pathname, buf, bufsize);
}

/*
 * Create a special file.
 */
static int op_mknod(const char *pathname, mode_t mode, dev_t dev)
{
  fprintf(stderr, "mknod not implemented\n");
  return -ENOSYS;
}

/*
 * Create a directory.
 */
static int op_mkdir(const char *pathname, mode_t mode)
{
  struct vfs_data_t *vfs_data;

  /* get VFS data */
  vfs_data = fuse_get_context()->private_data;

  /* make directory */
  return vfs_mkdir(vfs_data->sb->root_inode, pathname, mode);
}

/*
 * Remove a file.
 */
static int op_unlink(const char *pathname)
{
  struct vfs_data_t *vfs_data;

  /* get VFS data */
  vfs_data = fuse_get_context()->private_data;

  /* remove file */
  return vfs_unlink(vfs_data->sb->root_inode, pathname);
}

/*
 * Remove a directory.
 */
static int op_rmdir(const char *pathname)
{
  struct vfs_data_t *vfs_data;

  /* get VFS data */
  vfs_data = fuse_get_context()->private_data;

  /* remove directory */
  return vfs_rmdir(vfs_data->sb->root_inode, pathname);
}

/*
 * Create a symbolic link.
 */
static int op_symlink(const char *target, const char *linkpath)
{
  fprintf(stderr, "symlink not implemented\n");
  return -ENOSYS;
}

/*
 * Rename a file.
 */
static int op_rename(const char *oldpath, const char *newpath, unsigned int flags)
{
  struct vfs_data_t *vfs_data;

  /* get VFS data */
  vfs_data = fuse_get_context()->private_data;

  /* rename file */
  return vfs_rename(vfs_data->sb->root_inode, oldpath, newpath);
}

/*
 * Make a new name for a file (= hard link).
 */
static int op_link(const char *oldpath, const char *newpath)
{
  struct vfs_data_t *vfs_data;

  /* get VFS data */
  vfs_data = fuse_get_context()->private_data;

  /* link file */
  return vfs_link(vfs_data->sb->root_inode, oldpath, newpath);
}

/*
 * Change permissions of a file.
 */
static int op_chmod(const char *pathname, mode_t mode, struct fuse_file_info *fi)
{
  struct vfs_data_t *vfs_data;

  /* get VFS data */
  vfs_data = fuse_get_context()->private_data;

  /* chmod */
  return vfs_chmod(vfs_data->sb->root_inode, pathname, mode);
}

/*
 * Change owner of a file.
 */
static int op_chown(const char *pathname, uid_t uid, gid_t gid, struct fuse_file_info *fi)
{
  struct vfs_data_t *vfs_data;

  /* get VFS data */
  vfs_data = fuse_get_context()->private_data;

  /* chown */
  return vfs_chown(vfs_data->sb->root_inode, pathname, uid, gid);
}

/*
 * Truncate a file.
 */
static int op_truncate(const char *pathname, off_t length, struct fuse_file_info *fi)
{
  struct vfs_data_t *vfs_data;

  /* get VFS data */
  vfs_data = fuse_get_context()->private_data;

  /* chown */
  return vfs_truncate(vfs_data->sb->root_inode, pathname, length);
}

/*
 * Open a file.
 */
static int op_open(const char *pathname, struct fuse_file_info *fi)
{
  struct vfs_data_t *vfs_data;
  struct file_t *file;

  /* get VFS data */
  vfs_data = fuse_get_context()->private_data;

  /* open file */
  file = vfs_open(vfs_data->sb->root_inode, pathname, fi->flags, 0);
  if (!file)
    return -ENOENT;

  /* store file */
  fi->fh = (uint64_t) file;

  return 0;
}

/*
 * Read from a file.
 */
static int op_read(const char *pathname, char *buf, size_t length, off_t offset, struct fuse_file_info *fi)
{
  struct vfs_data_t *vfs_data;
  int err, close_fi = 0;
  struct file_t *file;

  /* get VFS data */
  vfs_data = fuse_get_context()->private_data;
  file = (struct file_t *) fi->fh;

  /* open file if needed */
  if (!file) {
    file = vfs_open(vfs_data->sb->root_inode, pathname, O_RDONLY, 0);
    if (!file)
      return -1;

    close_fi = 1;
  }

  /* seek to position */
  err = vfs_lseek(file, offset, SEEK_SET);
  if (err == -1)
    goto out;

  /* read */
  err = vfs_read(file, buf, length);

out:
  if (close_fi)
    vfs_close(file);

  return err;
}

/*
 * Write to a file.
 */
static int op_write(const char *pathname, const char *buf, size_t length, off_t offset, struct fuse_file_info *fi)
{
  struct vfs_data_t *vfs_data;
  int err, close_fi = 0;
  struct file_t *file;

  /* get VFS data */
  vfs_data = fuse_get_context()->private_data;
  file = (struct file_t *) fi->fh;

  /* open file if needed */
  if (!file) {
    file = vfs_open(vfs_data->sb->root_inode, pathname, O_WRONLY, 0);
    if (!file)
      return -1;

    close_fi = 1;
  }

  /* seek to position */
  err = vfs_lseek(file, offset, SEEK_SET);
  if (err == -1)
    goto out;

  /* write */
  err = vfs_write(file, buf, length);

out:
  if (close_fi)
    vfs_close(file);

  return err;
}

/*
 * Get statistics on a file system.
 */
static int op_statfs(const char *pathname, struct statvfs *statbuf)
{
  struct vfs_data_t *vfs_data;
  struct statfs statbuf_fs;
  int err;

  /* get VFS data */
  vfs_data = fuse_get_context()->private_data;

  /* get stats */
  err = vfs_statfs(vfs_data->sb, &statbuf_fs);
  if (err)
    return err;

  /* copy statistics */
  statbuf->f_bsize = statbuf_fs.f_bsize;
  statbuf->f_blocks = statbuf_fs.f_blocks;
  statbuf->f_bfree = statbuf_fs.f_bfree;
  statbuf->f_bavail = statbuf_fs.f_bavail;
  statbuf->f_files = statbuf_fs.f_files;
  statbuf->f_ffree = statbuf_fs.f_ffree;
  statbuf->f_namemax = statbuf_fs.f_namelen;
  statbuf->f_flag = statbuf_fs.f_flags;

  return 0;
}

/*
 * Fluse a file on disk.
 */
static int op_flush(const char *pathname, struct fuse_file_info *fi)
{
  fprintf(stderr, "flush not implemented\n");
  return -ENOSYS;
}

/*
 * Release a file.
 */
static int op_release(const char *pathname, struct fuse_file_info *fi)
{
  struct file_t *file;

  /* get file */
  file = (struct file_t *) fi->fh;

  /* close file */
  return vfs_close(file);
}

/*
 * Synchronize a file.
 */
static int op_fsync(const char *pathname, int data_sync, struct fuse_file_info *fi)
{
  fprintf(stderr, "fsync not implemented\n");
  return -ENOSYS;
}

/*
 * Set extended attribute.
 */
static int op_setxattr(const char *pathname, const char *name, const char *value, size_t size, int flags)
{
  fprintf(stderr, "setxattr not implemented\n");
  return -ENOSYS;
}

/*
 * Get extended attribute.
 */
static int op_getxattr(const char *pathname, const char *name, char *value, size_t size)
{
  fprintf(stderr, "getxattr not implemented\n");
  return -ENOSYS;
}

/*
 * List extended attributes.
 */
static int op_listxattr(const char *pathname, char *list, size_t size)
{
  fprintf(stderr, "listxattr not implemented\n");
  return -ENOSYS;
}

/*
 * Remove extended attribute.
 */
static int op_removexattr(const char *pathname, const char *name)
{
  fprintf(stderr, "removexattr not implemented\n");
  return -ENOSYS;
}

/*
 * Read a directory.
 */
static int op_readdir(const char *pathname, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi,
                      enum fuse_readdir_flags flags)
{
  struct dirent64_t *dir_entry;
  char dir_buf[DIR_BUF_SIZE];
  struct file_t *file;
  int n, i;

  /* get file */
  file = (struct file_t *) fi->fh;

  /* read directory */
  for (;;) {
    /* read next entries */
    n = vfs_getdents64(file, dir_buf, DIR_BUF_SIZE);
    if (n < 0)
      return n;

    /* no more data */
    if (n == 0)
      break;

    /* for each entry */
    for (i = 0; i < n;) {
      /* get entry */
      dir_entry = (struct dirent64_t *) (dir_buf + i);

      /* fill directory */
      filler(buf, dir_entry->d_name, NULL, 0, 0);

      /* go to next entry */
      i += dir_entry->d_reclen;
    }
  }

  return 0;
}

/*
 * Init operation (= mount file system).
 */
static void *op_init(struct fuse_conn_info *conn, struct fuse_config *cfg)
{
  struct vfs_data_t *vfs_data;
  struct fuse_context *ctx;

  /* get VFS data */
  ctx = fuse_get_context();
  vfs_data = ctx->private_data;
  
  /* mount file system */
  vfs_data->sb = vfs_mount(vfs_data->dev, VFS_MINIX_TYPE);
  if (!vfs_data->sb)
    fuse_exit(ctx->fuse);

  return vfs_data;
}

/*
 * Destroy operation (= umount file system).
 */
static void op_destroy(void *private_data)
{
  struct vfs_data_t *vfs_data;
  
  /* get VFS data */
  vfs_data = fuse_get_context()->private_data;
  
  /* unmount file system */
  if (vfs_data->sb)
    vfs_umount(vfs_data->sb);
}

/*
 * Check user's permissions for a file.
 */
static int op_access(const char *pathname, int mask)
{
  struct vfs_data_t *vfs_data;

  /* get VFS data */
  vfs_data = fuse_get_context()->private_data;

  /* check access */
  return vfs_access(vfs_data->sb->root_inode, pathname, 0);
}

/*
 * Create a file.
 */
static int op_create(const char *pathname, mode_t mode, struct fuse_file_info *fi)
{
  struct vfs_data_t *vfs_data;

  /* get VFS data */
  vfs_data = fuse_get_context()->private_data;

  /* create file */
  return vfs_create(vfs_data->sb->root_inode, pathname, mode);
}

/*
 * Lock a file.
 */
static int op_lock(const char *pathname, struct fuse_file_info *fi, int cmd, struct flock *lock)
{
  fprintf(stderr, "lock not implemented\n");
  return -ENOSYS;
}

/*
 * Change file timestamps.
 */
static int op_utimens(const char *pathname, const struct timespec tv[2], struct fuse_file_info *fi)
{
  struct vfs_data_t *vfs_data;

  /* get VFS data */
  vfs_data = fuse_get_context()->private_data;

  /* set timestamps */
  return vfs_utimens(vfs_data->sb->root_inode, pathname, tv, 0);
}

/*
 * Fuse operations.
 */
static const struct fuse_operations vfs_ops = {
  .getattr        = op_getattr,
  .readlink       = op_readlink,
  .mknod          = op_mknod,
  .mkdir          = op_mkdir,
  .unlink         = op_unlink,
  .rmdir          = op_rmdir,
  .symlink        = op_symlink,
  .rename         = op_rename,
  .link           = op_link,
  .chmod          = op_chmod,
  .chown          = op_chown,
  .truncate       = op_truncate,
  .open           = op_open,
  .read           = op_read,
  .write          = op_write,
  .statfs         = op_statfs,
  .flush          = op_flush,
  .release	      = op_release,
  .fsync          = op_fsync,
  .setxattr       = op_setxattr,
  .getxattr       = op_getxattr,
  .listxattr      = op_listxattr,
  .removexattr    = op_removexattr,
  .opendir        = op_open,
  .readdir        = op_readdir,
  .releasedir     = op_release,
  .fsyncdir       = op_fsync,
  .init		        = op_init,
  .destroy	      = op_destroy,
  .access         = op_access,
  .create         = op_create,
  .lock           = op_lock,
  .utimens        = op_utimens,
};

/* Mount parameters */
static const char *sopt = "t:h";
static const struct option lopt[] = {
    { "type",     required_argument,    NULL,   't'},
    { "help",     no_argument,          NULL,   'h'},
    { NULL,       0,                    NULL,    0 }
};

/*
 * Usage function.
 */
static void usage(char *prog_name)
{
  printf("Usage: %s -t fstype <image_file> <mount_point>\n", prog_name);
}

/*
 * Parse options.
 */
static int parse_options(int argc, char **argv, struct vfs_data_t *vfs_data)
{
  char *fs_type = NULL;
  int c;

  /* reset VFS data */
  memset(vfs_data, 0, sizeof(struct vfs_data_t));

  /* parse options */
  while ((c = getopt_long(argc, argv, sopt, lopt, NULL)) != -1) {
    switch (c) {
      case 'h':
        usage(argv[0]);
        exit(0);
        break;
      case 't':
        fs_type = optarg;
        break;
      default:
        break;
    }
  }

  /* get image file */
  if (optind < argc && argv[optind] != NULL)
    vfs_data->dev = strdup(argv[optind++]);

  /* get mount point */
  if (optind < argc && argv[optind] != NULL)
    vfs_data->mnt_point = strdup(argv[optind++]);

  /* image file or mount point no specified */
  if (!vfs_data->dev || !vfs_data->mnt_point) {
    usage(argv[0]);
    return -1;
  }

  /* choose file system type */
  if (strcmp(fs_type, "minix") == 0) {
    vfs_data->fs_type = VFS_MINIX_TYPE;
  } else {
    printf("VFS: Unknown file system type '%s'\n", fs_type);
    return -1;
  }
  
  return 0;
}

/*
 * Main.
 */
int main(int argc, char **argv)
{
  struct fuse_args fargs = FUSE_ARGS_INIT(0, NULL);
  struct vfs_data_t vfs_data;
  int err;

  /* init VFS block buffers and inodes */
  err = vfs_init();
  if (err) {
    printf("VFS: can't init block buffers map or inodes map\n");
    exit(err);
  }

  /* parse options */
  err = parse_options(argc, argv, &vfs_data);
  if (err)
    exit(err);

  /* add fuse options */
  if (fuse_opt_add_arg(&fargs, argv[0]) == -1
      || fuse_opt_add_arg(&fargs, "-f") == -1
      || fuse_opt_add_arg(&fargs, "-s") == -1
      || fuse_opt_add_arg(&fargs, vfs_data.mnt_point) == -1) {
    fuse_opt_free_args(&fargs);
    goto err;
  }

  return fuse_main(fargs.argc, fargs.argv, &vfs_ops, &vfs_data);
err:
  free(vfs_data.dev);
  free(vfs_data.mnt_point);
  return -1;
}
