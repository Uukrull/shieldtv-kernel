#!/bin/sh
MEDIA_OUT=$ANDROID_PRODUCT_OUT/media_build_root
[ -e "$ANDROID_PRODUCT_OUT/external-modules-media_build.tar.gz" ] && rm -f $ANDROID_PRODUCT_OUT/external-modules-media_build.tar.gz
[ -d "$MEDIA_OUT" ] && rm -Rf $MEDIA_OUT
mkdir -p $MEDIA_OUT

cd $TOP/media_build

make \
 ARCH=arm64 \
 CROSS_COMPILE=$ANDROID_TOOLCHAIN/aarch64-linux-android- \
 CFLAGS_MODULE=-fno-pic \
 CFLAGS=-fno-pic \
 CROSS32CC=$ARM_EABI_TOOLCHAIN/arm-eabi-gcc \
 DIR=$ANDROID_PRODUCT_OUT/obj/KERNEL untar

cp media_build.config v4l/.config

make \
 ARCH=arm64 \
 CROSS_COMPILE=$ANDROID_TOOLCHAIN/aarch64-linux-android- \
 CFLAGS_MODULE=-fno-pic \
 CFLAGS=-fno-pic \
 CROSS32CC=$ARM_EABI_TOOLCHAIN/arm-eabi-gcc \
 O=$ANDROID_PRODUCT_OUT/obj/media_build \
 DIR=$ANDROID_PRODUCT_OUT/obj/KERNEL && \
 find $TOP/media_build/v4l -type f -name \*.ko -exec cp {} $MEDIA_OUT \; &&
 ( cd $MEDIA_OUT; tar -czf $ANDROID_PRODUCT_OUT/external-modules-media_build.tar.gz ./ ) &&
 ( cd $TOP/media_build; make distclean )
          
            
            