
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>

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
	    (int)tv.tv_usec, gb, tgb);
    fflush(stdout);
}

off_t
getofft(char *s)
{
    off_t x = 0;

    while (isdigit(*s))
    {
    	x *= 10;
	x += *s++ - '0';
    }

    if (*s == 'm' || *s == 'M')
    	x *= 1024*1024;
    else if (*s == 'g' || *s == 'G')
    	x *= 1024*1024*1024;
    else if (*s == 'k' || *s == 'K')
    	x *= 1024;
    else if (*s == 'b' || *s == 'B')
    	x *= 512;

    return x;
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
    off_t size = -1;
    int errs = 0;
    int dots = 0;
    char *rdev;
    int fflag = 0;

    printf("sizeof (off_t) = %lu\n", sizeof (off_t));

    if (argc > 1 && strcmp(argv[1], "-f") == 0)
    {
	fflag = 1;
	argc--;
	argv++;
    }

    if (argc > 2)
    {
	pos = getofft(argv[1]);
	lastdot = pos;
	argc--;
	argv++;

	if (argc > 2)
	{
	    size = getofft(argv[1]);
	    argc--;
	    argv++;
	}
    }

    if (argc != 2)
	usage();

    fd = open(argv[1], O_RDONLY);

    if (fd < 0)
	perror(argv[1]);

    dots = pos / (1024*1024);
    printtime(dots);

    while (1)
    {
	int rsize;
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

	rsize = sizeof buf;

	if (size >= 0)
	{
	    if (rsize > size)
		rsize = size;

	    if (rsize == 0)
		break;
	}

	if (lseek(fd, pos, SEEK_SET) < 0)
	    printf("lseek failed @%ld\n", pos);

	act = read(fd, buf, rsize);
//	act = rsize;

	if (act == 0)
	    break;

	if (act > 0)
	{
	    if (act > rsize)
	    {
		printf("weird read gave too many bytes %d v. %d\n", act, rsize);
		act = rsize;
	    }

	    pos += act;
	    size -= act;
	    continue;
	}

	/* must be a bad block, see if we can narrow it down */
	printf("Bad 64K block @%ld\n", pos);
	printf("fd %d buf %p rsize %d act %d\n", fd, buf, rsize, act);
	perror("read");

	if (fflag)
	{
	    for (i = 0; i < rsize / 512; i++)
	    {
		if (lseek(fd, pos, SEEK_SET) < 0)
		    printf("lseek failed @%ld\n", pos);

		act = read(fd, buf, 512);

		if (act == 0)
			break;

		if (act > 0)
		{
		    pos += act;
		    continue;
		}

		/* we've narrowed down a bad block, report it */
		printf("Bad 512 block @%ld\n", pos);

		/* now skip it */
		pos += 512;
		errs++;
	    }
	}
	else
	{
	    pos += rsize;
	    errs++;
	}

	size -= rsize;
    }

    printf("\n");

    if ((dots % 50) != 0)
	printtime(dots);

    close(fd);
    exit(errs ? 1 : 0);
}
