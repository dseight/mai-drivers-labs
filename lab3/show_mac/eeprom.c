/* Set of functions to work with Intel PRO/1000 (82540EM) EEPROM. */

#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/delay.h>
#include <asm/io.h>

#include "eeprom.h"

static int e1000_acquire_eeprom(u8 __iomem *);
static void e1000_release_eeprom(u8 __iomem *);

static u16 e1000_shift_in_ee_bits(u8 __iomem *, u16);
static void e1000_shift_out_ee_bits(u8 __iomem *, u16, u16);
static void e1000_lower_ee_clk(u8 __iomem *, u32 *);
static void e1000_raise_ee_clk(u8 __iomem *, u32 *);

/* Register Set.
 *
 * Registers are defined to be 32 bits and  should be accessed as 32 bit values.
 * These registers are physically located on the NIC, but are mapped into the
 * host memory address space.
 *
 * RW - register is both readable and writable
 * RO - register is read only
 * WO - register is write only
 */
#define E1000_STATUS   0x00008	/* Device Status - RO */
#define E1000_EECD     0x00010	/* EEPROM/Flash Control - RW */

/* EEPROM/Flash Control */
#define E1000_EECD_SK        0x00000001	/* EEPROM Clock */
#define E1000_EECD_CS        0x00000002	/* EEPROM Chip Select */
#define E1000_EECD_DI        0x00000004	/* EEPROM Data In */
#define E1000_EECD_DO        0x00000008	/* EEPROM Data Out */
#define E1000_EECD_REQ       0x00000040	/* EEPROM Access Request */
#define E1000_EECD_GNT       0x00000080	/* EEPROM Access Grant */
#define E1000_EECD_PRES      0x00000100	/* EEPROM Present */
#define E1000_EECD_SIZE      0x00000200	/* EEPROM Size (0=64 word 1=256 word) */

#define EEPROM_GRANT_ATTEMPTS 1000	/* EEPROM # attempts to gain grant */

/* EEPROM Commands - Microwire */
#define EEPROM_READ_OPCODE_MICROWIRE  0x6	/* EEPROM read opcode */
#define EEPROM_WRITE_OPCODE_MICROWIRE 0x5	/* EEPROM write opcode */
#define EEPROM_ERASE_OPCODE_MICROWIRE 0x7	/* EEPROM erase opcode */

/* 82540EM specific EEPROM */
#define EEPROM_DELAY_USEC    50
#define EEPROM_OPCODE_BITS   3

#define E1000_WRITE_FLUSH() ioread32(hw_addr + E1000_STATUS)

/**
 * e1000_read_eeprom - Reads a 16 bit word from the EEPROM.
 * @hw_addr: Address of mapped pci device memory
 * @offset: offset of word in the EEPROM to read
 * @data: word read from the EEPROM
 */
int e1000_read_eeprom(u8 __iomem *hw_addr, u16 offset, u16 *data)
{
	u16 address_bits;
	u32 eecd = ioread32(hw_addr + E1000_EECD);

	if (e1000_acquire_eeprom(hw_addr))
		return -1;

	address_bits = eecd & E1000_EECD_SIZE ? 8 : 6;

	/* Send the READ command (opcode + addr)  */
	e1000_shift_out_ee_bits(hw_addr, EEPROM_READ_OPCODE_MICROWIRE,
				EEPROM_OPCODE_BITS);
	e1000_shift_out_ee_bits(hw_addr, offset, address_bits);

	/* Read the data.  For microwire, each word requires the
	 * overhead of eeprom setup and tear-down.
	 */
	*data = e1000_shift_in_ee_bits(hw_addr, 16);

	e1000_release_eeprom(hw_addr);

	return 0;
}

/**
 * e1000_raise_ee_clk - Raises the EEPROM's clock input.
 * @hw_addr: Address of mapped pci device memory
 * @eecd: EECD's current value
 */
static void e1000_raise_ee_clk(u8 __iomem *hw_addr, u32 *eecd)
{
	/* Raise the clock input to the EEPROM (by setting the SK bit), and then
	 * wait <delay> microseconds.
	 */
	*eecd = *eecd | E1000_EECD_SK;
	iowrite32(*eecd, hw_addr + E1000_EECD);
	E1000_WRITE_FLUSH();
	udelay(EEPROM_DELAY_USEC);
}

/**
 * e1000_lower_ee_clk - Lowers the EEPROM's clock input.
 * @hw_addr: Address of mapped pci device memory
 * @eecd: EECD's current value
 */
static void e1000_lower_ee_clk(u8 __iomem *hw_addr, u32 *eecd)
{
	/* Lower the clock input to the EEPROM (by clearing the SK bit), and
	 * then wait 50 microseconds.
	 */
	*eecd = *eecd & ~E1000_EECD_SK;
	iowrite32(*eecd, hw_addr + E1000_EECD);
	E1000_WRITE_FLUSH();
	udelay(EEPROM_DELAY_USEC);
}

/**
 * e1000_shift_out_ee_bits - Shift data bits out to the EEPROM.
 * @hw_addr: Address of mapped pci device memory
 * @data: data to send to the EEPROM
 * @count: number of bits to shift out
 */
