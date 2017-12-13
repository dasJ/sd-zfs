#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include "zfs-util.h"

// Buffer for reading ZFS command output
#define BUFSIZE 16

int execute(char *command, char needOutput, char **output, char *param[], char needPassword, char *prompt) {
	int pip[2], pwpip[2];
	pid_t pid, pwpid;
	int status, pwstatus;
	char *linebuffer;
	size_t size;
	int nRead;
	char **cmdline;

	if (needPassword == 1) {
		// Execute password prompt
		if (pipe(pwpip) == -1) {
			perror("Cannot create password pipe");
			exit(1);
		}
		pwpid = fork();
		if (pwpid == 0) {
			// Redirect stdout to pipe
			while ((dup2(pwpip[1], STDOUT_FILENO) == -1) && (errno == EINTR)) {}
			if (errno != 0) {
				perror("Cannot connect stdout to parent password pipe");
				exit(1);
			}
			// Close pipe, we won't need it anymore
			close(pwpip[1]);
			// Close stdin to prevent weird redirection
			close(0);
			// Execute
			if (prompt == NULL) {
				cmdline = (char*[]) { SYSTEMD_ASK_PASS_CMD, NULL };
			} else {
				cmdline = (char*[]) { SYSTEMD_ASK_PASS_CMD, prompt, NULL };
			}
			execv(SYSTEMD_ASK_PASS_EXE, cmdline);
			exit(254);
		} else if (pwpid < 0) {
			perror("Cannot fork");
			close(pwpip[0]);
			close(pwpip[1]);
			return pwpid;
		}
	}

	// Execute
	if (needOutput == 1) {
		if (pipe(pip) == -1) {
			perror("Cannot create output pipe");
			exit(1);
		}
	}
	pid = fork();
	if (pid == 0) {
		if (needPassword == 1) {
			// Redirect password pipe to stdin
			while ((dup2(pwpip[0], STDIN_FILENO) == -1) && (errno == EINTR)) {}
			if (errno != 0) {
				perror("Cannot connect stdin to parent password pipe");
				exit(1);
			}
			// Close pipe, we won't need them anymore
			close(pwpip[0]);
		}

		// Set up pipe
		if (needOutput == 1) {
			// Redirect stdout to pipe
			while ((dup2(pip[1], STDOUT_FILENO) == -1) && (errno == EINTR)) {}
			if (errno != 0) {
				perror("Cannot connect stdout to parent output pipe");
				exit(1);
			}
			// Close pipe, we won't need it anymore
			close(pip[0]);
			close(pip[1]);
		} else {
			// Close stdout so it doesn't get written to the journal
			close(1);
		}
		// Execute
		execv(command, param);
		exit(254);
	} else if (pid < 0) {
		perror("Cannot fork");
		if (needOutput == 1) {
			close(pip[0]);
			close(pip[1]);
		}
		return pid;
	}
	// Close out the password pipe
	if (needPassword == 1) {
		close(pwpip[0]);
		close(pwpip[1]);
	}
	// Capture output
	if (needOutput == 1) {
		// We don't need to write to the pipe
		close(pip[1]);
		// Read lines
		linebuffer = malloc(BUFSIZE * sizeof(char));
		while (1) {
			while ((nRead = read(pip[0], linebuffer, BUFSIZE)) > 0) {
				// Append buffer to output
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
		close(pip[0]);
		free(linebuffer);
	}
	// Wait for quit
	if (needPassword == 1) {
		waitpid(pwpid, &pwstatus, 0);
	}
	waitpid(pid, &status, 0);
	if (needPassword == 1 && pwstatus != 0) {
		return pwstatus;
	}
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
	return execute(ZFS_EXE, needOutput, output, param, 0, NULL);
}

/*
 * Executes a zfs command, feeding it the input from a systemd password
 * prompt to stdin.
 * If needOutput is 1, the output of the command is written to output
 * which will be allocated. It must be NULL when passing in.
 * param must be a null-terminated array of parameters where the first
 * is ZFS_CMD
 */
int executeZfsWithPassword(char needOutput, char **output, char *param[], char *prompt) {
	return execute(ZFS_EXE, needOutput, output, param, 1, prompt);
}

/*
 * Executes a zpool command.
 * If needOutput is 1, the output of the command is written to output
 * which will be allocated. It must be NULL when passing in.
 * param must be a null-terminated array of parameters where the first
 * is ZPOOL_CMD
 */
int executeZpool(char needOutput, char **output, char *param[]) {
	return execute(ZPOOL_EXE, needOutput, output, param, 0, NULL);
}

/*
 * Executes a mount command.
 * If needOutput is 1, the output of the command is written to output
 * which will be allocated. It must be NULL when passing in.
 * param must be a null-terminated array of parameters where the first
 * is MOUNT_CMD
 */
int executeMount(char needOutput, char **output, char *param[]) {
	return execute(MOUNT_EXE, needOutput, output, param, 0, NULL);
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

int zfs_ds_requires_password(char *dataset) {
	char *output = NULL;
	char *cmdline[] = { ZFS_CMD, "get", "-Ho", "value", "encryption,keyformat", dataset, NULL };
	char *status = NULL;
	char *keytype = NULL;

	if (executeZfs(1, &output, cmdline) != 0) {
		return -1;
	}

	status = strtok(output, "\n");
	if (strcmp(status, "off") != 0) {
		keytype = strtok(NULL, "\n");
		if (strcmp(keytype, "passphrase") == 0) {
			return 1;
		}
	}

	return 0;
}

int zfs_decrypt_ds_with_password(char *dataset) {
	int ret;
	char *encroot = NULL;
	char **cmdline;
	const char *promptMessage = "Enter passphrase for '%s':";
	char *prompt;

	cmdline = (char*[]) { ZFS_CMD, "get", "-Ho", "value", "encryptionroot", dataset, NULL };
	if (executeZfs(1, &encroot, cmdline) != 0) {
		return 0;
	}

	if (encroot != NULL) {
		(encroot)[strlen(encroot) - 1] = '\0';
	}

	cmdline = (char*[]) { ZFS_CMD, "load-key", encroot, NULL };
	prompt = (char*)malloc((strlen(promptMessage) - 2 + strlen(encroot)) * sizeof(char));
	sprintf(prompt, promptMessage, encroot);

	ret = executeZfsWithPassword(0, NULL, cmdline, prompt);
	if (ret == 0) {
		return 1;
	} else {
		return 0;
	}
}
