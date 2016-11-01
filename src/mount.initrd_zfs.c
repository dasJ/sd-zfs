#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

/*
 * Tries to get the pool to mount from the bootfs value
 */
int handleBootfs(char **pool) {
	char *rpool = NULL;
	char *line = NULL;
	char *linebuffer;
	int nRead;
	size_t size;
	pid_t pid;
	int zpool[2];
	char *tok;
	int ret = 1;

	// Handle rpool
	if (strncmp(*pool, "zfs:AUTO:", strlen("zfs:AUTO:")) == 0) {
		rpool = malloc((strlen(*pool) - strlen("zfs:AUTO:") + 1) * sizeof(char));
		strcpy(rpool, &((*pool)[strlen("zfs:AUTO:")]));
	}

	// Let's go!
	pipe(zpool);
	pid = fork();
	if (pid == 0) {
		close(1);
		dup(zpool[1]);
		if (rpool == NULL) {
			execl("/usr/bin/zpool", "zpool", "list", "-Ho", "bootfs", NULL);
		} else {
			execl("/usr/bin/zpool", "zpool", "list", "-Ho", "bootfs", rpool, NULL);
		}
		exit(254);
	} else if (pid < 0) {
		fprintf(stderr, "Can not fork\n");
		free(rpool);
		return -1;
	}
	close(zpool[1]);

	linebuffer = malloc(16 * sizeof(char));
	while (1) {
		while ((nRead = read(zpool[0], linebuffer, 16)) > 0) {
			if (line == NULL) {
				line = malloc(nRead + 1);
				memcpy(line, linebuffer, nRead);
				line[nRead] = '\0';
			} else {
				size = strlen(line) + nRead + 1;
				line = realloc(line, size);
				memcpy(&line[strlen(line)], linebuffer, nRead);
				line[size] = '\0';
			}
		}
		if (nRead == 0) {
			break;
		}
	}
	waitpid(pid, 0, 0);
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
	free(linebuffer);
	free(line);

	close(zpool[0]);
	
	free(rpool);
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
	// For mounting
	int children[2];
	pid_t pid;
	char *linebuffer;
	char *lines = NULL;
	size_t nRead;
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

	// Mount the dataset(s)
	pipe(children);
	pid = fork();
	if (pid == 0) {
		close(1);
		dup(children[1]);
		execl("/usr/bin/zfs", "zfs", "list", "-r", pool, "-t", "filesystem", "-Ho", "name,mountpoint", NULL);
		exit(254);
	} else if (pid < 0) {
		fprintf(stderr, "Can not fork\n");
		free(pool);
		free(mountpoint);
		if (options != NULL) {
			free(options);
		}
		close(children[0]);
		close(children[1]);
	}
	close(children[1]);

	linebuffer = malloc(16 * sizeof(char));
	while (1) {
		while ((nRead = read(children[0], linebuffer, 16)) > 0) {
			if (lines == NULL) {
				lines = malloc(nRead + 1);
				memcpy(lines, linebuffer, nRead);
				lines[nRead] = '\0';
			} else {
				size = strlen(lines) + nRead + 1;
				lines = realloc(lines, size);
				memcpy(&lines[strlen(lines)], linebuffer, nRead);
				lines[size] = '\0';
			}
		}
		if (nRead == 0) {
			break;
		}
	}
	waitpid(pid, 0, 0);
	free(linebuffer);
	close(children[0]);

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
