#ifndef _EEPROM_H_
#define _EEPROM_H_

#include <linux/kernel.h>

int e1000_read_eeprom(u8 __iomem *hw_addr, u16 offset, u16 *data);

#endif