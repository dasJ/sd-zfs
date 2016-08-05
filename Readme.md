# sd-zfs

This project is an attempt to allow using systemd in the initrd and also having a ZFS dataset as root.
It is intended for [mkinitcpio](https://git.archlinux.org/mkinitcpio.git/) (used by [Arch Linux](https://www.archlinux.org/)) but should also work with other systems.
You are expected to already have a root filesystem on a ZFS dataset.

**Read and understand the Limitations before using this!**

## Installation
Get [mkinitcpio-sd-zfs](https://aur.archlinux.org/packages/mkinitcpio-sd-zfs/) from the [AUR](https://wiki.archlinux.org/index.php/Arch_User_Repository). Users without Arch should read the manual installation instructions at the bottom of this document. **sd-zfs is not ready for use yet. You need to configure it first.**

## Configuration

### Bootfs
sd-zfs uses the bootfs value of your zpools. This is an options which is intended to point to the root filesystem that is used for booting. You need to set it on any pool (the pool with the root fielsystem is recommended). If you set it to different values on multiple pools, one is picked at random.

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

This will make the system boot from the dataset "`root`" of the pool "`tank`". **The `mountpoint` value of the dataset needs to be `/` or `legacy`.** Note that legacy mountpoints are not really tested. But feel free to test and report any success/failures.

### root parameter
Your kernel command line usually contains an option called `root`. You can check this by reading `/proc/cmdline`. How this value is configured depends on your bootloader. You must set it to any partition containing any pool. This is necessary so mount(8) detects that it needs to boot a ZFS filesystem.

### mkinitcpio
Of course, this is only relevant for mkinitcpio, but users of other systems need to adopt these settings.

#### mkinitcpio.conf
Add `sd-zfs` to the `HOOK` array of `/etc/mkinitcpio.conf`. As it depends on the `systemd` hook, it needs to come after it.

#### Cache file
When booting the system, all devices are scanned to check for pools. Depending on the number of devices you have, it can be faster to cache the pools. This is accomplished by using the standard ZFS `cachefile`, which will be created at `/etc/zfs/zpool.cache`. If it exists during creation of the initrd, it will be included.

#### Custom module options
If you have any options for the ZFS module, you can add them to `/etc/modprobe.d/zfs.conf`. This file will be included into the initrd if it exists during initrd build.

#### Hostid
If `/etc/hostid` exists during build, it will be included in the initrd. It is highly recommended to use this file. More information is found in the [Arch wiki](https://wiki.archlinux.org/index.php/Installing_Arch_Linux_on_ZFS#After_the_first_boot).

#### Rebuilding initcpio
After changing any of these mkinitcpio related things, you need to rebuild your initrd. Assuming you have the default `linux` package, you can just run:
```
# mkinitcpio -p linux
```
If you use another kernel (like `linux-lts`), you need to adapt the command.

## Forcing imports
When the pool was not properly exported by a system with another hostid, the pool can not be imported during mount. Add `zfs_force=1` to your kernel command line to force importing the pool.

## How it works

### Importing
When the initrd is running, the pools are imported (without actually mounting them).
When importing, the `altroot` property of the pool is set (it's needed later, but also causes some trouble, see the limitations). It's ensured that all `cryptsetup` devices are unlocked before the pools are imported.

There are two ways to import the pools:

#### By scan
If no cachefile exists, all devices in `/etc/disk/by-id` are scanned and all pools that imported without actually mount them.

#### By cachefile
If the cachefile exists, all devices from the cachefile are imported without actually mounting them.

### Mounting
The systemd mount-unit is overriden so it will only run after the pools are imported. systemd will invoke mount(8) with your `root` kernel parameter like this: `mount /dev/sda1 /sysroot` (assuming your root is `/dev/sda1`). This triggers `mount.zfs_member`, which is part of this repo. The disk parameter is irgnored by `mount.zfs`, instead, the `bootfs` value is searched in the pools. When a bootfs is found, it gets mounted. The exact process depends depends on the `mountpoint` value.

#### legacy mounting
If the `mountpoint` value of the dataset is `legacy`, it gets mounted to `/sysroot` by executing `mount.zfs tank/root /sysroot`. Subdatasets are not handled in any way.

#### Non-legacy mounting
If the `mountpoint` value of the dataset is not `legacy`, it gets mounted by `zfs mount tank/root`. As the pool was imported with `altroot` set to `/legacy`, this mount will be relative to `/sysroot`. After this, all non-legacy datasets are searched and mounted as well.

### Switching root
systemd will now take care of killing all processes and switching root to `/sysroot`.

## Limitations
When using non-legacy mounts, the mounted datasets will be relative to `/sysroot`. Even if the files are found in `/`, they will show up in `/sysroot`:
```
# zfs list
NAME         USED  AVAIL  REFER  MOUNTPOINT
tank        1001M  8.65G    37K  /sysroot/zroot
tank/root    999M  8.65G   999M  /sysroot
```
This is not just a cosmeting issue, it also affects the behaviour when using `zfs create`:
```
# zfs create tank/test
# ls -la /zroot/test
ls: cannot access '/zroot/test': No such file or directory
# ls -la /sysroot/zroot/test
total 1
drwxr-xr-x 2 root root 2 Aug  4 19:10 .
drwxr-xr-x 4 root root 4 Aug  4 19:10 ..
```

## Manual installation
See `mkinitcpio-sd-zfs.install`, which instructs `mkinitcpio` what to do.
`$BUILDROOT` is the root of the initrd that is currently being built.
You need to `make all` and put the resulting files to the locations mentioned in the install-file.

## Warranty
Nope.

## TODO
- Test legacy mounts
- Fix the limitations
- Test forcing
