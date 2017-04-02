#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/serio.h>
#include <linux/interrupt.h>
#include <linux/printk.h>
#include <linux/fs.h>
#include <asm/string.h>
#include <asm/uaccess.h>

static int kbd_major;
static unsigned int kbd_irq_count;

static irqreturn_t kbd_interrupt(struct serio *serio, unsigned char data,
				   unsigned int flags)
{
	kbd_irq_count++;
	return IRQ_HANDLED;
}

static int kbd_connect(struct serio *serio, struct serio_driver *drv)
{
	int err = serio_open(serio, drv);

	if (err)
		return err;

	return 0;
}

static void kbd_disconnect(struct serio *serio)
{
	serio_close(serio);
}

static ssize_t
kbd_read(struct file *file, char __user *buf, size_t count, loff_t *offp)
{
	size_t len;
	char *irq_count;

	irq_count = kasprintf(GFP_KERNEL, "%u", kbd_irq_count);
	if (!irq_count)
		return -ENOMEM;

	len = strlen(irq_count);

	if (*offp > len)
		return 0;

	count = min(count, (size_t)(len - *offp));

	if (copy_to_user(buf, irq_count + *offp, count))
		return -EFAULT;

	*offp += count;
	return count;
}

static struct serio_device_id kbd_serio_ids[] = {
	{
		.type	= SERIO_8042,
		.proto	= SERIO_ANY,
		.id	= SERIO_ANY,
		.extra	= SERIO_ANY,
	},
	{
		.type	= SERIO_8042_XL,
		.proto	= SERIO_ANY,
		.id	= SERIO_ANY,
		.extra	= SERIO_ANY,
	},
	{
		.type	= SERIO_RS232,
		.proto	= SERIO_PS2SER,
		.id	= SERIO_ANY,
		.extra	= SERIO_ANY,
	},
	{ }
};
MODULE_DEVICE_TABLE(serio, kbd_serio_ids);

static struct serio_driver kbd_drv = {
	.driver		= {
		.name	= "kbd_irq",
	},
	.id_table	= kbd_serio_ids,
	.interrupt	= kbd_interrupt,
	.connect	= kbd_connect,
	.disconnect	= kbd_disconnect
};

static const struct file_operations kbd_fops = {
	.owner = THIS_MODULE,
	.read = kbd_read,
};

static int __init kbd_init(void)
{
	kbd_major = register_chrdev(0, "kbd_irq", &kbd_fops);
	if (kbd_major < 0) {
		pr_err("failed to register major device number\n");
		return kbd_major;
	}

	return serio_register_driver(&kbd_drv);
}

static void __exit kbd_exit(void)
{
	unregister_chrdev(kbd_major, "kbd_irq");
	serio_unregister_driver(&kbd_drv);
}

module_init(kbd_init);
module_exit(kbd_exit);

MODULE_DESCRIPTION("AT keyboard interrupt count showing module");
MODULE_AUTHOR("Dmitry Gerasimov <di.gerasimov@gmail.com>");
MODULE_LICENSE("GPL");
