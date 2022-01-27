#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "change_resolution_ratio.h"





int main(int argc, char *argv[])
{
	
	
	if(!argv[1] || !argv[2] || !argv[3] || !argv[4])
	{
		printf("usage: srcFileName, dstFileName, dstWidth, dstHeight\n");
		return 0;
	}
	
	int width = atoi(argv[3]);
	int height = atoi(argv[4]);
	change_resolution_ratio(argv[1], argv[2], width, height);
	
	return 0;
}


