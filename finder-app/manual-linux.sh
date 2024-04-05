#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.1.10
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # TODO: Add your kernel build steps here
    echo "Building kernal..."
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper
    if [ $? != 0 ]; then echo "make mproper error" && exit 1; fi
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
    if [ $? != 0 ]; then echo "make defconfig error" && exit 1; fi
    make -j4 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all
    if [ $? != 0 ]; then echo "make all error" && exit 1; fi
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules
    if [ $? != 0 ]; then echo "make modules error" && exit 1; fi
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs
    if [ $? != 0 ]; then echo "make dtbs error" && exit 1; fi
    echo "Building kernal done."
    
fi

echo "Adding the Image in outdir"
if [ -e ${OUTDIR}/Image ]; then echo "Removing old Image file " && rm ${OUTDIR}/Image; fi
cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}/Image

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
mkdir -p ${OUTDIR}/rootfs
cd ${OUTDIR}/rootfs

mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr var
mkdir -p usr/bin usr/lib usr/sbin
mkdir -p var/log

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
else
    cd busybox
fi

# TODO: Make and install busybox
make distclean
if [ $? != 0 ]; then echo "make distclean error" && exit 1; fi
make defconfig
if [ $? != 0 ]; then echo "make defconfig error" && exit 1; fi
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
if [ $? != 0 ]; then echo "make cross-compile error" && exit 1; fi
make CONFIG_PREFIX=${OUTDIR}/rootfs ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install
if [ $? != 0 ]; then echo "make install error" && exit 1; fi

echo "Library dependencies"
${CROSS_COMPILE}readelf -a ${OUTDIR}/rootfs/bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a ${OUTDIR}/rootfs/bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
sysroot_dir=$(aarch64-none-linux-gnu-gcc -print-sysroot)
echo "sysroot_dir: ${sysroot_dir}"

cp ${sysroot_dir}/lib/ld-linux-aarch64.so.1 ${OUTDIR}/rootfs/lib/ld-linux-aarch64.so.1

cp ${sysroot_dir}/lib64/libm.so.6 ${OUTDIR}/rootfs/lib64/libm.so.6

cp ${sysroot_dir}/lib64/libresolv.so.2 ${OUTDIR}/rootfs/lib64/libresolv.so.2

cp ${sysroot_dir}/lib64/libc.so.6 ${OUTDIR}/rootfs/lib64/libc.so.6

# TODO: Make device nodes
sudo mknod -m 666 ${OUTDIR}/rootfs/dev/null c 1 3
sudo mknod -m 666 ${OUTDIR}/rootfs/dev/console c 5 1

# TODO: Clean and build the writer utility
cd ${FINDER_APP_DIR}
make clean
make CROSS_COMPILE=${CROSS_COMPILE}

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cd ${FINDER_APP_DIR}
cp writer ${OUTDIR}/rootfs/home/writer
cp finder-test.sh ${OUTDIR}/rootfs/home/finder-test.sh
cp finder.sh ${OUTDIR}/rootfs/home/finder.sh
mkdir -p ${OUTDIR}/rootfs/home/conf
cp conf/username.txt ${OUTDIR}/rootfs/home/conf/username.txt
cp conf/assignment.txt ${OUTDIR}/rootfs/home/conf/assignment.txt
cp autorun-qemu.sh ${OUTDIR}/rootfs/home/autorun-qemu.sh

# TODO: Chown the root directory
cd ${OUTDIR}/rootfs/
sudo chown -R root:root *

# TODO: Create initramfs.cpio.gz
cd ${OUTDIR}/rootfs
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio

cd ${OUTDIR}
gzip -f initramfs.cpio
if [ $? != 0 ]; then echo "gzip error" && exit 1; fi

