#include <errno.h>
#include <fcntl.h>

#include "vfs.h"

/*
 * Follow a link.
 */
static struct inode_t *vfs_follow_link(struct inode_t *dir, struct inode_t *inode)
{
  struct inode_t *res_inode;
  
  /* follow link not implemented : return inode */
  if (!inode || !inode->i_op || !inode->i_op->follow_link)
    return inode;
  
  /* follow link */
  inode->i_op->follow_link(dir, inode, &res_inode);
  return res_inode;
} 

/*
 * Resolve a path name to the inode of the top most directory.
 */
static struct inode_t *vfs_dir_namei(struct inode_t *root, struct inode_t *dir, const char *pathname,
                                     const char **basename, size_t *basename_len)
{
  struct inode_t *inode, *tmp;
  const char *name;
  size_t name_len;
  int err;
  
  /* absolute path */
  if (*pathname == '/') {
    pathname++;
    inode = root;
  } else {
    inode = dir;
  }
  
  if (!inode)
    return NULL;
  
  /* update reference count */
  inode->i_ref++;
  
  /* resolve path */
  for (;;) {
    /* check if inode is a directory */
    if (!S_ISDIR(inode->i_mode)) {
      vfs_iput(inode);
      return NULL;
    }
             
    /* compute next path name */
    name = pathname;
    for (name_len = 0; *pathname && *pathname++ != '/'; name_len++);
    
    /* end of path */
    if (!*pathname)
      break;
    
    /* skip empty folder */
    if (!name_len)
      continue;
    
    /* lookup not implemented */
    inode->i_ref++;
    if (!inode->i_op || !inode->i_op->lookup) {
      vfs_iput(inode);
      return NULL;
    }
    
    /* lookup file */
    err = inode->i_op->lookup(inode, name, name_len, &tmp);
    if (err) {
      vfs_iput(inode);
      return NULL;
    }
             
    /* follow symbolic link */
    inode = vfs_follow_link(inode, tmp);
    if (!inode)
      return NULL;
  }
  
  /* set basename */
  *basename = name;
  *basename_len = name_len;

  return inode;
}

/*
 * Resolve a path name to an inode.
 */
struct inode_t *vfs_namei(struct inode_t *root, struct inode_t *base, const char *pathname, int follow_links)
{
  struct inode_t *dir, *inode;
  const char *basename;
  size_t basename_len;
  int err;
  
  /* resolve directory */
  dir = vfs_dir_namei(root, base, pathname, &basename, &basename_len);
  if (!dir)
    return NULL;
  
  /* special case : '/' */
  if (!basename_len)
    return dir;
  
  /* lookup not implemented */
  if (!dir->i_op || !dir->i_op->lookup) {
    vfs_iput(dir);
    return NULL;
  }
  
  /* lookup file */
  dir->i_ref++;
  err = dir->i_op->lookup(dir, basename, basename_len, &inode);
  if (err) {
    vfs_iput(dir);
    return NULL;
  }
  
  /* follow symbolic link */
  if (follow_links)
    inode = vfs_follow_link(dir, inode);
    
  /* release directory */
  vfs_iput(dir);

  return inode;
}

/*
 * Resolve and open a file.
 */
int vfs_open_namei(struct inode_t *root, const char *pathname, int flags, mode_t mode, struct inode_t **res_inode)
{
  struct inode_t *dir, *inode;
  const char *basename;
  size_t basename_len;
  int err;
  
  /* get directory */
  dir = vfs_dir_namei(root, NULL, pathname, &basename, &basename_len);
  if (!dir)
    return -ENOENT;
  
  /* open a directory */
  if (!basename_len) {
    /* do not allow to create/truncate directory here */
    if (!(flags & (O_ACCMODE | O_CREAT | O_TRUNC))) {
      *res_inode = dir;
      return 0;
    }
                       
    vfs_iput(dir);
    return -EISDIR;
  }
  
  /* set mode (needed if a new file is created) */
  mode |= S_IFREG;
  
  /* lookup not implemented */
  if (!dir->i_op || !dir->i_op->lookup) {
    vfs_iput(dir);
    return -EPERM;
  }
  
  /* lookup inode */
  dir->i_ref++;
  err = dir->i_op->lookup(dir, basename, basename_len, &inode);
  
  /* no such entry : create a new one */
  if (err) {
    /* no such entry */
    if (!(flags & O_CREAT)) {
      vfs_iput(dir);
      return -ENOENT;
    }
             
    /* create not implemented */
    if (!dir->i_op || !dir->i_op->create) {
      vfs_iput(dir);
      return -EPERM;
    }
             
    /* create new inode */
    dir->i_ref++;
    err = dir->i_op->create(dir, basename, basename_len, mode, res_inode);
    
    /* release directory */
    vfs_iput(dir);
    return err;
  }
  
  /* follow symbolic link */
  *res_inode = vfs_follow_link(dir, inode);
  if (!*res_inode)
    return -EACCES;
  
  /* truncate file */
  if (flags & O_TRUNC && (*res_inode)->i_op && (*res_inode)->i_op->truncate) {
    (*res_inode)->i_size = 0;
    (*res_inode)->i_op->truncate(*res_inode);
    (*res_inode)->i_dirt = 1;
  }
  
  return 0;
}

