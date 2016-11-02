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

#endif /* ZFS_UTIL_H */
