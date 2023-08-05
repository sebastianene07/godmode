# SPDX-License-Identifier: GPL-2.0
#
# Makefile for god_mode devices
#

KERNEL_SRC ?= /lib/modules/$(shell uname -r)/build
M ?= $(shell pwd)

KBUILD_OPTIONS += CONFIG_$(m)=m

include $(KERNEL_SRC)/../private/google-modules/soc/gs/Makefile.include

obj-m = god_mode.o

modules modules_install clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) W=1 \
	$(KBUILD_OPTIONS) EXTRA_CFLAGS="$(EXTRA_CFLAGS)" KBUILD_EXTRA_SYMBOLS="$(EXTRA_SYMBOLS)" $(@)
