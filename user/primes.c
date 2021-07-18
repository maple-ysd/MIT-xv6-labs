#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
static int N = 35;
void primes(int fd)
{
	int i;
	int x;	// prime number
	int p[2];
	// read the leftest number which must be a prime number
	if (read(fd, &x, 4) && x <= N && x > 2)
		printf("prime %d\n", x);

	else{
		exit(0);
	}
	pipe(p);
	if (fork() == 0){
		close(p[1]);
		primes(p[0]);
	}else{
		// select the remaining numbers
		while (read(fd, &i, 4) && i <= N && i > 2){
			if (i % x != 0)
				write(p[1], &i, 4);
		}
		close(fd);
		close(p[0]);
		close(p[1]);
		wait(0);
		exit(0);
	}


}
int main()
{
	int i;
	int p[2];
	
	printf("prime %d\n", 2);
	pipe(p);
	
	if (fork() == 0){
		close(p[1]);
		primes(p[0]);
		exit(0);
	} else{
		for (i = 3; i <= N; ++i){
			if (i % 2 != 0)
				write(p[1], &i, 4);
		}
		close(p[0]);
		close(p[1]);
		wait(0);
		exit(0);
	}	
}
