#!/bin/sh

module="kbd_irq"

if [ "$(lsmod | grep ${module})" ]; then
	rm /dev/$module
	rmmod $module
fi

if [ "$(lsmod | grep 'atkbd')" ]; then
	exit 0
fi

modprobe atkbd
