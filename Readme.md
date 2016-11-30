# sd-zfs

This project is an attempt to allow using systemd in the initrd and also having a ZFS dataset as root.
It is intended for [mkinitcpio](https://git.archlinux.org/mkinitcpio.git/) (used by [Arch Linux](https://www.archlinux.org/)) but should also work with other systems.
You are expected to already have a root filesystem on a ZFS dataset.

Please note that legacy root mounts are not supported because they are legacy technology (as the name implies).

## Functionality
- Boot from any ZFS dataset. All subdatasets are mounted as well
- Use `bootfs` to decide what dataset to use
- Use zpool.cache for pool caching (can be overridden)
- Included udev rules for importing by vdev
- All pools are exported on shutdown
- root is mounted as read-only when `rw` is not set on command line
- Actual hostid value is used when it's not found in `/etc/hostid`
- `/etc/modprobe.d/{spl,zfs}.conf` is included in initrd

## Snapshot booting
`sd-zfs` supports booting from ZFSsnapshots.
As snapshots are read-only and not bootable, they are automatically cloned to new datasets which are booted.
All subdatasets wil be checked for the same snapshot.

Example:
- Boot with root=tank/root@snap
- The following datasets with the following snapshots exist:
```
tank/root
tank/root@snap
tank/root/etc
tank/root/etc@snap
```
- When booting, the following datasets are created (**they get deleted before creating if they exist**):
```
tank/root_initrd_snap
tank/root_initrd_snap/etc
```
- `sd-zfs` will boot from `tank/root_initrd_snap`

## Installation
Get [mkinitcpio-sd-zfs](https://aur.archlinux.org/packages/mkinitcpio-sd-zfs/) from the [AUR](https://wiki.archlinux.org/index.php/Arch_User_Repository).
Users without Arch should read the manual installation instructions at the bottom of this document.
**sd-zfs is not ready for use yet. You need to configure it first.**

## Configuration

### Kernel parameters
sd-zfs supports multiple kernel parameters to select what dataset to boot from and to tune the booting process.

#### Which dataset to boot from
- `root=zfs:somepool/somedataset` - Use this dataset to boot from
- `root=zfs:AUTO` - Check all pools for the bootfs value. See rpool to narrow the search
- `rpool=somepool` - Check only this pool for the bootfs value. This may not contain slashes

The root option can be suffixed with `@snap` to boot from a snapshot named `snap`.
See "Snapshot booting" for more information.

#### Other options
- `rootflags=flags` - Use these flags when mounting the dataset
- `zfs_force=1` - Force import of the pools
- `zfs_ignorecache=1` - Ignore the pool cache file while booting

### Bootfs
sd-zfs can use the bootfs value of your zpools.
This is an property of ZFS pools which is intended to point to the root filesystem that is used for booting.
You need to set it on any pool (the pool with the root fielsystem is recommended).
If you set it to different values on multiple pools, the first one that is found will be used.

Check the bootfs value of all pools:
```
# zpool get -p bootfs
NAME   PROPERTY  VALUE   SOURCE
tank   bootfs    -       default
```

Set the bootfs value:
```
# zpool set bootfs=tank/root tank
```

This will make the system boot from the dataset "`root`" of the pool "`tank`".

**The `mountpoint` value of the dataset needs to be `/`.**

### Custom module options
If you have any options for the SPL and ZFS modules, you can add them to `/etc/modprobe.d/zfs.conf` and `/etc/modprobe.d/spl.conf`.
These files will be included into the initrd if they exist during initrd build.

### mkinitcpio.conf
Add `sd-zfs` to the `HOOK` array of `/etc/mkinitcpio.conf`.
As it depends on the `systemd` hook, it needs to come after it.
There are no more dependencies than `systemd`.

### Cache file
When booting the system, all devices are scanned to check for pools.
Depending on the number of devices you have, it can be faster to cache the pools.
This is accomplished by using the standard ZFS `cachefile`, which will be created at `/etc/zfs/zpool.cache`.
If it exists during creation of the initrd, it will be included.

### Hostid
If `/etc/hostid` exists during build, it will be included in the initrd.
It is highly recommended to use this file.
More information is found in the [Arch wiki](https://wiki.archlinux.org/index.php/Installing_Arch_Linux_on_ZFS#After_the_first_boot).
If the file is not found, the actual value of the `hostid` command will be written to the initrd.

### Rebuilding initrd
After changing any of these mkinitcpio related things (apart from the kernel command line and the bootfs value), you need to rebuild your initrd.
Assuming you have the default `linux` package, you can just run:
```
# mkinitcpio -p linux
```
If you use another kernel (like `linux-lts`), you need to adapt the command.

## How it works

### Generating
When systemd is starting, all generators are run, this includes a generator for ZFS units.
This generator parses the kernel parameters and creates systemd services for importing the pools as well as overriding `sysroot.mount` which is responsible for mounting the root filesystem.

### Importing
When systemd is running, the pools are imported (without actually mounting them).

There are two ways to import the pools:

#### By scan
If no cachefile exists, all devices in `/etc/disk/by-id` are scanned and all pools that imported without actually mounting them.
This can be forced via kernel parameter.

#### By cachefile
If the cachefile exists, all devices from the cachefile are imported without actually mounting them.
This can be prevented via kernel parameter.

### Mounting
The systemd unit `sysroot.mount` is overriden so it will run a custom command.
This command figures out the bootfs (if `zfs:AUTO` is used) and handles snapshot creation.
It proceeds to mount the correct dataset, including all subdatasets.

### Switching root
systemd will now take care of killing all processes and switching root to `/sysroot`.

### Shutdown
When shutting down, systemd pivots back to another ramdisk.
All executables from `/usr/lib/systemd/system-shutdown` are run.
One of them is provided by this package and is responsible for forcefully exporting all pools.
To accomplish that, `zpool` is added to the ramdisk via a systemd unit.

## Manual installation
See `mkinitcpio-install/sd-zfs`, which instructs `mkinitcpio` what to do.
`$BUILDROOT` is the root of the initrd that is currently being built.
You need to `make all` and put the resulting files to the locations mentioned in the install file.

`mkinitcpio-install/sd-zfs-shutdown` is responsible for copying `zpool` to the ramdisk on shutdown.
This is required for `zfs-shutdown`.

## Warranty
Nope.
