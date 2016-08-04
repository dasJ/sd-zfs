MOUNT_OUT=mount.zfs_member
FORCE_OUT=zfs-getforce
LDFLAGS=-static
CCFLAGS=

all: mount force

mount: $(MOUNT_OUT)

force: $(FORCE_OUT)

$(MOUNT_OUT): src/mount.zfs_member.c
	$(CC) $(CCFLAGS) $(LDFLAGS) -o $@ $?

$(FORCE_OUT): src/zfs-getforce.c
	$(CC) $(CCFLAGS) $(LDFLAGS) -o $@ $?

clean:
	$(RM) $(MOUNT_OUT) $(FORCE_OUT)

.PHONY: all mount force clean
