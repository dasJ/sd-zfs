#include <unistd.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
	execl("/usr/bin/zpool", "zpool", "export", "-af", NULL);
	exit(254);
}
