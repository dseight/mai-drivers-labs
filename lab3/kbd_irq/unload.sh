#!/bin/sh

module="kbd_irq"

if [ "$(lsmod | grep ${module})" ]; then
	rm /dev/$module
	rmmod $module
fi
