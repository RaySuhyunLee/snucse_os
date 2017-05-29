#include<stdlib.h>
#include<stdio.h>
#include<unistd.h>
#include<sys/types.h>
#include<errno.h>

struct gps_location {
	int lat_integer;
	int lat_fractional;
	int lng_integer;
	int lng_fractional;
	int accuracy;
};


int main (int argc, char** argv) {
	int ret;
	char *path = argv[1];
	printf("%s\n",path);

 	struct gps_location* file_loc = malloc (sizeof(struct gps_location));
	ret = syscall(381, path, file_loc);
	if (ret != 0) {
		printf("System call returned with %d.\n", errno);
		printf("Abort...\n");
		return -1;
	}

	printf("https://www.google.co.kr/maps/place/");
	printf("%d.%06d°N+%d.%06d°E\n",
			file_loc->lat_integer, file_loc->lat_fractional,
			file_loc->lng_integer, file_loc->lng_fractional);

	return 0;
}
	
