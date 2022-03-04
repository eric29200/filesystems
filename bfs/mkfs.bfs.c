#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "bfs.h"

/* global variables */
static char fs_volume_name[6];
static char fs_name[6];
static int fs_inodes = 0;
static char *fs_dev_name = NULL;
static int fs_dev_fd = -1;
static uint64_t fs_blocks = 0;
static uint64_t fs_inode_blocks = 0;
static uint64_t fs_data_blocks = 0;

/*
 * Print usage.
 */
static void usage(char *prog_name)
{
  printf("%s [options] <device> [blocks]\n", prog_name);
  printf("\n");
  printf("Options :\n");
  printf(" -N, --inodes <num>        number of inodes\n");
  printf(" -V, --vname <name>        volume name\n");
  printf(" -F, --fname <name>        file system name\n");
  exit(0);
}

/*
 * Parse user options.
 */
static int parse_options(int argc, char **argv)
{
  static const struct option opts[] = {
      { "inodes",         required_argument,  NULL,     'N'},
      { "vname",          required_argument,  NULL,     'V'},
      { "fname",          required_argument,  NULL,     'F'},
      { "help",           no_argument,        NULL,     'h'},
      { NULL,             0,                  NULL,     0}
  };
  int c;

  /* reset volume/fs name */
  memset(fs_volume_name, 0, 6);
  memset(fs_name, 0, 6);

  /* parse options */
  while ((c = getopt_long(argc, argv, "N:V:F:h", opts, NULL)) != -1) {
    switch (c) {
      case 'N':
        fs_inodes = atoi(optarg);
        break;
      case 'V':
        memcpy(fs_volume_name, optarg, 6);
        break;
      case 'F':
        memcpy(fs_name, optarg, 6);
        break;
      default:
        usage(argv[0]);
        break;
    }
  }

  /* get device name */
  argc -= optind;
  argv += optind;
  if (argc > 0) {
    fs_dev_name = argv[0];
    argc--;
    argv++;
  }

  /* get number of blocks */
  if (argc > 0)
    fs_blocks = atoll(argv[0]);

  /* no specified device */
  if (!fs_dev_name) {
    fprintf(stderr, "no device specified\n");
    return -EINVAL;
  }

  return 0;
}

/*
 * Open device.
 */
int open_device()
{
  struct stat statbuf;
  off_t fs_dev_size;
  int err;

  /* get stats on device */
  err = stat(fs_dev_name, &statbuf);
  if (err) {
    err = errno;
    fprintf(stderr, "can't stat() device %s\n", fs_dev_name);
    return err;
  }

  /* get device size */
  fs_dev_size = statbuf.st_size;

  /* set and check number of blocks */
  if (!fs_blocks)
    fs_blocks = fs_dev_size / BFS_BLOCK_SIZE;

  /* check number of blocks */
  if (fs_blocks > fs_dev_size / BFS_BLOCK_SIZE) {
    fprintf(stderr, "%s : requested blocks > number of available blocks\n", fs_dev_name);
    return -EINVAL;
  }

  /*
   * Compute number of inodes :
   * - number of blocks / 100
   * - minimum = 48
   * - maximum = 512
   */
  if (!fs_inodes) {
    fs_inodes = fs_blocks / 100;
    if (fs_inodes < 48)
      fs_inodes = 48;
    else if (fs_inodes > 512)
      fs_inodes = 512;
  } else if (fs_inodes > 512) {
    fprintf(stderr, "%s : too many inodes %d (maximum is 512)\n", fs_dev_name, fs_inodes);
    return -EINVAL;
  }

  /* compute inodes and data blocks */
  fs_inode_blocks = (fs_inodes * sizeof(struct bfs_inode_t) + BFS_BLOCK_SIZE - 1) / BFS_BLOCK_SIZE;
  fs_data_blocks = fs_blocks - fs_inode_blocks - 1;

  /* check number of data blocks */
  if (fs_data_blocks < 32) {
    fprintf(stderr, "%s : not enough space, need at least %lu blocks\n", fs_dev_name, fs_inode_blocks + 33);
    return -EINVAL;
  }

  /* open device */
  fs_dev_fd = open(fs_dev_name, O_RDWR);
  if (fs_dev_fd < 0) {
    err = errno;
    fprintf(stderr, "can't open() device %s\n", fs_dev_name);
    return err;
  }

  return 0;
}

/*
 * Write super block.
 */
