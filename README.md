MAI Drivers Labs
================

LAB WORK 2
----------

### Part A

Module that implements simple pipe with circular buffer. Two tasks of the same
user can exchange data through it. Size of the buffer can be specified in the
module parameter. Task goes to sleep if there no space or data in buffer.

### Part B

On superuser access to pipe, file operations are substituted to something
different from normal user file operations.


LAB WORK 3
----------

### Part A

Character device driver that displays MAC address of some *specific* installed
network card. With some network cards this may be difficult, thus we may display
something different. See details in [driver code](lab3/show_mac/main.c).

### Part B

Implement userspace workqueue.

### Part C

Attach interrupt handler to the ~mouse~ keyboard and display interrupt count
in character device.


LAB WORK 4
----------

Implement character device driver for reading/writing data from/to EEPROM
attached by I2C interface. Implement ioctl to get EEPROM size. If there is no
any EEPROM attached by I2C on PC, implement virtual I2C EEPROM:

`[ chardev | i2c device driver ] <-> I2C Core <-> [ i2c adapter driver | file ]`
