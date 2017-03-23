#!/bin/sh

if [ "$#" -lt 1 ]; then
	echo "usage: ./build.sh [project_num]"
elif [ "$1" -eq 1 ]; then
	arm-linux-gnueabi-gcc -I ../os-team20/include test1.c -o test
fi
