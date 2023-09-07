## What is this ? ##

A GKI kernel module written to provide root access to unprivilged sandboxed
Android apps. Apart from that it describes the process of adding an external kernel
module to the GKI kernel. Moreover we look at the produced images and what is their
content.

The module gets a calling app root priviliges by doing the following:
	1. disable SELinux
	2. set capabilities for the calling process
	3. set root process id

## How can I integrate the modules as part of the vendor_dlkm image ##

1. Patch `private/google-modules/soc/gs/build.config.slider` with the following

```
diff --git a/build.config.slider b/build.config.slider
index 98e460d5f..70a1e81c4 100644
--- a/build.config.slider
+++ b/build.config.slider
@@ -59,6 +59,7 @@ private/google-modules/touch/sec
 private/google-modules/power/reset
 private/google-modules/bluetooth/broadcom
 private/google-modules/nfc
+private/google-modules/god_mode
 "
 # TODO(b/266414538) disable till properly fixed
 # private/google-modules/uwb/kernel
```

2. Patch `private/google-modules/soc/gs/vendor_boot_modules.slider`

```
diff --git a/vendor_boot_modules.slider b/vendor_boot_modules.slider
index bbbb0b65f..cdff51500 100644
--- a/vendor_boot_modules.slider
+++ b/vendor_boot_modules.slider
@@ -86,6 +86,7 @@ gs_acpm.ko
 gsa.ko
 gsa_gsc.ko
 gsc-spi.ko
+god_mode.ko
 gvotable.ko
 hardlockup-debug.ko
 hardlockup-watchdog.ko
```

3. Patch `private/google-modules/soc/gs/BUILD.bazel` to include the module in
the external modules compilation list when we create a build for slider:

```
diff --git a/BUILD.bazel b/BUILD.bazel
index adccf7cc0..acc3d53b2 100644
--- a/BUILD.bazel                                      
+++ b/BUILD.bazel                                      
@@ -71,6 +71,7 @@ kernel_module_group(
         "//private/google-modules/power/reset",
         "//private/google-modules/bluetooth/broadcom:bluetooth.broadcom",
         "//private/google-modules/nfc",
+       "//private/google-modules/god_mode",
     ],                                                
 )     
```

## How can I build it ##

Fetch this module in your ACK (android common kernel tree) in the following
location: private/google-modules/god_mode

Invoke the build process with the following command:

`./tools/bazel build --config=fast //private/google-modules/god_mode:god_mode`

Complete build:

For (Pixel 6 - Oriole/codename Slider)

./tools/bazel run --config=fast //private/google-modules/soc/gs:slider_dist

## How can I flash this on my device ##

I used the following script to flash my Pixel 6:

ARTIFACTS_DIR=out/slider/dist/ flash_my_pixel.sh <your_serial_from_fastboot> y

```
#!/bin/bash
#

if [ -z $ARTIFACTS_DIR ]; then
        ARTIFACTS_DIR=out/android14-5.15/dist/
        echo "Specify artifacts path as env variable like"
        echo "export ARTIFACTS_DIR="$ARTIFACTS_DIR
        exit 1
fi

echo $ARTIFACTS_DIR
STAGE_1_IMGS=(dtbo vendor_boot boot)
STAGE_2_IMGS=vendor_dlkm

function show_help() {
        echo "flash_my_pixel_kernel.sh <address> <is_code_in_vendor_dlkm>"
        echo "where <address> is reported from 'fastboot devices -l' or"
        echo "remote_device_proxy. Specify ARTIFACTS_DIR"
}

if [ -z $1 ]; then
        show_help
        exit 1
fi      

echo "My fastboot device is:" $1
fastboot -s $1 oem disable-verity
fastboot -s $1 oem disable-verification
fastboot -s $1 oem uart enable
echo "Disable verity and verification: success"
for img in ${STAGE_1_IMGS[@]}
do
        file $ARTIFACTS_DIR"/$img.img"
        fastboot -s $1 flash $img $ARTIFACTS_DIR"/$img.img"
done
echo "First stage flash done."
if [ -z $2 ]; then
        exit 0
fi

fastboot -s $1 reboot fastboot
for img in ${STAGE_2_IMGS[@]}
do
        fastboot -s $1 flash $img $ARTIFACTS_DIR"/$img.img"
done
echo "Wipe user-data\n"
fastboot -s $1 -w
echo "Final stage done. rebooting"
fastboot -s $1 reboot
```

## Content of the images ###

boot-artifacts/ramdisks/vendor_ramdisk-oriole.img - contains the ramdisk image
a in LZ4 format that wraps a CPIO archive

```
├── default.prop -> prop.default
├── dev
├── etc -> /system/etc
├── first_stage_ramdisk
│   ├── avb
│   │   ├── q-developer-gsi.avbpubkey
│   │   ├── r-developer-gsi.avbpubkey
│   │   └── s-developer-gsi.avbpubkey
│   └── system
│       ├── bin
│       │   ├── defrag.f2fs -> fsck.f2fs
│       │   ├── dump.f2fs -> fsck.f2fs
│       │   ├── e2fsck
│       │   ├── fsck.f2fs
│       │   ├── linker64
│       │   ├── linker_asan64 -> linker64
│       │   ├── resize.f2fs -> fsck.f2fs
│       │   ├── resize2fs
│       │   ├── snapuserd
│       │   └── tune2fs
│       ├── etc
│       │   ├── fstab.gs101
│       │   └── fstab.gs101-fips
│       └── lib64
│           ├── ld-android.so
│           ├── libbase.so
│           ├── libc++.so
│           ├── libc.so
│           ├── libdl.so
│           ├── libext2_blkid.so
│           ├── libext2_com_err.so
│           ├── libext2_e2p.so
│           ├── libext2_quota.so
│           ├── libext2_uuid.so
│           ├── libext2fs.so
│           ├── liblog.so
│           ├── libm.so
│           ├── libsparse.so
│           └── libz.so
├── init -> /system/bin/init
├── init.recovery.gs101.rc
├── init.recovery.oriole.rc
├── lib
│   └── firmware
│       └── drv2624.bin
├── linkerconfig
├── metadata
├── mnt
├── odm
(and so on)
```

`out/slider/dist/vendor_dlkm.img` - lib/modules/6.3.0-mainline-maybe-dirty/*.ko
^
|_ This should contain our module if everything went fine



`out/slider/dist/boot.img` - the kernel Image 


# Warning ! Warning ! Warning #
