#ifndef CMDLINE_H
#define CMDLINE_H

#ifndef CMDLINE_FILE
#define CMDLINE_FILE "/proc/cmdline"
#endif /* CMDLINE_FILE */

/*
 * Reads kernel parameter 'paramName' and returns it into 'value'.
 * Returns 0 on success, 1 if not found and < 0 if there was an error.
 * Make sure that 'paramName' ends with '='
 */
int cmdline_getParam(char *paramName, char **value);

/*
 * Sets value to 1 if switchName is found on the command line (like rw).
 * Sets it to 0 if it was not found.
 */
int cmdline_getSwitch(char *switchName, char *value);

#endif /* CMDLINE_H */
