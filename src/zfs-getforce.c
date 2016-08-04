#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
	char *cmdline = NULL;
	size_t linelen;
	FILE *fd;
	char force;
	char *param;
	char *value;

	// Read cmdline
	fd = fopen("/proc/cmdline", "r");
	if (fd == NULL) {
		printf("Can not open cmdline\n");
		exit(1);
	}
	if (getline(&cmdline, &linelen, fd) < 0) {
		printf("Can not read cmdline\n");
		fclose(fd);
		exit(1);
	}
	fclose(fd);

	// Parse tokens
	param = strtok(cmdline, " \t\n");
	do {
		if (strncmp(param, "zfs_force=", strlen("zfs_force=")) == 0) {
			value = malloc((strlen(param) - strlen("zfs_force=") + 1) * sizeof(char));
			strcpy(value, &param[10]);
			force = ((strcmp(value, "1") == 0) || (strcmp(value, "yes") == 0));
			free(value);
		}
	} while ((param = strtok(NULL, " \t\n")) != NULL);

	free(cmdline);
	free(param);
	
	// Write file
	fd = fopen("/etc/conf.d/zfs", "w");
	if (fd == NULL) {
		printf("Can not open output file\n");
		exit(1);
	}
	fprintf(fd, "FORCE=\"%s\"\n", ((force == 1) ? "-f" : ""));
	fclose(fd);

	exit(0);
}
