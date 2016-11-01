GENERATOR_OUT=initrd-zfs-generator
MOUNT_OUT=mount.initrd_zfs
SHUTDOWN_OUT=zfs-shutdown
LDFLAGS=-static
CCFLAGS=-Wall -std=gnu11 -pedantic

all: mount generator shutdown

mount: $(MOUNT_OUT)

generator: $(GENERATOR_OUT)

shutdown: $(SHUTDOWN_OUT)

$(MOUNT_OUT): src/mount.initrd_zfs.c
	$(CC) $(CCFLAGS) $(LDFLAGS) -o $@ $^

$(GENERATOR_OUT): src/zfs-generator.c src/cmdline.c src/cmdline.h
	$(CC) $(CCFLAGS) $(LDFLAGS) -o $@ $^

$(SHUTDOWN_OUT): src/zfs-shutdown.c
	$(CC) $(CCFLAGS) $(LDFLAGS) -o $@ $^

clean:
	$(RM) $(MOUNT_OUT) $(ZFS_GENERATOR_OUT) $(ZFS_SHUTDOWN_OUT)

.PHONY: all mount generator shutdown clean
