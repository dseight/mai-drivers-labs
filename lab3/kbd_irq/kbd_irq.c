#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/printk.h>
#include <linux/fs.h>
#include <asm/string.h>
#include <asm/uaccess.h>

#define I8042_KBD_IRQ 1

static int kbd_major;
static unsigned int kbd_irq_count;

static irqreturn_t kbd_interrupt(int irq, void *dev_id)
{
	kbd_irq_count++;
	return IRQ_NONE;
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

static const struct file_operations kbd_fops = {
	.owner = THIS_MODULE,
	.read = kbd_read,
};

static int __init kbd_init(void)
{
	int ret;

	ret = request_irq(I8042_KBD_IRQ, kbd_interrupt, IRQF_SHARED,
			  "kbd_irq", &kbd_major);
	if (ret)
		goto err_request_irq;

	ret = register_chrdev(0, "kbd_irq", &kbd_fops);
	if (ret < 0)
		goto err_register_chrdev;

	kbd_major = ret;

	return 0;

err_register_chrdev:
	pr_err("failed to register major device number\n");
	free_irq(I8042_KBD_IRQ, &kbd_major);
err_request_irq:
	return ret;
}

static void __exit kbd_exit(void)
{
	free_irq(I8042_KBD_IRQ, &kbd_major);
	unregister_chrdev(kbd_major, "kbd_irq");
}

module_init(kbd_init);
module_exit(kbd_exit);

MODULE_DESCRIPTION("AT keyboard interrupt count showing module");
MODULE_AUTHOR("Dmitry Gerasimov <di.gerasimov@gmail.com>");
MODULE_LICENSE("GPL");
