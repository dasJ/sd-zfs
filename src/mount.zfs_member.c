#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Not sure if this is obvious, but I'm not really a C programmer.
 * The code might be awful, but I can't do any better.
 * Please file a PR if you can improve anything.
 * But please do not replace the function pointer logic.
 * It's just awful here but I love it.
 */

typedef int (*MOUNTER_PTR)(char*, char*);

void parseOptions(int argc, char *argv[], char **options) {
	*options = NULL;

	char isOption = 0;

	if (argc < 3) {
        // This is fake, we just care about -o [...]
		printf("Usage: %s [-o {option}..] {disk} {mountpoint}\n", argv[0]);
		exit(1);
	}

	for (int i = 1; i < argc; i++) {
		// This is an option
		if (isOption == 1) {
			size_t optlen = strlen(argv[i]) + 2;
			if (*options != NULL) {
				optlen += strlen(*options);
			}
			char *newoptions = malloc(optlen * sizeof(char));
			if (*options != NULL) {
				strcpy(newoptions, *options);
			}
			strcat(newoptions, argv[i]);
            strcat(newoptions, ",");
			if (*options != NULL) {
				free(*options);
			}
			*options = newoptions;
			isOption = 0;
			continue;
		}
		// Next will be an option
		if (strcmp(argv[i], "-o") == 0) {
			isOption = 1;
			continue;
		}
        // This is something we don't care about
	}

	// Get rid of that damn comma
	if (*options != NULL) {
		char *newoptions = malloc(sizeof(*options) - sizeof(char));
		strncpy(newoptions, *options, strlen(*options) - 1);
		free(*options);
		*options = newoptions;
	}
}

int findBootfs(char **bootfs) {
	char *line = NULL;
	size_t linelen;
	FILE *proc;
	int ret = 0;

	proc = popen("/usr/bin/zpool get -Hp bootfs", "r");
	if (proc == NULL) {
		printf("Can not read bootfs value\n");
		ret = 1;
		goto done;
	}
	while (getline(&line, &linelen, proc) > 0) {
		// TODO Learn how to use strtok
		strtok(line, "\t");
		strtok(NULL, "\t");
		char *value = strtok(NULL, "\t");

		if (strcmp(value, "-") != 0) {
			//printf("Using bootfs of pool %s\n", line);
			*bootfs = strdup(value);
			goto done;
		}
	}
	printf("No bootfs value was found\n");
	ret = 1;
done:
	pclose(proc);
	free(line);
	return ret;
}

int mountLegacy(char *dataset, char *options) {
	// The two blanks in the string are needed, never ever touch dis!
	size_t commandlen = (strlen(dataset) + strlen("/usr/bin/mount.zfs  /sysroot") + 1) * sizeof(char);
	if (options != NULL) {
		commandlen += (strlen(options) + strlen(" -o ") * sizeof(char));
	}
	char *command = malloc(commandlen);
	strcpy(command, "/usr/bin/mount.zfs ");
    if (options != NULL) {
		strcat(command, "-o ");
		strcat(command, options);
		strcat(command, " ");
	}
	strcat(command, dataset);
	strcat(command, " /sysroot");

	int ret = system(command);
	free(command);
	return ret;
}

int mountNonLegacy(char *dataset, char *options) {
	char *line = NULL;
	char *commandBeginning;
    char *commandBeginningBeginning; // Variable naming: 10/10
	char *command;
	size_t linelen;
	FILE *proc;
	int ret = 0;

	commandBeginning = "/usr/bin/zfs list -Hr -o name,mountpoint ";
	command = malloc((strlen(commandBeginning) + strlen(dataset) + 1) * sizeof(char));
	strcpy(command, commandBeginning);
	strcat(command, dataset);

	// For later
	commandBeginningBeginning = "/usr/bin/zfs mount ";
	if (options != NULL) {
		commandBeginning = malloc((strlen(commandBeginningBeginning) + strlen(" -o ") + strlen(options) + 1) * sizeof(char));
		strcpy(commandBeginning, commandBeginningBeginning);
		strcat(commandBeginning, "-o ");
		strcat(commandBeginning, options);
		strcat(commandBeginning, " ");
	} else {
		commandBeginning = malloc((strlen(commandBeginningBeginning) + 1) * sizeof(char));
		strcpy(commandBeginning, commandBeginningBeginning);
	}

	// Check for any (sub)datasets
	proc = popen(command, "r");
	if (proc == NULL) {
		printf("Can not discover subdatasets\n");
		ret = 1;
		goto done;
	}
	while (getline(&line, &linelen, proc) > 0) {
		strtok(line, "\t");
		char *mountpoint = strtok(NULL, "\t");
		mountpoint[strlen(mountpoint) - 1] = 0;
		if ((strcmp(mountpoint, "none") == 0) || (strcmp(mountpoint, "legacy") == 0) || (strcmp(mountpoint, "-") == 0)) {
			continue;
		}
		// Mount dis
		free(command);
		command = malloc((strlen(commandBeginning) + strlen(line) + 1) * sizeof(char));
        strcpy(command, commandBeginning);
		strcat(command, line);
		ret = system(command);
		if (ret != 0) {
			goto done;
		}
	}

done:
    free(command);
	free(commandBeginning);
    pclose(proc);
	return ret;
}

int getMounter(char *dataset, MOUNTER_PTR *ptr) {
	char *line = NULL;
	size_t linelen;
	FILE *proc;
	int ret = 0;

	char *commandBeginning = "/usr/bin/zfs get mountpoint -Ho value ";
	char *command = malloc(sizeof(char) * (strlen(commandBeginning) + strlen(dataset) + 1));
	sprintf(command, "%s%s", commandBeginning, dataset);

	proc = popen(command, "r");
	if (proc == NULL) {
		printf("Can not check if dataset is legacy\n");
		ret = 1;
		goto done;
	}
	if (getline(&line, &linelen, proc) <= 0) {
		printf("Dataset not found\n");
		ret = 1;
		goto done;
	}
	if (strcmp(line, "legacy\n") == 0) {
		*ptr = mountLegacy;
	} else {
        *ptr = mountNonLegacy;
	}
done:
	pclose(proc);
	free(line);
	free(command);
	return ret;
}

int main(int argc, char *argv[]) {
	char *options;
	char *dataset = NULL;
	MOUNTER_PTR mounter;
	int ret = 0;

	parseOptions(argc, argv, &options);

	if (findBootfs(&dataset) != 0) {
        ret = 1;
		goto done;
	}

	if (getMounter(dataset, &mounter) != 0) {
        ret = 1;
		goto done;
	}

	ret = (*mounter)(dataset, options);

done:
	free(options);
	if (dataset != NULL) free(dataset);
    return ret;
}
