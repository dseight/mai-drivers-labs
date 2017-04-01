#!/bin/sh

module="e1000_show_mac"

if [ "$(lsmod | grep ${module})" ]; then
	rmmod ${module}
fi

if [ "$(lsmod | grep 'e1000 ')" ]; then
	exit 0
fi

modprobe e1000
