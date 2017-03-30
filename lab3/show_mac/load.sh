#!/bin/sh

module="e1000_show_mac"

nic_count=$(lspci | grep 82540 | wc -l)

if [ ${nic_count} -eq 0 ]; then
	echo "Intel 82540 NIC not found"
	exit 1
fi

if [ "$(lsmod | grep 'e1000 ')" ]; then
	rmmod e1000
fi

if [ "$(lsmod | grep ${module})" ]; then
	echo "Module already loaded"
	exit 2
fi

insmod ${module}.ko

major=$(cat /proc/devices | grep ${module} | sed 's/[ \t].*//')
nic_count=$(( ${nic_count} - 1 ))

for i in $(seq 0 ${nic_count}); do
	if [ -f "/dev/mac$i" ]; then
		rm "/dev/mac$i"
	fi
	mknod -m=0666 "/dev/mac$i" c $major $i
done
