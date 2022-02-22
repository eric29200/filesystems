#define FUSE_USE_VERSION  32
#include <fuse3/fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>

#include "vfs/vfs.h"

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

static int op_readlink(const char *pathname, char *buf, size_t bufsize)
{
  fprintf(stderr, "getattr not implemented\n");
  return -ENOSYS;
}

static int op_mknod(const char *pathname, mode_t mode, dev_t dev)
{
  fprintf(stderr, "mnod not implemented\n");
  return -ENOSYS;
}

static int op_mkdir(const char *pathname, mode_t mode)
{
  fprintf(stderr, "mkdir not implemented\n");
  return -ENOSYS;
}

static int op_unlink(const char *pathname)
{
  fprintf(stderr, "unlink not implemented\n");
  return -ENOSYS;
}

static int op_rmdir(const char *pathname)
{
  fprintf(stderr, "rmdir not implemented\n");
  return -ENOSYS;
}

static int op_symlink(const char *target, const char *linkpath)
{
  fprintf(stderr, "symlink not implemented\n");
  return -ENOSYS;
}

static int op_rename(const char *oldpath, const char *newpath, unsigned int flags)
{
  fprintf(stderr, "rename not implemented\n");
  return -ENOSYS;
}

static int op_link(const char *oldpath, const char *newpath)
{
  fprintf(stderr, "link not implemented\n");
  return -ENOSYS;
}

static int op_chmod(const char *pathname, mode_t mode, struct fuse_file_info *fi)
{
  fprintf(stderr, "chmod not implemented\n");
  return -ENOSYS;
}

static int op_chown(const char *pathname, uid_t uid, gid_t gid, struct fuse_file_info *fi)
{
  fprintf(stderr, "chown not implemented\n");
  return -ENOSYS;
}

static int op_truncate(const char *pathname, off_t length, struct fuse_file_info *fi)
{
  fprintf(stderr, "truncate not implemented\n");
  return -ENOSYS;
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

static int op_read(const char *pathname, char *buf, size_t length, off_t offset, struct fuse_file_info *fi)
{
  fprintf(stderr, "read not implemented\n");
  return -ENOSYS;
}

static int op_write(const char *pathname, const char *buf, size_t length, off_t offset, struct fuse_file_info *fi)
{
  fprintf(stderr, "write not implemented\n");
  return -ENOSYS;
}

static int op_statfs(const char *pathname, struct statvfs *statbuf)
{
  fprintf(stderr, "statfs not implemented\n");
  return -ENOSYS;
}

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

static int op_fsync(const char *pathname, int data_sync, struct fuse_file_info *fi)
{
  fprintf(stderr, "fsync not implemented\n");
  return -ENOSYS;
}

static int op_setxattr(const char *pathname, const char *name, const char *value, size_t size, int flags)
{
  fprintf(stderr, "setxattr not implemented\n");
  return -ENOSYS;
}

static int op_getxattr(const char *pathname, const char *name, char *value, size_t size)
{
  fprintf(stderr, "getxattr not implemented\n");
  return -ENOSYS;
}

static int op_listxattr(const char *pathname, char *list, size_t size)
{
  fprintf(stderr, "listxattr not implemented\n");
  return -ENOSYS;
}

static int op_removexattr(const char *pathname, const char *name)
{
  fprintf(stderr, "removexattr not implemented\n");
  return -ENOSYS;
}

static int op_readdir(const char *pathname, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi,
                      enum fuse_readdir_flags flags)
{
  fprintf(stderr, "readdir not implemented\n");
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
  vfs_data->sb = vfs_mount(vfs_data->dev, VFS_MINIXFS_TYPE);
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

static int op_access(const char *pathname, int mask)
{
  fprintf(stderr, "access not implemented\n");
  return -ENOSYS;
}

static int op_create(const char *pathname, mode_t mode, struct fuse_file_info *fi)
{
  fprintf(stderr, "create not implemented\n");
  return -ENOSYS;
}

static int op_lock(const char *pathname, struct fuse_file_info *fi, int cmd, struct flock *lock)
{
  fprintf(stderr, "lock not implemented\n");
  return -ENOSYS;
}

static int op_utimens(const char *pathname, const struct timespec tv[2], struct fuse_file_info *fi)
{
  fprintf(stderr, "utimens not implemented\n");
  return -ENOSYS;
}

static int op_bmap(const char *pathname, size_t blocksize, uint64_t *idx)
{
  fprintf(stderr, "bmap not implemented\n");
  return -ENOSYS;
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
  .bmap           = op_bmap,
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
  if (strcmp(fs_type, "minixfs") == 0) {
    vfs_data->fs_type = VFS_MINIXFS_TYPE;
  } else {
    printf("Unknown file system type '%s'\n", fs_type);
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
  int ret;

  /* parse options */
  ret = parse_options(argc, argv, &vfs_data);
  if (ret)
    exit(ret);

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
