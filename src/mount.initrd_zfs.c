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
	char *line = NULL;
	char *tok;
	int ret = 1;

	// Handle rpool
	if (strncmp(*pool, "zfs:AUTO:", strlen("zfs:AUTO:")) == 0) {
		rpool = malloc((strlen(*pool) - strlen("zfs:AUTO:") + 1) * sizeof(char));
		strcpy(rpool, &((*pool)[strlen("zfs:AUTO:")]));
	}

	ret = zfs_get_bootfs(rpool, &line);
	if (ret != 0) {
		if (rpool != NULL) {
			free(rpool);
		}
		return ret;
	}

	tok = strtok(line, "\n");
	while (tok != NULL) {
		if (strcmp(tok, "-") != 0) {
			*pool = malloc((strlen(tok) + 1) * sizeof(char));
			strcpy(*pool, tok);
			ret = 0;
			break;
		}
		tok = strtok(NULL, "\n");
	}
	free(line);

	if (rpool != NULL) {
		free(rpool);
	}
	return ret;
}

int main(int argc, char *argv[]) {
	char *pool = NULL;
	char *mountpoint = NULL;
	char *options = NULL;
	// For parameter parsing
	char isOption = 0;
	char *newoptions;
	size_t size;
	// For snapshotting
	int ret;
	char *snapDS = NULL;
	// For mounting
	char *snap;
	char *lines = NULL;
	pid_t pid;
	char *endLine;
	char *lineToken;
	char *mountpointToken;
	char *dataset;
	char *where;
	int status;

	// Parse parameters
	if (argc < 3) {
		fprintf(stderr, "Call like: %s <pool> <mount_point> [-o option[,...]]\n", argv[0]);
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
		// Next is the pool
		if (pool == NULL) {
			pool = malloc((strlen(argv[i]) + 1) * sizeof(char));
			strcpy(pool, argv[i]);
			continue;
		}
		// Next is the mountpoint
		if (mountpoint == NULL) {
			mountpoint = malloc((strlen(argv[i]) + 1) * sizeof(char));
			strcpy(mountpoint, argv[i]);
			continue;
		}
		// Call is broken
		free(pool);
		free(mountpoint);
		fprintf(stderr, "Call like: %s <pool> <mount_point> [-o option[,...]]\n", argv[0]);
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
	strtok(pool, "@");
	snap = strtok(NULL, "@");

	// Check if we need to handle bootfs
	if (strncmp(pool, "zfs:AUTO", strlen("zfs:AUTO")) == 0) {
		if (handleBootfs(&pool) != 0) {
			fprintf(stderr, "Can not get bootfs value\n");
			free(pool);
			free(mountpoint);
			if (options != NULL) {
				free(options);
			}
		}
	}

	// Handle snapshot
	if (snap != NULL) {
		ret = zfs_snapshot_exists(pool, snap);
		if (ret < 0) {
			fprintf(stderr, "Error while trying to check if snapshot exists\n");
			goto aftersnap;
		} else if (ret == 1) {
			goto aftersnap;
		}
		// Build name of temporary dataset
		snapDS = malloc((strlen(pool) + strlen(snap) + strlen("_initrd_") + 1) * sizeof(char));
		strcpy(snapDS, pool);
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
		// TODO Create new datasets from snapshots
		// TODO Boot to new dataset
	}
aftersnap:
	if (snapDS != NULL) {
		free(snapDS);
	}

	// Mount the dataset(s)
	ret = zfs_list_datasets_with_mp(pool, &lines);
	if (ret != 0) {
		free(pool);
		free(mountpoint);
		if (options != NULL) {
			free(options);
		}
	}

	lineToken = strtok_r(lines, "\n", &endLine);
	while (lineToken != NULL) {
		dataset = strtok_r(lineToken, "\t", &mountpointToken);
		// Do not mount what we don't need to mount
		if (strcmp(mountpointToken, "-") == 0 || strcmp(mountpointToken, "legacy") == 0) {
			goto loopend;
		}
		// Build where to mount
		where = malloc((strlen(mountpoint) + strlen(mountpointToken) + 1) * sizeof(char));
		strcpy(where, mountpoint);
		strcat(where, mountpointToken);
		// Mount
		pid = fork();
		if (pid == 0) {
			if (options == NULL) {
				execl("/usr/bin/mount", "mount", "-o", "zfsutil", "-t", "zfs", dataset, where, NULL);
			} else {
				execl("/usr/bin/mount", "mount", "-o", options, "-t", "zfs", dataset, where, NULL);
			}
			exit(254);
		} else if (pid < 0) {
			fprintf(stderr, "Can not fork\n");
		}
		waitpid(pid, &status, 0);
		if (status != 0) {
			fprintf(stderr, "ZFS command failed with exit code %d, bailing out\n", status);
			free(where);
			break;
		}
		free(where);

loopend:
		lineToken = strtok_r(NULL, "\n", &endLine);
	}

	free(lines);
	
	free(pool);
	free(mountpoint);
	if (options != NULL) {
		free(options);
	}
}
