#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

int main(int argc, char *argv[])
{
	char *arr[MAXARG];
	int i, n = 0;
	if (argc == 1){
		arr[0] = (char*)malloc(strlen("echo") + 1);
		strcpy(arr[n], "echo");
		++n;
	} else{
		i = 1;
		if (!strcmp(argv[1], "-n"))
			i = 3;		// skip "-n" "1"
		for (; i < argc; ++i, ++n){
			arr[n] = (char *)malloc(strlen(argv[i]) + 1);
			strcpy(arr[n], argv[i]);
		}
	}
	arr[n] = 0;
	char buf[1024];
	char *p, *q;
	int m = 0, cnt = 0;
	/*
	printf("before fork:\n*************\n");
	for (i = 0; i < n; ++i)
		printf("%s\n", arr[i]);
	printf("**************\n");
	*/
	while ((cnt = read(0, &buf + m, sizeof(buf) - m - 1)) != 0){
		m += cnt;
		buf[m] = '\0';
		p = buf;
		while ((q = strchr(p, '\n')) != 0){
			*q = '\0';
			arr[n] = (char *)malloc(strlen(p) + 1);
			strcpy(arr[n], p);
			arr[n + 1] = 0;
			/*
			printf("when fork:\n*************\n");
       			for (i = 0; i <= n; ++i)
                		printf("%s\n", arr[i]);
        		printf("**************\n");
			*/

			if (fork() == 0){
                		exec(arr[0], arr);
                		fprintf(2, "exec failed...\n");
                		exit(1);
			}
			wait(0);
			free(arr[n]);
			arr[n] = 0;
			p = q + 1;
		}
		if (m > 0){
			m -= p - buf;
			memmove(buf, p, m);
		}
	}
	for (i = 0; i < n; ++i)
		free(arr[i]);
	exit(0);
}
