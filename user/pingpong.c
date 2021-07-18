#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main()
{
	int p1[2];
	int p2[2];
	int pid;

	char buf[1] = {'p'};

	pipe(p1);
	pipe(p2);

	if (fork() == 0){
		close(p1[1]);
		close(p2[0]);
		if (read(p1[0], buf, 1) != 1){
			fprintf(2, "read failed...\n");
			exit(1);
		}
		pid = getpid();
		fprintf(1, "%d: received ping\n", pid);
		write(p2[1], buf, 1);
		exit(0);
	} else{
		close(p1[0]);
		close(p2[1]);
		write(p1[1], buf, 1);
		if (read(p2[0], buf, 1) != 1){
			fprintf(2, "read failed...\n");
			exit(1);
		}
		pid = getpid();
		fprintf(1, "%d: received pong\n", pid);
		wait(0);
		exit(0);
	}
}
