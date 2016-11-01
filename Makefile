ZFS_GENERATOR_OUT=initrd-zfs-generator
MOUNT_OUT=mount.initrd_zfs
LDFLAGS=-static
CCFLAGS=-Wall -ggdb

all: mount zfs-generator

mount: $(MOUNT_OUT)

zfs-generator: $(ZFS_GENERATOR_OUT)

$(MOUNT_OUT): src/mount.initrd_zfs.c src/cmdline.c src/cmdline.h
	$(CC) $(CCFLAGS) $(LDFLAGS) -o $@ $^

$(ZFS_GENERATOR_OUT): src/zfs-generator.c src/cmdline.c src/cmdline.h
	$(CC) $(CCFLAGS) $(LDFLAGS) -o $@ $^

clean:
	$(RM) $(MOUNT_OUT) $(ZFS_GENERATOR_OUT)

.PHONY: all mount zfs-generator clean
