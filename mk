#!/bin/sh -x
#
# Description	: Android Build Script.
# Authors		: jianjun jiang - jerryjianjun@gmail.com
# Version		: 2.00
# Notes			: None
#

#
# JAVA_HOME
#
export ANDROID_JAVA_HOME=/usr/lib/jvm/java-6-sun/

#
# Some Directories
#
BS_DIR_TOP=$(cd `dirname $0` ; pwd)
BS_DIR_RELEASE=${BS_DIR_TOP}/out/release
BS_DIR_TOOLS=${BS_DIR_TOP}/tools
BS_DIR_TARGET=${BS_DIR_TOP}/out/target/product/smdk4x12/

#
# Cross Toolchain Path
#
BS_CROSS_TOOLCHAIN_BOOTLOADER=${BS_DIR_TOP}/prebuilt/linux-x86/toolchain/arm-2009q3/bin/arm-none-linux-gnueabi-
BS_CROSS_TOOLCHAIN_KERNEL=${BS_DIR_TOP}/prebuilt/linux-x86/toolchain/arm-eabi-4.4.3/bin/arm-eabi-

#
# Target Config
#
BS_CONFIG_BOOTLOADER_UBOOT=x4412_config
BS_CONFIG_KERNEL=x4412_android_defconfig
BS_CONFIG_FILESYSTEM=PRODUCT-full_smdk4x12-eng
BS_CONFIT_BUILDROOT=x4412_defconfig

setup_environment()
{
	LANG=C
	cd ${BS_DIR_TOP};
	mkdir -p ${BS_DIR_RELEASE} || return 1
}

build_uboot()
{	
	# Compiler uboot
	cd ${BS_DIR_TOP}/uboot || return 1
	make distclean || return 1
	make ${BS_CONFIG_BOOTLOADER_UBOOT} || return 1
	make -j${threads} || return 1

	# Copy bootloader to release directory
	cp -v ${BS_DIR_TOP}/uboot/ubootpak.bin ${BS_DIR_RELEASE}

	#rm ${BS_DIR_TOP}/uboot/bl2.bin
	#rm ${BS_DIR_TOP}/uboot/u-boot.bin

	# Copy some burnning script to release directory
	cp -v ${BS_DIR_TOP}/uboot/x4412-irom-sd.sh ${BS_DIR_RELEASE}

	echo "^_^ uboot path: ${BS_DIR_RELEASE}/ubootpak.bin"
	return 0
}

build_kernel()
{
	# Compiler kernel
	cd ${BS_DIR_TOP}/kernel || return 1
	make ${BS_CONFIG_KERNEL}  || return 1
	make -j${threads} || return 1

	# Copy zImage to release directory
	cp -v ${BS_DIR_TOP}/kernel/arch/arm/boot/zImage ${BS_DIR_RELEASE}

	echo "^_^ kernel path: ${BS_DIR_RELEASE}/zImage"
	return 0
}

build_bootimg()
{
	# generate boot.img
	cd ${BS_DIR_TOP} || return 1
	echo 'boot.img ->' ${BS_DIR_RELEASE}
	
	${BS_DIR_TOP}/out/host/linux-x86/bin/mkbootimg --kernel ${BS_DIR_RELEASE}/zImage --ramdisk ${BS_DIR_RELEASE}/ramdisk-uboot.img -o ${BS_DIR_RELEASE}/boot.img
}

build_system()
{
	if [ ! -f ${BS_DIR_RELEASE}/zImage ]; then
		echo "Not zImage is found at ${BS_DIR_RELEASE}, Please build kernel first"
		return 1;
	fi

	cd ${BS_DIR_TOP} || return 1
	source build/envsetup.sh || return 1
	make -j${threads} ${BS_CONFIG_FILESYSTEM} || return 1

	cp -av ${BS_DIR_TARGET}/system.img ${BS_DIR_RELEASE} || return 1;
	# Don't copy userdata.img
	# cp -av ${BS_DIR_TARGET}/userdata.img ${BS_DIR_RELEASE} || return 1;
	mkimage -A arm -O linux -T ramdisk -C none -a 0x41000000 -n "ramdisk" -d ${BS_DIR_TARGET}/ramdisk.img ${BS_DIR_RELEASE}/ramdisk-uboot.img || return 1;

	echo 'boot.img ->' ${BS_DIR_RELEASE}
	${BS_DIR_TOP}/out/host/linux-x86/bin/mkbootimg --kernel ${BS_DIR_RELEASE}/zImage --ramdisk ${BS_DIR_RELEASE}/ramdisk-uboot.img -o ${BS_DIR_RELEASE}/boot.img

	echo 'update.zip ->' ${BS_DIR_RELEASE}
	zip -j ${BS_DIR_RELEASE}/update.zip ${BS_DIR_TARGET}/android-info.txt ${BS_DIR_TARGET}/installed-files.txt ${BS_DIR_RELEASE}/boot.img ${BS_DIR_RELEASE}/system.img

	return 0
}

build_buildroot()
{
	# Compiler buildroot
	cd ${BS_DIR_TOP}/buildroot || return 1
	make ${BS_CONFIT_BUILDROOT} || return 1
	make || return 1

	# Copy image to release directory
	cp -v ${BS_DIR_TOP}/buildroot/output/images/rootfs.ext4 ${BS_DIR_RELEASE}/qt-rootfs.img
}

threads=$(grep processor /proc/cpuinfo | awk '{field=$NF};END{print field+1}')
uboot=no
kernel=no
system=no
buildroot=no

if [ -z $1 ]; then
	uboot=yes
	kernel=yes
	bootimg=no
	system=yes
	buildroot=yes
fi

while [ "$1" ]; do
    case "$1" in
	-j=*)
		x=$1
		threads=${x#-j=}
		;;
	-u|--uboot)
		uboot=yes
	    ;;
	-k|--kernel)
	    	kernel=yes
	    ;;
	-kr|--boot)
		bootimg=yes
	    ;;
	-s|--system)
		system=yes
	    ;;
	-b|--buildroot)
	    	buildroot=yes
	    ;;
	-a|--all)
		uboot=yes
		kernel=yes
		bootimg=no
		system=yes
		buildroot=yes
	    ;;
	-h|--help)
	    cat >&2 <<EOF
Usage: build.sh [OPTION]
Build script for compile the source of telechips project.

  -j=n                 using n threads when building source project (example: -j=16)
  -u, --uboot          build bootloader uboot from source
  -k, --kernel         build kernel from source
  -kr,--boot	       generate boot.img
  -s, --system         build android file system from source
  -b, --buildroot      build buildroot file system for QT platform
  -a, --all            build all, include anything
  -h, --help           display this help and exit
EOF
	    exit 0
	    ;;
	*)
	    echo "build.sh: Unrecognised option $1" >&2
	    exit 1
	    ;;
    esac
    shift
done

setup_environment || exit 1

if [ "${kernel}" = yes ]; then
	build_kernel || exit 1
fi

if [ "${uboot}" = yes ]; then
	build_uboot || exit 1
fi

if [ "${bootimg}" = yes ]; then
	build_bootimg || exit 1
fi

if [ "${system}" = yes ]; then
	build_system || exit 1
fi

if [ "${buildroot}" = yes ]; then
	build_buildroot || exit 1
fi

exit 0

