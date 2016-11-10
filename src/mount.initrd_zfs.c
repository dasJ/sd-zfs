#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include "zfs-util.h"

/*
 * Tries to get the pool to mount from the bootfs value
 */
int handleBootfs(char **pool) {
	char *rpool = NULL;
	int ret = 1;

	// Handle rpool
	if (strncmp(*pool, "zfs:AUTO:", strlen("zfs:AUTO:")) == 0) {
		rpool = malloc((strlen(*pool) - strlen("zfs:AUTO:") + 1) * sizeof(char));
		strcpy(rpool, &((*pool)[strlen("zfs:AUTO:")]));
	}

	ret = zfs_get_bootfs(rpool, pool);
	if (rpool != NULL) {
		free(rpool);
	}
	return ret;
}

int main(int argc, char *argv[]) {
	char *dataset = NULL;
	char *mountpoint = NULL;
	char *options = NULL;
	// For parameter parsing
	char isOption = 0;
	char *newoptions;
	size_t size;
	// For snapshotting
	int ret;
	char *snapDS = NULL;
	char *snapMP = NULL;
	char *snapBase;
	char *snapPath;
	char *snaps;
	char *snapshot;
	char *at;
	// For mounting
	char *snap;
	char *lines = NULL;
	char *endLine;
	char *lineToken;
	char *mountpointToken;
	char *what;
	char *where;
	char *where_tmp = NULL;

	// Parse parameters
	if (argc < 3) {
		fprintf(stderr, "Call like: %s <dataset> <mount_point> [-o option[,...]]\n", argv[0]);
		exit(1);
	}
	for (int i = 1; i < argc; i++) {
		// This is an option
		if (isOption == 1) {
			size = strlen(argv[i]) + 2;
			if (options != NULL) {
				size += strlen(options);
			}
			newoptions = malloc(size * sizeof(char));
			if (options != NULL) {
				strcpy(newoptions, options);
			}
			strcat(newoptions, argv[i]);
			strcat(newoptions, ",");
			if (options != NULL) {
				free(options);
			}
			options = newoptions;
			isOption = 0;
			continue;
		}
		// Next will be an option
		if (strcmp(argv[i], "-o") == 0) {
			isOption = 1;
			continue;
		}
		// Next is the dataset
		if (dataset == NULL) {
			dataset = malloc((strlen(argv[i]) + 1) * sizeof(char));
			strcpy(dataset, argv[i]);
			continue;
		}
		// Next is the mountpoint
		if (mountpoint == NULL) {
			mountpoint = malloc((strlen(argv[i]) + 1) * sizeof(char));
			strcpy(mountpoint, argv[i]);
			continue;
		}
		// Call is broken
		free(dataset);
		free(mountpoint);
		fprintf(stderr, "Call like: %s <dataset> <mount_point> [-o option[,...]]\n", argv[0]);
		exit(1);
	}
	// Get rid of the trailing comma
	if (options != NULL) {
		newoptions = malloc(sizeof(options) - sizeof(char));
		strncpy(newoptions, options, strlen(options) - 1);
		free(options);
		options = newoptions;
	}

	// Split away snapshot name
	strtok(dataset, "@");
	snap = strtok(NULL, "@");

	// Check if we need to handle bootfs
	if (strncmp(dataset, "zfs:AUTO", strlen("zfs:AUTO")) == 0) {
		if (handleBootfs(&dataset) != 0) {
			fprintf(stderr, "Can not get bootfs value\n");
			free(dataset);
			free(mountpoint);
			if (options != NULL) {
				free(options);
			}
		}
	}

	// Handle snapshot
	if (snap != NULL) {
		ret = zfs_snapshot_exists(dataset, snap);
		if (ret < 0) {
			fprintf(stderr, "Error while trying to check if snapshot exists\n");
			goto aftersnap;
		} else if (ret == 1) {
			goto aftersnap;
		}
		// Build name of temporary dataset
		snapDS = malloc((strlen(dataset) + strlen(snap) + strlen("_initrd_") + 1) * sizeof(char));
		strcpy(snapDS, dataset);
		strcat(snapDS, "_initrd_");
		strcat(snapDS, snap);
		// Remove temporary datasets if they exist
		ret = zfs_ds_exists(snapDS);
		if (ret < 0) {
			fprintf(stderr, "Error while trying to check if temporary dataset exists\n");
			goto aftersnap;
		} else if (ret == 0) {
			if (zfs_destroy_recursively(snapDS) != 0) {
				fprintf(stderr, "Failed to remove existing datasets for snapshotting\n");
				goto aftersnap;
			}
		}
		// Create new datasets
		ret = zfs_list_snapshots(dataset, snap, &snaps);
		if (ret != 0) {
			fprintf(stderr, "Error while trying to list snapshots\n");
			goto aftersnap;
		}
		snapshot = strtok(snaps, "\n");
		while (snapshot != NULL) {
			snapPath = malloc((strlen(snapDS) + strlen(&snapshot[strlen(dataset)]) + 1) * sizeof(char));
			strcpy(snapPath, snapDS);
			strcat(snapPath, &(snapshot[strlen(dataset)]));
			at = strrchr(snapPath, '@');
			if (at != 0) {
				*at = '\0';
			}
			snapBase = malloc((strlen(snapshot) + 1) * sizeof(char));
			strcpy(snapBase, snapshot);
			at = strrchr(snapBase, '@');
			if (at != 0) {
				*at = '\0';
			}
			snapMP = NULL;
			if (zfs_get_mountpoint(snapBase, &snapMP) != 0) {
				fprintf(stderr, "Can not get mountpoint of snapshot\n");
				goto aftersnap;
			}

			if (strcmp(snapMP, "/") == 0) {
				free(dataset);
				dataset = malloc((strlen(snapPath) + 1) * sizeof(char));
				strcpy(dataset, snapPath);
			}

			if (zfs_clone_snap(snapshot, snapPath, snapMP) != 0) {
				fprintf(stderr, "Can not clone snapshot to dataset\n");
				free(snapPath);
				goto aftersnap;
			}
			free(snapPath);

			snapshot = strtok(NULL, "\n");
		}
	}
aftersnap:
	if (snapDS != NULL) {
		free(snapDS);
	}

	// Mount the dataset(s)
	ret = zfs_list_datasets_with_mp(dataset, &lines);
	if (ret != 0) {
		free(dataset);
		free(mountpoint);
		if (options != NULL) {
			free(options);
		}
	}

	lineToken = strtok_r(lines, "\n", &endLine);
	while (lineToken != NULL) {
		what = strtok_r(lineToken, "\t", &mountpointToken);
		// Do not mount what we don't need to mount
		if (strcmp(mountpointToken, "-") == 0 || strcmp(mountpointToken, "legacy") == 0 || strcmp(mountpointToken, "none") == 0) {
			if (zfs_get_alt_mp(what, &where_tmp) != 0) {
				goto loopend;
			}
			if (strcmp(where_tmp, "-") == 0 || strcmp(where_tmp, "legacy") == 0 || strcmp(where_tmp, "none") == 0) {
				goto loopend;
			}
		}
		// Build where to mount
		where = malloc((strlen(mountpoint) + strlen((where_tmp == NULL) ? mountpointToken : where_tmp) + 1) * sizeof(char));
		strcpy(where, mountpoint);
		strcat(where, (where_tmp == NULL) ? mountpointToken : where_tmp);
		// Mount
		ret = zfs_mount(what, where, options);
		if (where_tmp != NULL) {
			free(where_tmp);
			where_tmp = NULL;
		}
		if (ret != 0) {
			fprintf(stderr, "ZFS command failed with exit code %d, bailing out\n", ret);
			free(where);
			break;
		}
		free(where);

loopend:
		lineToken = strtok_r(NULL, "\n", &endLine);
	}

	free(lines);
	
	free(dataset);
	free(mountpoint);
	if (options != NULL) {
		free(options);
	}
}
