#!/bin/bash
# Usage: ./build_mainline_uboot.sh [2 | pc | plus | lite | pc-plus | one | plus2e]

#==========================================================
basepath=$(cd `dirname $0`; pwd)

if [ "${1}" = "" ]; then
	echo "USAGE: sudo ./build_mainline_uboot.sh [2|pc|plus|lite|pc-plus|one|plus2e]"
	exit -1
fi

echo "check uboot source ..."
echo " "
cd $basepath/u-boot-2016.7/configs
 
config="orangepi_${1}_defconfig"
dts="sun8i-h3-orangepi-${1}.dtb"
echo $config

if [ ! -f $config ]; then
	echo "source not found !"
	exit 0
fi

echo "enter u-boot source director" && cd ../
	echo "clear u-boot" && make distclean
	echo "compile u-boot" && make $config

date
echo "build u-boot,please wait ..." && make -j4 ARCH=arm CROSS_COMPILE=arm-linux-gnueabi-
date

cd ../build/uboot
if [ -f u-boot-sunxi-with-spl.bin ]; then
	rm -rf u-boot-sunxi-with-spl.bin
fi

	echo " "
	echo $PWD
	echo " "
	cp ../../u-boot-2016.7/u-boot-sunxi-with-spl.bin ./ -rf  
	echo "****************************************Compile uboot OK***********************************"


sed -i '/sun8i-h3/d' orangepi.cmd
linenum=`grep -n "uImage" orangepi.cmd | awk '{print $1}' | awk -F: '{print $1}'`
sed -i "${linenum}i fatload mmc 0 0x46000000 ${dts}" orangepi.cmd
sudo chmod +x orangepi.cmd u-boot-sunxi-with-spl.bin
mkimage -C none -A arm -T script -d orangepi.cmd boot.scr

cp -rf u-boot-sunxi-with-spl.bin ../../../OrangePi-BuildLinux/orange

cp -rf boot.scr orangepi.cmd ../../../
cd ../../../
mv  boot.scr orangepi.cmd ./OrangePi-BuildLinux/orange
echo $PWD




