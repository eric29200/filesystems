# Collection of Fuse filesystems, based on linux 2.0 VFS

## Virtual File System structure
  - **_struct buffer_head_t_** : block buffers, used to access disk
  - **_struct super_block_t_** : generic super block, describing a file system
  - **_struct inode_t_** : generic inode, describing a file on disk
  - **_struct file_t_** : generic opened file
  - **implemented system calls** : mount, umount, statfs, stat, access, chmod, chown, create, unlink, mkdir, rmdir, rename, link, readlink, symlink, read, write, lseek, getdents64, truncate, utimens

## Disk file systems
- **BFS** : SCO BFS file system
- **Minix** : Minix File System (v1, 2 and 3)
- **IsoFS** : ISO 9660 disc filesystem (read only)
- **Ext2** : 2nd extended file system

## Memory filesystems
- **MemFS** : in memory file system

## Network file systems
- **FtpFS** : mount a FTP server as a posix directory

## Special file systems
- **TarFS** : mount a TAR archive as a posix directory (read only)
