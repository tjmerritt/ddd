
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/time.h>

void
printtime(int meg)
{
    char timebuf[20];
    struct tm *t;
    struct timeval tv;
    time_t timeval;
    int gb = meg / 1024;
    int tgb = ((meg - gb * 1024) * 100) / 1024;

    gettimeofday(&tv, NULL);
    timeval = tv.tv_sec;
    t = localtime(&timeval);
    printf("%02d:%02d:%02d.%06d %2d.%02dG ", t->tm_hour, t->tm_min, t->tm_sec,
	    tv.tv_usec, gb, tgb);
    fflush(stdout);
}

void
usage()
{
	fprintf(stderr, "Usage: ddd raw_device_file\n");
	exit(2);
}

int
main(int argc, char **argv)
{
	int fd;
	unsigned char buf[64*1024];
	off_t pos = 0;
	off_t lastdot = 0;
	int errs = 0;
	int dots = 0;

	if (argc != 2)
		usage();

	fd = open(argv[1], O_RDONLY);

	if (fd < 0)
		perror(argv[1]);

	printtime(0);

	while (1)
	{
		int act;
		int i;

		if (pos - lastdot >= 1024*1024)
		{
			printf(".");
			fflush(stdout);
			lastdot += 1024*1024;
			dots++;

			if ((dots % 50) == 0)
			{
				printf("\n");
				printtime(dots);
			}
		}

		if (lseek(fd, pos, SEEK_SET) < 0)
			printf("lseek failed @%lld\n", pos);

		act = read(fd, buf, sizeof buf);

		if (act == 0)
			break;

		if (act > 0)
		{
			pos += act;
			continue;
		}

		/* must be a back block, see if we can narrow it down */
		printf("Bad 64K block @%lld\n", pos);

		for (i = 0; i < 128; i++)
		{
			if (lseek(fd, pos, SEEK_SET) < 0)
				printf("lseek failed @%lld\n", pos);

			act = read(fd, buf, 512);

			if (act == 0)
				break;

			if (act > 0)
			{
				pos += act;
				continue;
			}

			/* we've narrowed down a bad block, report it */
			printf("Bad 512 block @%lld\n", pos);

			/* now skip it */
			pos += 512;
			errs++;
		}
	}

	close(fd);
	exit(errs ? 1 : 0);
}
