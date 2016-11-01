ZFS_GENERATOR_OUT=initrd-zfs-generator
MOUNT_OUT=mount.zfs_member
LDFLAGS=-static
CCFLAGS=

all: mount zfs-generator

mount: $(MOUNT_OUT)

force: $(FORCE_OUT)

zfs-generator: $(GENERATOR_OUT)

$(MOUNT_OUT): src/mount.zfs_member.c
	$(CC) $(CCFLAGS) $(LDFLAGS) -o $@ $?

$(ZFS_GENERATOR_OUT): src/zfs-generator.c src/cmdline.c src/cmdline.h
	$(CC) $(CCFLAGS) $(LDFLAGS) -o $@ $^

clean:
	$(RM) $(MOUNT_OUT) $(GENERATOR_OUT)

.PHONY: all mount generator clean
