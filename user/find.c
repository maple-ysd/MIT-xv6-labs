#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

void find(const char *path, const char *filename)
{
	char buf[512];
	char *p;
	struct stat st, st1;
	struct dirent de;
	int fd;

	if ((fd = open(path, 0)) < 0){
		printf("find: cannot open %s\n", path);
		return;
	}
	if ((fstat(fd, &st)) < 0){
		printf("find: cannot stat %s\n", path);
		close(fd);
		return;
	}
	switch(st.type){
		case T_FILE:
			printf("find: <path> has to be a directory\n");
			break;
		case T_DIR:
			if(strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf)){
     				 printf("find: path too long\n");
     				 break;
   		 	}
			strcpy(buf, path);
			p = buf + strlen(path);
			*p++ = '/';
			while (read(fd, &de, sizeof(de)) == sizeof(de)){
				if (de.inum == 0)
					continue;
				if (!strcmp(de.name, ".") || !strcmp(de.name, ".."))
					continue;

				memmove(p, de.name, DIRSIZ);
				p[DIRSIZ] = '\0';

				if ((stat(buf, &st1)) < 0)
					continue;
				else {
					if (st1.type == T_DIR)
						find(buf, filename);
					else if (st1.type == T_FILE && !strcmp(de.name, filename))
						printf("%s\n", buf);
					else
						continue;
				}
			}
			break;
	}
	close(fd);
}

int main(int argc, char *argv[])
{
	if (argc < 3){
		printf("Usage: find <path> <filname>\n");
		exit(1);
	}
	find(argv[1], argv[2]);
	exit(0);
}