/*
 * Create a file in a directory.
 */
int vfs_create(struct inode_t *root, const char *pathname, mode_t mode)
{
  struct inode_t *dir, *res_inode;
  const char *basename;
  size_t basename_len;
  int err;
  
  /* get directory */
  dir = vfs_dir_namei(root, NULL, pathname, &basename, &basename_len);
  if (!dir)
    return -ENOENT;
  
  /* check file name */
  if (!basename_len) {
    vfs_iput(dir);
    return -ENOENT;
  }
  
  /* create not implemented */
  if (!dir->i_op || !dir->i_op->create) {
    vfs_iput(dir);
    return -EPERM;
  }
  
  /* create file */
  dir->i_ref++;
  err = dir->i_op->create(dir, basename, basename_len, mode, &res_inode);
  
  /* release inode */
  if (!err)
    vfs_iput(res_inode);
  
  /* release directory */
  vfs_iput(dir);
  
  return err;
}

/*
 * Unlink (remove) a file.
 */
int vfs_unlink(struct inode_t *root, const char *pathname)
{
  struct inode_t *dir;
  const char *basename;
  size_t basename_len;
  
  /* get parent directory */
  dir = vfs_dir_namei(root, NULL, pathname, &basename, &basename_len);
  if (!dir)
    return -ENOENT;
  
  /* check name length */
  if (!basename_len) {
    vfs_iput(dir);
    return -ENOENT;
  }
  
  /* unlink not implemented */
  if (!dir->i_op || !dir->i_op->unlink) {
    vfs_iput(dir);
    return -EPERM;
  }
  
  /* unlink */
  return dir->i_op->unlink(dir, basename, basename_len);
}

/*
 * Make a directory.
 */
int vfs_mkdir(struct inode_t *root, const char *pathname, mode_t mode)
{
  const char *basename;
  size_t basename_len;
  struct inode_t *dir;
  int err;
  
  /* get parent directory */
  dir = vfs_dir_namei(root, NULL, pathname, &basename, &basename_len);
  if (!dir)
    return -ENOENT;
  
  /* check basename length */
  if (!basename_len) {
    vfs_iput(dir);
    return -ENOENT;
  }
  
  /* mkdir not implemented */
  if (!dir->i_op || !dir->i_op->mkdir) {
    vfs_iput(dir);
    return -EPERM;
  }
  
  /* make directory */
  dir->i_ref++;
  err = dir->i_op->mkdir(dir, basename, basename_len, mode);
  
  /* release directory */
  vfs_iput(dir);
  
  return err;
}

/*
 * Remove a directory.
 */
int vfs_rmdir(struct inode_t *root, const char *pathname)
{
  const char *basename;
  size_t basename_len;
  struct inode_t *dir;
  
  /* get parent directory */
  dir = vfs_dir_namei(root, NULL, pathname, &basename, &basename_len);
  if (!dir)
    return -ENOENT;
  
  /* check name length */
  if (!basename_len) {
    vfs_iput(dir);
    return -ENOENT;
  }
  
  /* rmdir not implemented */
  if (!dir->i_op || !dir->i_op->rmdir) {
    vfs_iput(dir);
    return -EPERM;
  }
  
  /* remove directory */
  return dir->i_op->rmdir(dir, basename, basename_len);
}

/*
 * Make a new name for a file (= hard link).
 */
