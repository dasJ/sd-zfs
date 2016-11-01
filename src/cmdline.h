#ifndef CMDLINE_H
#define CMDLINE_H

/*
 * Reads kernel parameter 'paramName' and returns it into 'value'.
 * Returns 0 on success, 1 if not found and < 0 if there was an error.
 * Make sure that 'paramName' ends with '='
 */
int cmdline_getParam(char *paramName, char **value);

#endif /* CMDLINE_H */
