#!/bin/sh

module="i2c_chardev"
address=0x13

modprobe i2c_stub chip_addr=$address

if [[ "$(lsmod | grep ${module})" ]]; then
	echo "Module already loaded"
	exit 0
fi

insmod $module chip_addr=$address
