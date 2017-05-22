#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>

struct gps_location {
	int lat_integer;
	int lat_fractional;
	int lng_integer;
	int lng_fractional;
	int accuracy;
};

void divide_float(char* num, int *integer, int *fractional);

int main (int argc, char** argv) {
	struct gps_location loc_buf;
	double lat_buf, lng_buf;

	if (argc == 1) {
		printf("Not implemented!\n");
		return 0;
		printf("Use Chook Ji Bup Mode!\n");
		//printf("Jumped to Lat: %f, Lng: %f, Accuracy: %d\n");
	} else if (argc != 4) {
		printf("usage: ./gpsupdate.o [latitude] [longitude] [accuracy]\n");
		printf("example: ./gpsupdate.o 39.0392 125.7625 10\n");
		return 0;
	} else {
		divide_float(argv[1], &loc_buf.lat_integer, &loc_buf.lat_fractional);
		divide_float(argv[2], &loc_buf.lng_integer, &loc_buf.lng_fractional);
		loc_buf.accuracy = atoi(argv[3]);
	}

	int a = syscall(380, &loc_buf);

	return 0;
}

void divide_float(char* num, int *integer, int *fractional) {
	int i = 0;
	double num_buf;

	sscanf(num, "%lf", &num_buf);
	*integer = (int)num_buf;

	// find point position
	while(num[i] != '\0' && num[i] != '.') {
		i++;
	}

	if (num[i] == '.') {
		*fractional = atoi(&num[i+1]);
	} else {
		*fractional = 0;
	}
}
