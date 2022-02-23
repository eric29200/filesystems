CFLAGS  := -Wall -g
LDFLAGS	:= `pkg-config --libs fuse3`
CC      := gcc 

all: fmounter

fmounter: vfs/buffer_head.o vfs/super.o vfs/inode.o vfs/namei.o vfs/open.o vfs/read_write.o vfs/readdir.o vfs/stat.o vfs/access.o \
	minix/super.o minix/bitmap.o minix/inode.o minix/namei.o minix/symlink.o minix/truncate.o minix/read_write.o minix/readdir.o \
	fmounter.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

.o: .c
	$(CC) $(CFLAGS) -c $^

test: fmounter
	dd if=/dev/zero of=./test.img bs=4096 count=1024
	mkfs.minix ./test.img
	-umount ./mnt
	-mkdir ./mnt
	./fmounter -t minixfs `pwd`/test.img ./mnt

clean :
	rm -f *.o */*.o fmounter
