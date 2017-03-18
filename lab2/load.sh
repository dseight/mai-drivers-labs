#!/bin/sh

module="pipe-shmipe"

insmod ${module}.ko $@
major=$(cat /proc/devices | grep $module | sed 's/[ \t]//')
mknod -m=0666 /dev/$module c $major 0
