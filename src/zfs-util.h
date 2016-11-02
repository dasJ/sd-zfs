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
 * Mounts what at where with specified options.
 * Options may be NULL.
 * Returns the return code of the mount command.
 */
int zfs_mount(char *what, char *where, char *options);

#endif /* ZFS_UTIL_H */
