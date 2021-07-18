#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// sleep time int unit: 100 ms / tick
int main(int argc, char *argv[])
{
	int n;
	if (argc < 2){
		fprintf(2, "Usage: sleep <n> ticks\n");
		exit(1);
	}
	n = atoi(argv[1]);
	sleep(n);
	exit(0);
}