static int write_super_block()
{
  struct bfs_super_block_t sb;

  /* set super block */
  memset(&sb, 0, sizeof(struct bfs_super_block_t));
  sb.s_magic = htole32(BFS_MAGIC);
  sb.s_start = htole32(fs_inodes * sizeof(struct bfs_inode_t) + sizeof(struct bfs_super_block_t));
  sb.s_end = htole32(fs_blocks * BFS_BLOCK_SIZE - 1);
  sb.s_from = -1;
  sb.s_to = -1;
  sb.s_bfrom = -1;
  sb.s_bto = -1;
  memcpy(sb.s_fsname, fs_name, 6);
  memcpy(sb.s_volume, fs_volume_name, 6);

  /* write super block */
  if (write(fs_dev_fd, &sb, sizeof(struct bfs_super_block_t)) != sizeof(struct bfs_super_block_t)) {
    fprintf(stderr, "can't write super block\n");
    return -EIO;
  }

  /* print informations */
  printf("Volume name : %6s\n", fs_volume_name);
  printf("FS name : %6s\n", fs_name);
  printf("Block size : %d\n", BFS_BLOCK_SIZE);
  printf("Blocks : %lu\n", fs_blocks);
  printf("Inodes : %d\n", fs_inodes);
  printf("Inodes blocks : %ld\n", fs_inode_blocks);

  return 0;
}

/*
 * Write inodes.
 */
static int write_inodes()
{
  struct bfs_dir_entry_t de;
  struct bfs_inode_t inode;
  int i;

  /* set root inode */
  memset(&inode, 0, sizeof(struct bfs_inode_t));
  inode.i_ino = htole16(BFS_ROOT_INO);
  inode.i_sblock = htole32(fs_inode_blocks + 1);
  inode.i_eblock = htole32(fs_inode_blocks + 1 + (fs_inodes * sizeof(struct bfs_dir_entry_t) - 1) / BFS_BLOCK_SIZE);
  inode.i_eoffset = htole32((fs_inode_blocks + 1) * BFS_BLOCK_SIZE + 2 * sizeof(struct bfs_dir_entry_t) - 1);
  inode.i_vtype = htole32(BFS_VDIR);
  inode.i_mode = htole32(S_IFDIR | 0755);
  inode.i_uid = htole32(0);
  inode.i_gid = htole32(1);
  inode.i_nlink = 2;
  inode.i_atime = inode.i_mtime = inode.i_ctime = htole32(time(NULL));

  /* write root inode */
  if (write(fs_dev_fd, &inode, sizeof(struct bfs_inode_t)) != sizeof(struct bfs_inode_t)) {
    fprintf(stderr, "can't write root inode\n");
    return -EIO;
  }

  /* reset all inodes */
  memset(&inode, 0, sizeof(struct bfs_inode_t));
  for (i = 1; i < fs_inodes; i++) {
    if (write(fs_dev_fd, &inode, sizeof(struct bfs_inode_t)) != sizeof(struct bfs_inode_t)) {
      fprintf(stderr, "can't write reset inodes\n");
      return -EIO;
    }
  }

  /* seek to root directory entries */
  if (lseek(fs_dev_fd, (fs_inode_blocks + 1) * BFS_BLOCK_SIZE, SEEK_SET) == -1) {
    fprintf(stderr, "can't seek to to root directory\n");
    return -EINVAL;
  }

  /* write "." entry */
  memset(&de, 0, sizeof(struct bfs_dir_entry_t));
  de.d_ino = htole16(BFS_ROOT_INO);
  memcpy(de.d_name, ".", 1);
  if (write(fs_dev_fd, &de, sizeof(struct bfs_dir_entry_t)) != sizeof(struct bfs_dir_entry_t)) {
    fprintf(stderr, "can't write root directory entry\n");
    return -EIO;
  }

  /* write ".." entry */
  memcpy(de.d_name, "..", 2);
  if (write(fs_dev_fd, &de, sizeof(struct bfs_dir_entry_t)) != sizeof(struct bfs_dir_entry_t)) {
    fprintf(stderr, "can't write root directory entry\n");
    return -EIO;
  }

  return 0;
}

/*
 * Main.
 */
int main(int argc, char **argv)
{
  int err;

  /* parse options */
  err = parse_options(argc, argv);
  if (err)
    goto out;

  /* open device */
  err = open_device();
  if (err)
    goto out;

  /* write super block */
  err = write_super_block();
  if (err)
    goto out;

  /* write inodes */
  err = write_inodes();
  if (err)
    goto out;

out:
  /* close device */
  if (fs_dev_fd > 0)
    close(fs_dev_fd);

  return err;
}
