# Collection of Fuse filesystems, based on linux 2.0 VFS

## Virtual File System structure :
  - buffer_head_t : block buffers, used to access disk
  - super_block_t : generic super block
  - inode_t : generic inode
  - file_t : generic opened file

## Disk file systems :
- BFS : SCO BFS file system
- Minix : Minix File System (v1, 2 and 3)
- IsoFS : ISO 9660 disc filesystem (read only)
- Ext2 : 2nd extended file system

## Memory filesystems :
- MemFS : in memory file system

## Network file systems :
- FtpFS : mount a FTP server as a posix directory

## Special file systems :
- TarFS : mount a TAR archive as a posix directory (read only)
