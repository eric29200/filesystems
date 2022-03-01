CFLAGS  := -O2 -Wall -g
LDFLAGS	:= `pkg-config --libs fuse3` -lm
CC      := gcc 

all: fmounter mkfs.minix

mkfs.minix: minix/mkfs.minix.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

fmounter: vfs/buffer_head.o vfs/super.o vfs/inode.o vfs/namei.o vfs/open.o vfs/read_write.o vfs/readdir.o vfs/stat.o vfs/access.o vfs/truncate.o \
	minix/super.o minix/bitmap.o minix/inode.o minix/namei.o minix/symlink.o minix/truncate.o minix/read_write.o minix/readdir.o \
	bfs/super.o bfs/inode.o bfs/namei.o bfs/read_write.o bfs/readdir.o \
	fmounter.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

.o: .c
	$(CC) $(CFLAGS) -c $^

test_minix: fmounter mkfs.minix
	dd if=/dev/zero of=./test.img bs=40M count=1
	./mkfs.minix -3 ./test.img
	-umount ./mnt
	-mkdir ./mnt
	./fmounter -t minix `pwd`/test.img ./mnt

test_bfs: fmounter
	dd if=/dev/zero of=./test.img bs=10M count=1
	mkfs.bfs ./test.img
	-umount ./mnt
	-mkdir ./mnt
	./fmounter -t bfs `pwd`/test.img ./mnt

clean :
	rm -f *.o */*.o fmounter mkfs.minix
