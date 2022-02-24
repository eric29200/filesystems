#!/bin/csh

/usr/bin/fio --name=seqwrite --rw=write --bs=128k --size=40m --end_fsync=1 --loops=4 --directory=./mnt
