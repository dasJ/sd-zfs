#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include "zfs-util.h"

// Buffer for reading ZFS command output
#define BUFSIZE 16
#define ZFS_EXE "/usr/bin/zfs"
#define ZPOOL_EXE "/usr/bin/zpool"
#define MOUNT_EXE "/usr/bin/mount"
#define ZFS_CMD "zfs"
#define ZPOOL_CMD "zpool"
#define MOUNT_CMD "mount"

int execute(char *command, char needOutput, char **output, char *param[]) {
	int pip[2];
	pid_t pid;
	int status;
	char *linebuffer;
	size_t size;
	int nRead;
	int fd;

	// Execute
	if (needOutput == 1) {
		pipe(pip);
	}
	pid = fork();
	if (pid == 0) {
		// Set up pipe
		close(1);
		close(2);
		if (needOutput == 1) {
			close(pip[0]);
			fd = dup(pip[1]);
			if (fd < 0) {
				perror("Can not duplicate pipe\n");
				close(pip[1]);
			}
		}
		// Execute
		execv(command, param);
		close(fd);
		exit(254);
	} else if (pid < 0) {
		perror("Can not fork\n");
		if (needOutput == 1) {
			close(pip[0]);
			close(pip[1]);
		}
		return pid;
	}
	if (needOutput == 1) {
		close(pip[1]);
		// Read lines
		linebuffer = malloc(BUFSIZE * sizeof(char));
		while (1) {
			while ((nRead = read(pip[0], linebuffer, BUFSIZE)) > 0) {
				if (*output == NULL) {
					*output = malloc(nRead + 1);
					memcpy(*output, linebuffer, nRead);
					(*output)[nRead] = '\0';
				} else {
					size = strlen(*output) + nRead + 1;
					*output = realloc(*output, size);
					memcpy(&(*output)[strlen(*output)], linebuffer, nRead);
					(*output)[size - 1] = '\0';
				}
			}
			if (nRead == 0) {
				break;
			}
		}
		free(linebuffer);
	}
	// Wait for quit
	waitpid(pid, &status, 0);
	return status;
}

/*
 * Executes a zfs command.
 * If needOutput is 1, the output of the command is written to output
 * which will be allocated. It must be NULL when passing in.
 * param must be a null-terminated array of parameters where the first
 * is ZFS_CMD
 */
int executeZfs(char needOutput, char **output, char *param[]) {
	return execute(ZFS_EXE, needOutput, output, param);
}

/*
 * Executes a zpool command.
 * If needOutput is 1, the output of the command is written to output
 * which will be allocated. It must be NULL when passing in.
 * param must be a null-terminated array of parameters where the first
 * is ZPOOL_CMD
 */
int executeZpool(char needOutput, char **output, char *param[]) {
	return execute(ZPOOL_EXE, needOutput, output, param);
}

/*
 * Executes a mount command.
 * If needOutput is 1, the output of the command is written to output
 * which will be allocated. It must be NULL when passing in.
 * param must be a null-terminated array of parameters where the first
 * is MOUNT_CMD
 */
int executeMount(char needOutput, char **output, char *param[]) {
	return execute(MOUNT_EXE, needOutput, output, param);
}

int zfs_destroy_recursively(char *dataset) {
	char *lines = NULL;
	char *token;

	// Execute command
	char *cmdline[] = { ZFS_CMD, "list", "-tfilesystem", "-Hro", "name", "-Sname", dataset, NULL };
	executeZfs(1, &lines, cmdline);
	if (lines == NULL) {
		return 0;
	}
	// Destoy all datasets
	token = strtok(lines, "\n");
	do {
		if (zfs_destroy(token) != 0) {
			fprintf(stderr, "Not destroying any more datasets\n");
			free(lines);
			return -2;
		}
	} while ((token = strtok(NULL, "\n")) != NULL);

	free(lines);
	return 0;
}

int zfs_destroy(char *dataset) {
	int status;
	char *cmdline[] = { ZFS_CMD, "destroy", dataset, NULL };

	status = executeZfs(0, NULL, cmdline);
	if (status != 0) {
		fprintf(stderr, "ZFS returned error %d\n", status);
	}
	return status;
}

int zfs_snapshot_exists(char *dataset, char *snapshot) {
	int status;
	char *toCheck;

	toCheck = malloc((strlen(dataset) + strlen(snapshot) + 2) * sizeof(char));
	strcpy(toCheck, dataset);
	strcat(toCheck, "@");
	strcat(toCheck, snapshot);
	
	status = zfs_ds_exists(toCheck);
	free(toCheck);
	return status;
}

int zfs_ds_exists(char *dataset) {
	int status;
	char *cmdline[] = { ZFS_CMD, "get", "-H", "type", dataset, NULL };

	status = executeZfs(0, NULL, cmdline);

	if (status == 0) {
		return 0;
	} else {
		return 1;
	}
}

