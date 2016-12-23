#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <string.h>
#include <sys/time.h>
#include <linux/limits.h>

//access to a remote file using MPI
//test function:
//open, read, write, flush, close

const int ROOT=0;

#define TIME(st, et) (((et).tv_sec-(st).tv_sec)*1000000+(et).tv_usec-(st).tv_usec)

int main(int argc, char ** argv)
{
	int fd;
	int rank, number, i=0;
	char *buffer=NULL;
	size_t total_size=0;
	ssize_t ret;
	struct stat file_stat;
	char   test;
	struct timeval st, et;
	char file_path[PATH_MAX];
	const char *mount_point=getenv("CBB_CLIENT_MOUNT_POINT");

	if(NULL == mount_point)
	{
		fprintf(stderr, "please set CBB_CLIENT_MOUNT_POINT");
		return EXIT_FAILURE;
	}
	sprintf(file_path, "%s%s", mount_point, "/test1");

	fd=open(file_path, O_WRONLY);
	fstat(fd, &file_stat);
	if(NULL == (buffer=malloc(file_stat.st_size)))
	{
		perror("malloc");
		return EXIT_FAILURE;
	}

	gettimeofday(&st, NULL);

	if(-1 == (ret=write(fd, buffer, file_stat.st_size)))
	{
		perror("read");
		return EXIT_FAILURE;
	}
	if(-1 == (ret=lseek(fd, 0, SEEK_SET)))
	{
		perror("lseek");
		return EXIT_FAILURE;
	}
	printf("seek to begin\n");
	printf("read again\n");
	scanf("%c", &test);
	if(-1 == (ret=write(fd, buffer, file_stat.st_size)))
	{
		perror("read");
		return EXIT_FAILURE;
	}
	total_size+=ret;

	gettimeofday(&et, NULL);

	printf("total size %lu\n", total_size);
	printf("throughput %fMB/s\n", (double)total_size/TIME(st, et));

	return EXIT_SUCCESS;
}
