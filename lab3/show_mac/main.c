/* This module creates character device that displays MAC address of installed
 * NIC. Application limited to the 82540EM NIC that is often used in VirtualBox.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/err.h>
#include <linux/pci.h>
#include <linux/fs.h>
#include <linux/mutex.h>
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
static char *e1000_devnode(struct device *, umode_t *);

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

static int e1000_major;
static struct cdev e1000_cdev;
static struct class *e1000_class;
static char *e1000_macs[MAX_DEVICES];

static unsigned int e1000_count;
DEFINE_MUTEX(e1000_count_mutex);


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
	dev_t *dev_id;
	struct device *dev;
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

	dev_id = devm_kmalloc(&pdev->dev, sizeof(*dev_id), GFP_KERNEL);
	if (!mac)
		return -ENOMEM;

	mutex_lock(&e1000_count_mutex);

	e1000_macs[e1000_count] = mac;

	*dev_id = MKDEV(e1000_major, e1000_count);
	dev = device_create(e1000_class, &pdev->dev, *dev_id, NULL,
		"mac%u", e1000_count);
	if (IS_ERR(dev)) {
		mutex_unlock(&e1000_count_mutex);
		return PTR_ERR(dev);
	}

	e1000_count++;

	mutex_unlock(&e1000_count_mutex);

	pci_set_drvdata(pdev, dev_id);

	return 0;
}

static void e1000_remove(struct pci_dev *pdev)
{
	dev_t *dev_id = pci_get_drvdata(pdev);

	mutex_lock(&e1000_count_mutex);
	device_destroy(e1000_class, *dev_id);
	e1000_count--;
	mutex_unlock(&e1000_count_mutex);
}

static int e1000_open(struct inode *inode, struct file *file)
{
	int minor = iminor(inode);

	file->private_data = e1000_macs[minor];

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

static char *e1000_devnode(struct device *dev, umode_t *mode)
{
	if (mode)
		*mode = 0444;

	return NULL;
}

static int __init e1000_init(void)
{
	int err;
	dev_t dev_id;

	err = alloc_chrdev_region(&dev_id, 0, MAX_DEVICES, KBUILD_MODNAME);
	if (err) {
		pr_err("can't get major number\n");
		goto error;
	}

	e1000_major = MAJOR(dev_id);

	cdev_init(&e1000_cdev, &e1000_fops);
	e1000_cdev.owner = THIS_MODULE;

	err = cdev_add(&e1000_cdev, dev_id, MAX_DEVICES);
	if (err) {
		pr_err("can't add cdev\n");
		goto cleanup_alloc_chrdev_region;
	}

	e1000_class = class_create(THIS_MODULE, "e1000_class");
	if (IS_ERR(e1000_class)) {
		pr_err("can't create class\n");
		err = PTR_ERR(e1000_class);
		goto cleanup_cdev_add;
	}
	e1000_class->devnode = e1000_devnode;

	return pci_register_driver(&e1000_driver);


cleanup_cdev_add:
	cdev_del(&e1000_cdev);
cleanup_alloc_chrdev_region:
	unregister_chrdev_region(dev_id, MAX_DEVICES);
error:
	return err;
}

static void __exit e1000_exit(void)
{
	dev_t dev_id = MKDEV(e1000_major, 0);

	pci_unregister_driver(&e1000_driver);
	class_destroy(e1000_class);
	cdev_del(&e1000_cdev);
	unregister_chrdev_region(dev_id, MAX_DEVICES);
}

module_init(e1000_init);
module_exit(e1000_exit);

MODULE_DESCRIPTION("Intel(R) PRO/1000 (82540EM) MAC address showing module");
MODULE_AUTHOR("Dmitry Gerasimov <di.gerasimov@gmail.com>");
MODULE_LICENSE("GPL");