int zfs_get_bootfs(char *rpool, char **bootfs) {
	int status;
	char **cmdline;
	char *tok;
	char *line = NULL;

	if (rpool == NULL) {
		cmdline = (char*[]) { ZPOOL_CMD, "list", "-Ho", "bootfs", NULL };
	} else {
		cmdline = (char*[]) { ZPOOL_CMD, "list", "-Ho", "bootfs", rpool, NULL };
	}

	*bootfs = NULL;

	status = executeZpool(1, &line, cmdline);

	if (status != 0) {
		fprintf(stderr, "zpool get returned %d\n", status);
		return status;
	}
	status = 1;

	tok = strtok(line, "\n");
	while (tok != NULL) {
		if (strcmp(tok, "-") != 0) {
			*bootfs = malloc((strlen(tok) + 1) * sizeof(char));
			strcpy(*bootfs, tok);
			status = 0;
			break;
		}
		tok = strtok(NULL, "\n");
	}
	free(line);
	return status;
}

int zfs_list_datasets_with_mp(char *dataset, char **datasets) {
	int status;
	char *cmdline[] = { ZFS_CMD, "list", "-r", dataset, "-t", "filesystem", "-Ho", "name,mountpoint", NULL };

	*datasets = NULL;

	status = executeZfs(1, datasets, cmdline);

	if (status != 0) {
		fprintf(stderr, "zfs returned %d\n", status);
	}
	return status;
}

int zfs_list_snapshots(char *dataset, char *snapshot, char **output) {
	int status;
	char *snaps = NULL;
	char *cmdline[] = { ZFS_CMD, "list", "-Ho", "name", "-tsnapshot", "-r", dataset, NULL };
	char *suffix;
	char *tok;

	status = executeZfs(1, &snaps, cmdline);
	if (status != 0) {
		fprintf(stderr, "zfs returned %d\n", status);
		return status;
	}
	// Filter
	suffix = malloc((strlen(snapshot) + 2) * sizeof(char));
	strcpy(suffix, "@");
	strcat(suffix, snapshot);

	*output = malloc(sizeof(char));
	strcpy(*output, "");

	tok = strtok(snaps, "\n");
	while (tok != NULL) {
		if (strlen(tok) < strlen(suffix)) {
			continue;
		}
		if (strncmp(tok + strlen(tok) - strlen(suffix), suffix, strlen(suffix)) == 0) {
			*output = realloc(*output, strlen(*output) + strlen(tok) + 2);
			strcat(*output, tok);
			strcat(*output, "\n");
		}
		tok = strtok(NULL, "\n");
	}
	free(suffix);
	free(snaps);
	return 0;
}

int zfs_mount(char *what, char *where, char *options) {
	char **cmdline;

	if (options == NULL) {
		cmdline = (char*[]) { MOUNT_CMD, "-t", "zfs", what, where, NULL };
	} else {
		cmdline = (char*[]) { MOUNT_CMD, "-t", "zfs", "-o", options, what, where, NULL };
	}

	return executeMount(0, NULL, cmdline);
}

int zfs_get_mountpoint(char *dataset, char **mountpoint) {
	int status;
	char *cmdline[] = { ZFS_CMD, "get", "-Ho", "value", "mountpoint", dataset, NULL };

	status = executeZfs(1, mountpoint, cmdline);
	if (status != 0) {
		fprintf(stderr, "zfs returned %d\n", status);
	}
	if (*mountpoint != NULL) {
		(*mountpoint)[strlen(*mountpoint) - 1] = '\0';
	}
	return status;
}

int zfs_clone_snap(char *snapshot, char *datasetTarget, char *mountpoint) {
	int status;
	char **cmdline;
	char *mountpointParam;

	mountpointParam = malloc((strlen("org.zol:mountpoint=") + strlen(mountpoint) + 1) * sizeof(char));
	strcpy(mountpointParam, "org.zol:mountpoint=");
	strcat(mountpointParam, mountpoint);

	cmdline = (char*[]) { ZFS_CMD, "clone",
		"-o", "canmount=noauto",
		"-o", "mountpoint=none",
		"-o", mountpointParam, snapshot, datasetTarget, NULL };

	status = executeZfs(0, NULL, cmdline);
	free(mountpointParam);
	if (status != 0) {
		fprintf(stderr, "zfs returned %d\n", status);
	}

	return status;
}

int zfs_get_alt_mp(char *dataset, char **mountpoint) {
	int status;
	char *cmdline[] = { ZFS_CMD, "get", "-Ho", "value", "org.zol:mountpoint", dataset, NULL };

	status = executeZfs(1, mountpoint, cmdline);
	if (*mountpoint != NULL) {
		(*mountpoint)[strlen(*mountpoint) - 1] = '\0';
	}
	return status;
}
