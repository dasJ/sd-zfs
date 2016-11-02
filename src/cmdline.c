#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cmdline.h"

int getCmdline(char **cmdline) {
	FILE *fd;
	size_t linelen;

	fd = fopen("/proc/cmdline", "r");
	if (fd == NULL) {
		fprintf(stderr, "Can not open kernel command line\n");
		return -1;
	}
	if (getline(cmdline, &linelen, fd) < 0) {
		fprintf(stderr, "Can not read kernel command line\n");
		fclose(fd);
		return -2;
	}
	fclose(fd);
	return linelen;
}

int cmdline_getParam(char *paramName, char **value) {
	char *cmdline = NULL;
	char *param;

	if (getCmdline(&cmdline) < 0) {
		return -1;
	}
	// Parse tokens of command line
	param = strtok(cmdline, " \t\n");
	do {
		if (strncmp(param, paramName, strlen(paramName)) == 0) {
			*value = malloc((strlen(param) - strlen(paramName) + 1) * sizeof(char));
			strcpy(*value, &(param[strlen(paramName)]));
			free(cmdline);
			return 0;
		}
	} while ((param = strtok(NULL, " \t\n")) != NULL);

	free(cmdline);
	return 1;
}

int cmdline_getSwitch(char *switchName, char *value) {
	char *cmdline = NULL;
	char *param;

	if (getCmdline(&cmdline) < 0) {
		return -1;
	}

	*value = 0;
	// Parse tokens of command line
	param = strtok(cmdline, " \t\n");
	do {
		if (strcmp(param, switchName) == 0) {
			*value = 1;
			break;
		}
	} while ((param = strtok(NULL, " \t\n")) != NULL);

	free(cmdline);
	return 0;
}