int vfs_link(struct inode_t *root, const char *oldpath, const char *new_path)
{
  struct inode_t *old_inode, *dir;
  const char *basename;
  size_t basename_len;
  int err;
  
  /* get old inode */
  old_inode = vfs_namei(root, NULL, oldpath, 1);
  if (!old_inode)
    return -ENOENT;
  
  /* do not allow to rename directory */
  if (S_ISDIR(old_inode->i_mode)) {
    vfs_iput(old_inode);
    return -EPERM;
  }
  
  /* get directory of new file */
  dir = vfs_dir_namei(root, NULL, new_path, &basename, &basename_len);
  if (!dir) {
    vfs_iput(old_inode);
    return -EACCES;   
  }
  
  /* not name to new file */
  if (!basename_len) {
    vfs_iput(old_inode);
    vfs_iput(dir);
    return -EPERM;
  }
  
  /* link not implemented */
  if (!dir->i_op || !dir->i_op->link) {
    vfs_iput(old_inode);
    vfs_iput(dir);
    return -EPERM;
  }
  
  /* create link */
  dir->i_ref++;
  err = dir->i_op->link(old_inode, dir, basename, basename_len);
  
  /* release directory */
  vfs_iput(old_inode);
  vfs_iput(dir);
  
  return err;
}

/*
 * Create a symbolic link.
 */
int vfs_symlink(struct inode_t *root, const char *target, const char *linkpath)
{
  const char *basename;
  size_t basename_len;
  struct inode_t *dir;
  int err;
  
  /* get new parent directory */
  dir = vfs_dir_namei(root, NULL, linkpath, &basename, &basename_len);
  if (!dir)
    return -ENOENT;
  
  /* check directory name */
  if (!basename_len) {
    vfs_iput(dir);
    return -ENOENT;
  }
  
  /* symlink not implemented */
  if (!dir->i_op || !dir->i_op->symlink) {
    vfs_iput(dir);
    return -EPERM;
  }
  
  /* create symbolic link */
  dir->i_ref++;
  err = dir->i_op->symlink(dir, basename, basename_len, target);
  
  /* release directory */
  vfs_iput(dir);
  
  return err;
}

/*
 * Rename a file.
 */
int vfs_rename(struct inode_t *root, const char *oldpath, const char *newpath)
{
  size_t old_basename_len, new_basename_len;
  const char *old_basename, *new_basename;
  struct inode_t *old_dir, *new_dir;
  
  /* get old directory */
  old_dir = vfs_dir_namei(root, NULL, oldpath, &old_basename, &old_basename_len);
  if (!old_dir)
    return -ENOENT;
  
  /* do not allow to move '.' and '..' */
  if (!old_basename_len
      || (old_basename[0] == '.' && (old_basename_len == 1 || (old_basename[1] == '.' && old_basename_len == 2)))) {
    vfs_iput(old_dir);
    return -EPERM;
  }
  
  /* get new directory */
  new_dir = vfs_dir_namei(root, NULL, newpath, &new_basename, &new_basename_len);
  if (!new_dir) {
    vfs_iput(old_dir);
    return -ENOENT;
  }
  
  /* do not allow to move '.' and '..' */
  if (!new_basename_len
      || (new_basename[0] == '.' && (new_basename_len == 1 || (new_basename[1] == '.' && new_basename_len == 2)))) {
    vfs_iput(new_dir);
    vfs_iput(old_dir);
    return -EPERM;
  }
  
  /* rename not implemented */
  if (!old_dir->i_op || !old_dir->i_op->rename) {
    vfs_iput(new_dir);
    vfs_iput(old_dir);
    return -EPERM;    
  }
  
  /* rename */
  new_dir->i_ref++;
  return old_dir->i_op->rename(old_dir, old_basename, old_basename_len, new_dir, new_basename, new_basename_len);
}

/*
 * Read value of a symbolic link.
 */
ssize_t vfs_readlink(struct inode_t *root, const char *pathname, char *buf, size_t bufsize)
{
  struct inode_t *inode;
  
  /* get inode */
  inode = vfs_namei(root, NULL, pathname, 0);
  if (!inode)
    return -ENOENT;

  /* readlink not implemented */
  if (!inode->i_op || !inode->i_op->readlink) {
    vfs_iput(inode);
    return -EPERM;
  }
  
  /* read link */
  return inode->i_op->readlink(inode, buf, bufsize);
}
