/* This module creates character device that displays MAC address of installed
 * NIC. Application limited to the 82540EM NIC that is often used in VirtualBox.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/fs.h>
#include <linux/spinlock.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#include "eeprom.h"

#define E1000_DEV_ID_82540EM     0x100E
#define E1000_DEV_ID_82540EM_LOM 0x1015


static const struct pci_device_id e1000_pci_table[] = {
	{ PCI_VDEVICE(INTEL, E1000_DEV_ID_82540EM) },
	{ PCI_VDEVICE(INTEL, E1000_DEV_ID_82540EM_LOM) },
	{ }
};
MODULE_DEVICE_TABLE(pci, e1000_pci_table);

static int e1000_probe(struct pci_dev *, const struct pci_device_id *);
static void e1000_remove(struct pci_dev *);
static int e1000_open(struct inode *, struct file *);
static ssize_t e1000_read(struct file *, char __user *, size_t, loff_t *);
static int e1000_read_mac(u8 __iomem *, char *);

static const struct file_operations e1000_fops = {
	.owner		= THIS_MODULE,
	.open		= e1000_open,
	.read		= e1000_read,
};

static struct pci_driver e1000_driver = {
	.name		= "e1000_show_mac",
	.id_table	= e1000_pci_table,
	.probe		= e1000_probe,
	.remove		= e1000_remove,
};

#define MAX_DEVICES 16
#define EMPTY_MAC "00:00:00:00:00:00"
#define MAC_ADDRESS_SIZE 6

static char *empty_mac = EMPTY_MAC;
static char *macs[MAX_DEVICES];
static int mac_index;
static spinlock_t macs_lock;
static int major;


static int e1000_read_mac(u8 __iomem *hw_addr, char *mac)
{
	u16 offset, eeprom_data;
	unsigned int i;

	for (i = 0; i < MAC_ADDRESS_SIZE; i += 2) {
		offset = i >> 1;
		if (e1000_read_eeprom(hw_addr, offset, &eeprom_data) < 0) {
			pr_err("EEPROM Read Error\n");
			return -1;
		}

		mac = hex_byte_pack_upper(mac, (u8)(eeprom_data & 0x00FF));
		*mac++ = ':';

		mac = hex_byte_pack_upper(mac, (u8)(eeprom_data >> 8));
		*mac++ = i < MAC_ADDRESS_SIZE - 2 ? ':' : '\0';
	}

	return 0;
}

static int e1000_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	int err, bars;
	char *mac;
	u8 __iomem *hw_addr;

	bars = pci_select_bars(pdev, IORESOURCE_MEM | IORESOURCE_IO);
	err = pcim_enable_device(pdev);
	if (err)
		return err;

	err = pci_request_selected_regions(pdev, bars, "e1000_show_mac");
	if (err)
		return err;

	hw_addr = pci_ioremap_bar(pdev, 0);
	if (!hw_addr) {
		pr_err("can't ioremap BAR 0\n");
		return -EIO;
	}

	mac = devm_kzalloc(&pdev->dev, sizeof(EMPTY_MAC), GFP_KERNEL);
	if (!mac)
		return -ENOMEM;

	if (e1000_read_mac(hw_addr, mac))
		return -EIO;

	spin_lock(&macs_lock);

	if (mac_index + 1 > MAX_DEVICES) {
		spin_unlock(&macs_lock);
		return -ENOMEM;
	}
	mac_index++;
	macs[mac_index] = mac;

	spin_unlock(&macs_lock);

	pci_set_drvdata(pdev, &macs[mac_index]);

	return 0;
}

static void e1000_remove(struct pci_dev *pdev)
{
	char **mac = pci_get_drvdata(pdev);
	*mac = NULL;
}

static int e1000_open(struct inode *inode, struct file *file)
{
	int minor = iminor(inode);

	/* Searching for device with specific minor */

	if (minor >= MAX_DEVICES || macs[minor] == NULL)
		file->private_data = empty_mac;
	else
		file->private_data = macs[minor];

	return 0;
}

static ssize_t
e1000_read(struct file *file, char __user *buf, size_t count, loff_t *offp)
{
	char *mac_address = file->private_data;

	if (*offp > ARRAY_SIZE(EMPTY_MAC))
		return 0;

	count = min(count, (size_t) (ARRAY_SIZE(EMPTY_MAC) - *offp));

	if (copy_to_user(buf, mac_address + *offp, count))
		return -EFAULT;

	*offp += count;
	return count;
}

static int __init e1000_init(void)
{
	mac_index = -1;
	major = register_chrdev(0, "e1000_show_mac", &e1000_fops);

	if (major < 0) {
		pr_err("failed to register major device number\n");
		return major;
	}

	return pci_register_driver(&e1000_driver);
}

static void __exit e1000_exit(void)
{
	unregister_chrdev(major, "e1000_show_mac");
	pci_unregister_driver(&e1000_driver);
}

module_init(e1000_init);
module_exit(e1000_exit);

MODULE_DESCRIPTION("Intel(R) PRO/1000 (82540EM) MAC address showing module");
MODULE_AUTHOR("Dmitry Gerasimov <di.gerasimov@gmail.com>");
MODULE_LICENSE("GPL");
