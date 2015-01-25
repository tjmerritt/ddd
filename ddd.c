
/*****************************************************************************
 *
 * Copyright (c) 2002, Thomas J. Merritt
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

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
	    printf("lseek failed @%lld\n", pos);

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

	/* must be a back block, see if we can narrow it down */
	printf("Bad 64K block @%lld\n", pos);
	printf("fd %d buf %p rsize %d act %d\n", fd, buf, rsize, act);
	perror("read");

	if (fflag)
	{
	    for (i = 0; i < rsize / 512; i++)
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
