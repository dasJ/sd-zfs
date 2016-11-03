#ifndef ZFS_UTIL_H
#define ZFS_UTIL_H

/*
 * Removes a dataset from a pool including
 * all children datasets.
 * Expects the dataset to be imported.
 */
int zfs_destroy_recursively(char *dataset);

/*
 * Destroys a dataset, not including children datasets.
 * Expects the dataset to be imported.
 */
int zfs_destroy(char *datset);

/*
 * Check if a snapshot exists in a dataset.
 * Returns 0 if it exists, 1 if it doesn't exist
 * and < 0 if there was an error.
 * Expects the dataset to be imported.
 */
int zfs_snapshot_exists(char *dataset, char *snapshot);

/*
 * Checks if a dataset exists.
 * Returns 0 if it exists, 1 if it doesn't exist
 * and < 0 if there was an error.
 * Expects the dataset to be imported.
 */
int zfs_ds_exists(char *dataset);

/**
 * Looks for a bootfs value in all imported pools (if rpool is NULL).
 * If rpool is not NULL, only this pool will be searched.
 * Returns 0 if everything was okay.
 * bootfs will be an allocated string containing all bootfs values
 * (including '-'), separated by newlines
 */
int zfs_get_bootfs(char *rpool, char **bootfs);

/*
 * Lists all datasets which are child of the specified dataset (including
 * the dataset itself). The output, which is \n-spearated, is written to dataset.
 * Each line contains the name of the dataset, a tab and the mountpoint of the dataset.
 * Returns 0 if everything was okay.
 */
int zfs_list_datasets_with_mp(char *dataset, char **datasets);

/*
 * Lists all snapshots of a dataset with a given name.
 * Returns 0 if everything went okay.
 * The snapshot names are newline-separated.
 *
 * Ex. with tank/root and snap1
 * tank/root@snap1
 * tank/root/home@snap1
 * tank/root/var@snap1
 */
int zfs_list_snapshots(char *dataset, char *snapshot, char **output);

/*
 * Mounts what at where with specified options.
 * Options may be NULL.
 * Returns the return code of the mount command.
 */
int zfs_mount(char *what, char *where, char *options);

/*
 * Returns the mountpoint of any dataset.
 * Dataset must be imported.
 * Returns the return code of the ZFS command (0 = okay).
 * mountpoint must be NULL
 */
int zfs_get_mountpoint(char *dataset, char **mountpoint);

/*
 * Clones a snapshot to a dataset and sets org.zfs:mountpoint to the mountpoint.
 * Returns 0 = okay
 */
int zfs_clone_snap(char *snapshot, char *datasetTarget, char *mountpoint);

/*
 * Returns the org.zol:mountpoint of any dataset.
 * Dataset must be imported.
 * Returns the return code of the ZFS command (0 = okay).
 * mountpoint must be NULL
 */
int zfs_get_alt_mp(char *dataset, char **mountpoint);

#endif /* ZFS_UTIL_H */
