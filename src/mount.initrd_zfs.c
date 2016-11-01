#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include "cmdline.h"

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
			execl("/usr/bin/zpool", "/usr/bin/zpool", "list", "-Ho", "bootfs", NULL);
		} else {
			execl("/usr/bin/zpool", "/usr/bin/zpool", "list", "-Ho", "bootfs", rpool, NULL);
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
		printf("Line is %s\n", argv[i]);
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
	// TODO Mount the dataset(s)
	printf("Pool: %s\n", pool);
	//printf("Mountpoint: %s\n", mountpoint);
	//printf("Options: %s\n", options);

	free(pool);
	free(mountpoint);
	if (options != NULL) {
		free(options);
	}
}
