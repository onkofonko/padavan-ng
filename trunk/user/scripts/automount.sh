#!/bin/sh

func_load_module()
{
	module_name=$1
	(
		flock 500
		module_exist=`cat /proc/modules | grep $module_name`
		if [ -z "$module_exist" ]; then
			modprobe -q $module_name
			[ $? -eq 0 ] && usleep 300000
		fi
	) 500>/var/lock/fs_module.lock
}

existed=`cat /proc/partitions | grep $1 | wc -l`
[ $existed == "0" ] && exit 1

[ ! -x /sbin/blkid ] && exit 1

dev_full="/dev/$1"

# check filesystem type
ID_FS_TYPE=""
ID_FS_UUID=""
ID_FS_LABEL=""
eval `/sbin/blkid -s "TYPE" -s "UUID" -s "LABEL" -o udev $dev_full`

if [ "$ID_FS_TYPE" == "swap" ]; then
	[ ! -x /sbin/swapon ] && exit 1
	swap_used=`cat /proc/swaps | grep '^/dev/' | grep -v 'zram' | grep 'partition' 2>/dev/null`
	if [ -z "$swap_used" ]; then
		swapon $dev_full
		if [ $? -eq 0 ]; then
			logger -t "automount" "Activate swap partition $dev_full SUCCESS!"
		fi
	fi

	# always return !=0 for swap
	exit 1
fi

dev_label=`echo "$ID_FS_LABEL" | tr -d '\/:*?"<>|~$^' | sed 's/ /_/g'`
[ "$dev_label" == "NO_NAME" ] && dev_label=""

mnt_legacy="/media/$2"
if [ -z "$dev_label" ]; then
	dev_mount="$mnt_legacy"
else
	dev_mount="/media/$dev_label"
fi

# if mounted, try to umount
if mountpoint -q "$dev_mount" ; then
	if ! umount -f "$dev_mount" ; then
		dev_mount="$mnt_legacy"
		if mountpoint -q "$dev_mount" ; then
			if ! umount -f "$dev_mount" ; then
				logger -t "automount" "Unable to prepare mountpoint $dev_mount!"
				exit 1
			fi
		fi
	fi
fi

if ! mkdir -p "$dev_mount" ; then
	logger -t "automount" "Unable to create mountpoint $dev_mount!"
	exit 1
fi

achk_enable=`nvram get achk_enable`

if [ "$ID_FS_TYPE" == "msdos" -o "$ID_FS_TYPE" == "vfat" ]; then
	if [ "$achk_enable" != "0" ] && [ -x /sbin/dosfsck ]; then
		/sbin/dosfsck -a -v "$dev_full" > "/tmp/dosfsck_result_$1" 2>&1
	fi
	kernel_vfat=`modprobe -l | grep vfat`
	if [ -n "$kernel_vfat" ]; then
		func_load_module vfat
		mount -t vfat "$dev_full" "$dev_mount" -o noatime,umask=0,iocharset=utf8,codepage=866,shortname=winnt
	else
		func_load_module exfat
		mount -t exfat "$dev_full" "$dev_mount" -o noatime,umask=0,iocharset=utf8
	fi
elif [ "$ID_FS_TYPE" == "exfat" ]; then
	func_load_module exfat
	mount -t exfat "$dev_full" "$dev_mount" -o noatime,umask=0,iocharset=utf8
elif [ "$ID_FS_TYPE" == "ntfs" ]; then
	if [ "$achk_enable" != "0" ] && [ -x /sbin/chkntfs ]; then
		/sbin/chkntfs -a -f --verbose "$dev_full" > "/tmp/chkntfs_result_$1" 2>&1
	fi
	kernel_ufsd=`modprobe -l | grep ufsd`
	if [ -n "$kernel_ufsd" ]; then
		func_load_module ufsd
		mount -t ufsd "$dev_full" "$dev_mount" -o noatime,sparse,nls=utf8,force
	elif [ -x /sbin/ntfs-3g ]; then
		/sbin/ntfs-3g "$dev_full" "$dev_mount" -o noatime,umask=0,big_writes
		if [ $? -ne 0 ]; then
			/sbin/ntfs-3g "$dev_full" "$dev_mount" -o noatime,umask=0,ro
		fi
	fi
elif [ "$ID_FS_TYPE" == "hfsplus" -o "$ID_FS_TYPE" == "hfs" ]; then
	if [ "$achk_enable" != "0" ] && [ -x /sbin/fsck.hfsplus ]; then
		/sbin/fsck.hfsplus -d -p -y "$dev_full" > "/tmp/fsck_hfs_result_$1" 2>&1
	fi
	kernel_hfsplus=`modprobe -l | grep hfsplus`
	if [ -n "$kernel_hfsplus" ]; then
		func_load_module hfsplus
		mount -t $ID_FS_TYPE -o noatime,umask=0,nls=utf8,force "$dev_full" "$dev_mount"
	else
		func_load_module ufsd
		mount -t ufsd -o noatime,nls=utf8,force "$dev_full" "$dev_mount"
	fi
elif [ "$ID_FS_TYPE" == "ext4" -o "$ID_FS_TYPE" == "ext3" -o "$ID_FS_TYPE" == "ext2" ]; then
	if [ "$achk_enable" != "0" ] && [ -x /sbin/e2fsck ]; then
		/sbin/e2fsck -p -v "$dev_full" > "/tmp/e2fsck_result_$1" 2>&1
	fi
	if [ "$ID_FS_TYPE" == "ext4" ]; then
		func_load_module ext4
	elif [ "$ID_FS_TYPE" == "ext3" ]; then
		kernel_ext3=`modprobe -l | grep ext3`
		if [ -n "$kernel_ext3" ]; then
			func_load_module ext3
		else
			func_load_module ext4
		fi
	elif [ "$ID_FS_TYPE" == "ext2" ]; then
		kernel_ext2=`modprobe -l | grep ext2`
		if [ -n "$kernel_ext2" ]; then
			func_load_module ext2
		else
			func_load_module ext4
		fi
	fi
	mount -t $ID_FS_TYPE -o noatime "$dev_full" "$dev_mount"
elif [ "$ID_FS_TYPE" == "xfs" ]; then
	func_load_module xfs
	mount -t $ID_FS_TYPE -o noatime "$dev_full" "$dev_mount"
fi

# check failed to mount, clean up mountpoint
if ! mountpoint -q "$dev_mount" ; then
	if [ -n "$ID_FS_TYPE" ]; then
		logger -t "automount" "Unable to mount device $dev_full ($ID_FS_TYPE) to $dev_mount!"
	fi
	rmdir "$dev_mount"
	exit 1
fi

chmod 777 "$dev_mount"

# check ACL files
if [ -x /sbin/test_share ]; then
	/sbin/test_share "$dev_mount"
fi

# call optware script
/usr/bin/opt-mount.sh "$dev_full" "$dev_mount"

exit 0
