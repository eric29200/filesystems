CFLAGS  := -O2 -Wall -g
LDFLAGS	:= `pkg-config --libs fuse3` -lm
CC      := gcc 

all: fmounter mkfs.minix mkfs.bfs

mkfs.minix: minix/mkfs.minix.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

mkfs.bfs: bfs/mkfs.bfs.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

fmounter: vfs/buffer_head.o vfs/super.o vfs/inode.o vfs/namei.o vfs/open.o vfs/read_write.o vfs/readdir.o vfs/stat.o vfs/access.o vfs/truncate.o \
	minix/super.o minix/bitmap.o minix/inode.o minix/namei.o minix/symlink.o minix/truncate.o minix/read_write.o minix/readdir.o \
	bfs/super.o bfs/inode.o bfs/namei.o bfs/read_write.o bfs/readdir.o bfs/bitmap.o bfs/truncate.o \
	ext2/super.o ext2/inode.o ext2/balloc.o ext2/ialloc.o ext2/read_write.o ext2/readdir.o ext2/namei.o ext2/truncate.o ext2/symlink.o \
	isofs/utils.o isofs/super.o isofs/inode.o isofs/namei.o isofs/readdir.o isofs/read_write.o \
	memfs/super.o memfs/inode.o memfs/namei.o memfs/readdir.o memfs/read_write.o memfs/truncate.o memfs/symlink.o \
	ftpfs/proc.o ftpfs/super.o ftpfs/inode.o ftpfs/namei.o ftpfs/readdir.o ftpfs/symlink.o ftpfs/open.o ftpfs/read_write.o \
	tarfs/super.o tarfs/inode.o tarfs/namei.o tarfs/readdir.o tarfs/read_write.o \
	fmounter.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

.o: .c
	$(CC) $(CFLAGS) -c $^

test_minix: fmounter mkfs.minix
	-umount ./mnt
	-mkdir ./mnt
	dd if=/dev/zero of=./test.img bs=20M count=1
	./mkfs.minix -3 ./test.img
	./fmounter -t minix `pwd`/test.img ./mnt

test_bfs: fmounter mkfs.bfs
	-umount ./mnt
	-mkdir ./mnt
	dd if=/dev/zero of=./test.img bs=20M count=1
	./mkfs.bfs ./test.img
	./fmounter -t bfs `pwd`/test.img ./mnt

test_ext2: fmounter
	-umount ./mnt
	-mkdir ./mnt
	dd if=/dev/zero of=./test.img bs=200M count=1
	mkfs.ext2 -b 4096 ./test.img
	./fmounter -t ext2 `pwd`/test.img ./mnt

test_isofs: fmounter
	-umount ./mnt
	-mkdir ./mnt
	genisoimage -o ./test.img .
	./fmounter -t isofs `pwd`/test.img ./mnt

test_memfs: fmounter
	-umount ./mnt
	-mkdir ./mnt
	./fmounter -t memfs ./mnt

test_ftpfs: fmounter
	-umount ./mnt
	-mkdir ./mnt
	./fmounter -t ftpfs localhost ./mnt

test_tarfs: fmounter
	-umount ./mnt
	-mkdir ./mnt
	tar -cvf test.img ~/tmp/tmp/tar/
	./fmounter -t tarfs `pwd`/test.img ./mnt

clean :
	rm -f *.o */*.o fmounter mkfs.minix mkfs.bfs
