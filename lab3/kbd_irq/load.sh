#!/bin/sh

module="kbd_irq"

if [ "$(lsmod | grep 'atkbd')" ]; then
	rmmod atkbd
fi

if [ "$(lsmod | grep ${module})" ]; then
	echo "Module already loaded"
	exit 0
fi

insmod ${module}.ko
major=$(cat /proc/devices | grep $module | sed 's/[ \t]//')
mknod -m=0444 /dev/$module c $major 0
