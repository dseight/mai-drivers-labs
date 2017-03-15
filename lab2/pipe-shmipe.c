#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/circ_buf.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/cred.h>
#include <linux/uidgid.h>
#include <linux/wait.h>
#include <asm/uaccess.h>

static unsigned int buf_size = 4096;
module_param(buf_size, uint, 0444);
MODULE_PARM_DESC(buf_size,
	"circular buffer size, only nonzero power of 2 allowed (default 4096)");

static ssize_t pipe_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t pipe_write(struct file *, const char __user *, size_t, loff_t *);
static int pipe_open(struct inode *, struct file *);
static int pipe_release(struct inode *, struct file *);

static ssize_t pipe_read_root(struct file *, char __user *, size_t, loff_t *);
static ssize_t pipe_write_root(struct file *, const char __user *, size_t, loff_t *);
static int pipe_release_root(struct inode *, struct file *);

static const struct file_operations fops = {
	.owner = THIS_MODULE,
	.read = pipe_read,
	.write = pipe_write,
	.open = pipe_open,
	.release = pipe_release,
};

static const struct file_operations fops_root = {
	.owner = THIS_MODULE,
	.read = pipe_read_root,
	.write = pipe_write_root,
	.release = pipe_release_root
};

static int major;

struct pipe_user {
	kuid_t uid;

	/* Task count acessing driver for current user */
	unsigned int count;

	/* Circular buffer */
	char *buf;
	int buf_head;
	int buf_tail;

	struct list_head head;
};

static LIST_HEAD(user_list);
static DECLARE_WAIT_QUEUE_HEAD(wait_queue);


static int pipe_open(struct inode *inode, struct file *file)
{
	struct pipe_user *userp;
	char *buf;

	if (uid_eq(current_uid(), GLOBAL_ROOT_UID)) {
		file->f_op = &fops_root;
		pr_warn(KBUILD_MODNAME ": file opened by root!\n");
		return 0;
	}

	file->f_op = &fops;

	/* Now let's find or create buffer for current user */

	list_for_each_entry(userp, &user_list, head) {
		if (uid_eq(current_uid(), userp->uid)) {
			userp->count++;
			file->private_data = userp;
			pr_info(KBUILD_MODNAME
				": uid %d count increased (%d now)\n",
				__kuid_val(current_uid()), userp->count);
			return 0;
		}
	}

	userp = kmalloc(sizeof(*userp), GFP_KERNEL);
	if (userp == NULL)
		return -ENOMEM;

	buf = kmalloc(buf_size, GFP_KERNEL);
	if (buf == NULL) {
		kfree(userp);
		return -ENOMEM;
	}

	userp->buf = buf;
	userp->buf_head = 0;
	userp->buf_tail = 0;
	userp->uid = current_uid();
	userp->count = 1;

	list_add(&userp->head, &user_list);
	pr_info(KBUILD_MODNAME ": uid %d added\n", __kuid_val(current_uid()));

	file->private_data = userp;

	return 0;
}

static int pipe_release(struct inode *inode, struct file *file)
{
	struct pipe_user *userp = file->private_data;

	if (userp->count > 1
		|| CIRC_CNT(userp->buf_head, userp->buf_tail, buf_size) > 0) {
		userp->count--;
		pr_info(KBUILD_MODNAME ": uid %d count decreased (%d now)\n",
			__kuid_val(current_uid()), userp->count);
		return 0;
	}

	list_del(&userp->head);

	kfree(userp->buf);
	kfree(userp);

	pr_info(KBUILD_MODNAME ": uid %d removed\n",
		__kuid_val(current_uid()));

	return 0;
}

static int pipe_release_root(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t
pipe_read(struct file *file, char __user *buf, size_t count, loff_t *offp)
{
	struct pipe_user *userp = file->private_data;
	size_t avail;
	size_t to_copy;
	int ret;

	ret = wait_event_interruptible(wait_queue,
		CIRC_CNT(userp->buf_head, userp->buf_tail, buf_size) > 0);

	if (ret)
		return -ERESTARTSYS;

	count = CIRC_CNT(userp->buf_head, userp->buf_tail, buf_size);

	avail = CIRC_CNT_TO_END(userp->buf_head, userp->buf_tail, buf_size);
	to_copy = min(count, avail);

	/* Read first part */
	if (copy_to_user(buf, userp->buf + userp->buf_tail, to_copy))
		return -EFAULT;

	/* Read second part */
	if (copy_to_user(buf + to_copy, userp->buf, count - to_copy))
		return -EFAULT;

	userp->buf_tail = (userp->buf_tail + count) & (buf_size - 1);

	wake_up(&wait_queue);

	return count;
}

static ssize_t
pipe_write(struct file *file, const char __user *buf, size_t count, loff_t *offp)
{
	struct pipe_user *userp = file->private_data;
	size_t avail;
	size_t to_copy;
	int ret;

	if (count >= buf_size)
		count = buf_size - 1;

	ret = wait_event_interruptible(wait_queue,
		CIRC_SPACE(userp->buf_head, userp->buf_tail, buf_size) >= count);

	if (ret)
		return -ERESTARTSYS;

	avail = CIRC_SPACE_TO_END(userp->buf_head, userp->buf_tail, buf_size);
	to_copy = min(count, avail);

	/* Write first part */
	if (copy_from_user(userp->buf + userp->buf_head, buf, to_copy))
		return -EFAULT;

	/* Write second part */
	if (copy_from_user(userp->buf, buf + to_copy, count - to_copy))
		return -EFAULT;

	userp->buf_head = (userp->buf_head + count) & (buf_size - 1);

	wake_up(&wait_queue);

	return count;
}

static ssize_t
pipe_read_root(struct file *file, char __user *buf, size_t count, loff_t *offp)
{
	pr_warn(KBUILD_MODNAME ": only mere mortals can read from here\n");

	return 0;
}

static ssize_t
pipe_write_root(struct file *file, const char __user *buf, size_t count, loff_t *offp)
{
	pr_warn(KBUILD_MODNAME ": only mere mortals can write here\n");

	return -EFAULT;
}

static int __init pipe_init(void)
{
	if (!buf_size || buf_size & (buf_size - 1)) {
		pr_err("buf_size must be nonzero power of 2\n");
		return -EINVAL;
	}

	major = register_chrdev(0, "pipe-shmipe", &fops);

	if (major < 0) {
		pr_err("failed to register major device number\n");
		return major;
	}

	pr_info(KBUILD_MODNAME ": loaded\n");

	return 0;
}

static void __exit pipe_exit(void)
{
	struct pipe_user *userp;
	struct pipe_user *tmp;

	list_for_each_entry_safe(userp, tmp, &user_list, head) {
		kfree(userp->buf);
		kfree(userp);
	}

	unregister_chrdev(major, "pipe-shmipe");

	pr_info(KBUILD_MODNAME ": removed\n");
}

module_init(pipe_init);
module_exit(pipe_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Pipe module");
MODULE_AUTHOR("Dmitry Gerasimov <di.gerasimov@gmail.com>");
