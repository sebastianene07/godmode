// SPDX-License-Identifier: GPL-2.0-only
//
#define pr_fmt(fmt) "godmode: " fmt

#include <linux/device/class.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/sched/signal.h>

#define GODMODE_CHARDEV_NAME "godmode"

MODULE_DESCRIPTION("pwn express to get godmode root rights");
MODULE_AUTHOR("uglycat");
MODULE_LICENSE("GPL");

static int godmode_major;
static int godmode_major_dev;
static struct class *godmode_class;
static struct device *godmode_dev;

static int godmode_open(struct inode *inode, struct file *file);
static int godmode_release(struct inode *inode, struct file *file);
static long godmode_unlocked_ioctl(struct file *file, unsigned int cmd,
			       unsigned long arg);
static ssize_t godmode_read(struct file *file, char __user *buf, size_t count,
			loff_t *off);
static ssize_t godmode_write(struct file *file, const char __user *buf,
			 size_t count, loff_t *off);

static const struct file_operations fops = {
	.open = godmode_open,
	.release = godmode_release,
	.unlocked_ioctl = godmode_unlocked_ioctl,
	.read = godmode_read,
	.write = godmode_write,

	.owner = THIS_MODULE,
};

static int godmode_open(struct inode *inode, struct file *file)
{
	return -ENOSYS;
}

static int godmode_release(struct inode *inode, struct file *file)
{
	return -ENOSYS;
}

static long godmode_unlocked_ioctl(struct file *file, unsigned int cmd,
			       unsigned long arg)
{
	return 0L;
}

static ssize_t godmode_read(struct file *file, char __user *buf, size_t count,
			loff_t *off)
{
	return 0L;
}

static ssize_t godmode_write(struct file *file, const char __user *buf,
			 size_t count, loff_t *off)
{
	return 0L;
}

static int godmode_init(void)
{
	int ret = 0;

	godmode_major = register_chrdev(0, GODMODE_CHARDEV_NAME, &fops);
	if (godmode_major < 0) {
		pr_err("register chardev\n");
		return godmode_major;
	}

	godmode_major_dev = MKDEV(godmode_major, 0);
 	godmode_class = class_create(THIS_MODULE, GODMODE_CHARDEV_NAME);
	if (IS_ERR(godmode_class)) {
		ret = PTR_ERR(godmode_class);
		pr_err("create class\n");
		goto cleanup_unregister;
	}

	godmode_dev = device_create(godmode_class, NULL, godmode_major_dev, NULL, GODMODE_CHARDEV_NAME);	
	if (!godmode_dev) {
		class_destroy(godmode_class);
		godmode_class = NULL;
		goto cleanup_unregister;
	}

	return 0;

cleanup_unregister:
	unregister_chrdev(godmode_major, GODMODE_CHARDEV_NAME);
	godmode_major = -1;
	return ret;
}

static void godmode_exit(void)
{
	/* TODO/2: print current process pid and name */
	pr_info("Current process: pid = %d; comm = %s\n",
		current->pid, current->comm);
}

module_init(godmode_init);
module_exit(godmode_exit);