static void e1000_shift_out_ee_bits(u8 __iomem *hw_addr, u16 data, u16 count)
{
	u32 eecd;
	u32 mask;

	/* We need to shift "count" bits out to the EEPROM. So, value in the
	 * "data" parameter will be shifted out to the EEPROM one bit at a time.
	 * In order to do this, "data" must be broken down into bits.
	 */
	mask = 0x01 << (count - 1);
	eecd = ioread32(hw_addr + E1000_EECD);
	eecd &= ~E1000_EECD_DO;

	do {
		/* A "1" is shifted out to the EEPROM by setting bit "DI" to a
		 * "1", and then raising and then lowering the clock (the SK bit
		 * controls the clock input to the EEPROM).  A "0" is shifted
		 * out to the EEPROM by setting "DI" to "0" and then raising and
		 * then lowering the clock.
		 */
		eecd &= ~E1000_EECD_DI;

		if (data & mask)
			eecd |= E1000_EECD_DI;

		iowrite32(eecd, hw_addr + E1000_EECD);
		E1000_WRITE_FLUSH();

		udelay(EEPROM_DELAY_USEC);

		e1000_raise_ee_clk(hw_addr, &eecd);
		e1000_lower_ee_clk(hw_addr, &eecd);

		mask = mask >> 1;

	} while (mask);

	/* We leave the "DI" bit set to "0" when we leave this routine. */
	eecd &= ~E1000_EECD_DI;
	iowrite32(eecd, hw_addr + E1000_EECD);
}

/**
 * e1000_shift_in_ee_bits - Shift data bits in from the EEPROM
 * @hw_addr: Address of mapped pci device memory
 * @count: number of bits to shift in
 */
static u16 e1000_shift_in_ee_bits(u8 __iomem *hw_addr, u16 count)
{
	u32 eecd;
	u32 i;
	u16 data;

	/* In order to read a register from the EEPROM, we need to shift 'count'
	 * bits in from the EEPROM. Bits are "shifted in" by raising the clock
	 * input to the EEPROM (setting the SK bit), and then reading the value
	 * of the "DO" bit.  During this "shifting in" process the "DI" bit
	 * should always be clear.
	 */

	eecd = ioread32(hw_addr + E1000_EECD);

	eecd &= ~(E1000_EECD_DO | E1000_EECD_DI);
	data = 0;

	for (i = 0; i < count; i++) {
		data = data << 1;
		e1000_raise_ee_clk(hw_addr, &eecd);

		eecd = ioread32(hw_addr + E1000_EECD);

		eecd &= ~(E1000_EECD_DI);
		if (eecd & E1000_EECD_DO)
			data |= 1;

		e1000_lower_ee_clk(hw_addr, &eecd);
	}

	return data;
}

/**
 * e1000_acquire_eeprom - Prepares EEPROM for access
 * @hw: Address of mapped pci device memory
 *
 * Lowers EEPROM clock. Clears input pin. Sets the chip select pin. This
 * function should be called before issuing a command to the EEPROM.
 */
static int e1000_acquire_eeprom(u8 __iomem *hw_addr)
{
	u32 eecd = ioread32(hw_addr + E1000_EECD);
	unsigned int i = 0;

	eecd |= E1000_EECD_REQ;
	iowrite32(eecd, hw_addr + E1000_EECD);
	while ((!(eecd & E1000_EECD_GNT)) &&
	    (i < EEPROM_GRANT_ATTEMPTS)) {
		i++;
		udelay(5);
		eecd = ioread32(hw_addr + E1000_EECD);
	}
	if (!(eecd & E1000_EECD_GNT)) {
		eecd &= ~E1000_EECD_REQ;
		iowrite32(eecd, hw_addr + E1000_EECD);
		pr_err("Could not acquire EEPROM grant\n");
		return -1;
	}

	/* Clear SK and DI */
	eecd &= ~(E1000_EECD_DI | E1000_EECD_SK);
	iowrite32(eecd, hw_addr + E1000_EECD);

	/* Set CS */
	eecd |= E1000_EECD_CS;
	iowrite32(eecd, hw_addr + E1000_EECD);

	return 0;
}

/**
 * e1000_release_eeprom - drop chip select
 * @hw: Struct containing variables accessed by shared code
 *
 * Terminates a command by inverting the EEPROM's chip select pin
 */
static void e1000_release_eeprom(u8 __iomem *hw_addr)
{
	u32 eecd = ioread32(hw_addr + E1000_EECD);

	/* cleanup eeprom */

	/* CS on Microwire is active-high */
	eecd &= ~(E1000_EECD_CS | E1000_EECD_DI);

	iowrite32(eecd, hw_addr + E1000_EECD);

	/* Rising edge of clock */
	eecd |= E1000_EECD_SK;
	iowrite32(eecd, hw_addr + E1000_EECD);
	E1000_WRITE_FLUSH();
	udelay(EEPROM_DELAY_USEC);

	/* Falling edge of clock */
	eecd &= ~E1000_EECD_SK;
	iowrite32(eecd, hw_addr + E1000_EECD);
	E1000_WRITE_FLUSH();
	udelay(EEPROM_DELAY_USEC);

	/* Stop requesting EEPROM access */
	eecd &= ~E1000_EECD_REQ;
	iowrite32(eecd, hw_addr + E1000_EECD);
}
