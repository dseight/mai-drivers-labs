#!/bin/sh

module="e1000_show_mac"

if [ "$(lsmod | grep 'e1000 ')" ]; then
	rmmod e1000
fi

if [ "$(lsmod | grep ${module})" ]; then
	echo "Module already loaded"
	exit 0
fi

insmod ${module}.ko
