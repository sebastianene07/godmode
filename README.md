## What is this ? ##

A GKI kernel module written to provide root access to unprivilged sandboxed
Android apps.

The module gets a calling app root priviliges by doing the following:
	1. disable SELinux
	2. set capabilities for the calling process
	3. set root process id

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



# Warning ! Warning ! Warning #


