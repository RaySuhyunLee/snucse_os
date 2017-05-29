#include<sys/stat.h>
#include<fcntl.h>
#include<string.h>
#include<unistd.h>
#include<stdlib.h>
#include<stdio.h>
#include<stdint.h>
#include<sys/mman.h>
#include<sys/types.h>
#include<errno.h>

int main(int argc, char** argv) {
	char *path = argv[1];
	int fd = open(path, O_RDWR);	
	struct stat file_stat = {};
	
	if(fd<0) {
		perror("Error opening file");
		exit(EXIT_FAILURE);
	}

	if(fstat(fd,&file_stat)) {
		perror("Error getting the size");
		exit(EXIT_FAILURE);
	}

	char *map = mmap(0,file_stat.st_size,PROT_READ|PROT_WRITE, MAP_SHARED,fd,0);

	if(map == MAP_FAILED) {
		perror("mmap failed");
		exit(EXIT_FAILURE);
	}

	map[0] = 'A';

	if(munmap(map, file_stat.st_size) == -1) {
		close(fd);
		perror("Error un-mapping");
		exit(EXIT_FAILURE);
	}

	close(fd);

	return 0;
}

