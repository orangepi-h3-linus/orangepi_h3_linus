#!/bin/bash

# === USAGE ===========================================
# build_linux_kernel <board> [clean]
# <board> = 2     ->  build for OPI-2/OPI-PC
# <board> = plus  ->  build for OPI-PLUS
# <board> = all   ->  build for OPI-2/OPI-PC & OPI-PLUS
# <board> = clean ->  clean all
# if 2nd parameter is clean, cleans all before build
# =====================================================
# After build uImage and lib are in build directory
# =====================================================


export PATH="$PWD/brandy/gcc-linaro/bin":"$PATH"
cross_comp="arm-linux-gnueabi"

# ##############
# Prepare rootfs
# ##############

cd build

rm rootfs-lobo.img.gz > /dev/null 2>&1

# create new rootfs cpio
cd rootfs-test1
mkdir run > /dev/null 2>&1
mkdir -p conf/conf.d > /dev/null 2>&1

find . | cpio --quiet -o -H newc > ../rootfs-lobo.img
cd ..
gzip rootfs-lobo.img
cd ..
#=========================================================


cd linux-4.7.6
LINKERNEL_DIR=`pwd`

# build rootfs
rm -rf output/* > /dev/null 2>&1
mkdir -p output/lib > /dev/null 2>&1
cp ../build/rootfs-lobo.img.gz output/rootfs.cpio.gz

#==================================================================================

    # ############
    # Build kernel
    # ############

    # ###########################
    if [ "${1}" = "clean" ]; then
		echo "cleaning..."
        make ARCH=arm CROSS_COMPILE=${cross_comp}- mrproper > /dev/null 2>&1
		if [ $? -ne 0 ]; then
			echo "  ERROR."
		fi
		rm -rf ../*.log
		rm -rf ../build/lib/* > /dev/null 2>&1
		rm -rf ../build/uImage* > /dev/null 2>&1
		rm -rf ../build/rootfs-lobo.img.gz > /dev/null 2>&1
		rm -rf output/* > /dev/null 2>&1
		rmdir ../build/lib > /dev/null 2>&1
		echo "******ok******"
		exit 1
    fi
    sleep 1
    if [ "${1}" = "opi" ]; then
    	echo "Building kernel for OPI ..."
    	echo "  Configuring ..."
    	make -j6 ARCH=arm CROSS_COMPILE=${cross_comp}- sun8iw7p1_mainline_defconfig zImage modules > ../kbuild_OPI.log 2>&1
    	if [ $? -ne 0 ]; then
        	echo "  Error: KERNEL NOT BUILT."
        	exit 1
    	fi
    	sleep 1

    # #############################################################################
    # build kernel (use -jN, where N is number of cores you can spare for building)
    	echo "  Building kernel & modules ..."
		cd arch/arm/boot/
		mkimage -A arm -O linux -T kernel -C none -a 0x48000000 -e 0x48000000 -n linux-4.7.6 -d zImage uImage
		cd ../../../
    	if [ $? -ne 0 ] || [ ! -f arch/arm/boot/uImage ]; then
        	echo "  Error: KERNEL NOT BUILT."
        	exit 1
    	fi
    	sleep 1

    # ########################
    # export modules to output
    	echo "  Exporting modules ..."
    	rm -rf output/lib/*
    	make ARCH=arm CROSS_COMPILE=${cross_comp}- INSTALL_MOD_PATH=output modules_install >> ../kbuild_OPI.log 2>&1
    	if [ $? -ne 0 ] || [ ! -f arch/arm/boot/uImage ]; then
        	echo "  Error."
    	fi
    	echo "  Exporting firmware ..."
    	make ARCH=arm CROSS_COMPILE=${cross_comp}- INSTALL_MOD_PATH=output firmware_install >> ../kbuild_OPI.log 2>&1
   		if [ $? -ne 0 ] || [ ! -f arch/arm/boot/uImage ]; then
        	echo "  Error."
			exit 1
    	fi
    	sleep 1

    # #####################
    # Copy uImage to output
    	cp arch/arm/boot/uImage output/uImage
		cd ..
    	cd $LINKERNEL_DIR
    	cp arch/arm/boot/uImage ../build/uImage
    	[ ! -d ../build/lib ] && mkdir ../build/lib
    	rm -rf ../build/lib/*
    	cp -R output/lib/* ../build/lib
        

		rm -rf ../../OrangePi-BuildLinux/orange/lib/* 
		cp -rf ../build/lib/* ../../OrangePi-BuildLinux/orange/lib/
	fi
#==================================================================================

echo "***OK***"

# cp uImage  sun8i-h3-orangepi-${1}.dtb -> boot分区
