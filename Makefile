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
	ext2/super.o ext2/inode.o ext2/balloc.o ext2/ialloc.o ext2/read_write.o ext2/readdir.o ext2/namei.o ext2/truncate.o \
	fmounter.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

.o: .c
	$(CC) $(CFLAGS) -c $^

test_minix: fmounter mkfs.minix
	dd if=/dev/zero of=./test.img bs=60M count=1
	./mkfs.minix -3 ./test.img
	-umount ./mnt
	-mkdir ./mnt
	./fmounter -t minix `pwd`/test.img ./mnt

test_bfs: fmounter mkfs.bfs
	dd if=/dev/zero of=./test.img bs=10M count=1
	./mkfs.bfs ./test.img
	-umount ./mnt
	-mkdir ./mnt
	./fmounter -t bfs `pwd`/test.img ./mnt

test_ext2: fmounter
	dd if=/dev/zero of=./test.img bs=20M count=1
	mkfs.ext2 ./test.img
	-umount ./mnt
	-mkdir ./mnt
	./fmounter -t ext2 `pwd`/test.img ./mnt

clean :
	rm -f *.o */*.o fmounter mkfs.minix mkfs.bfs
