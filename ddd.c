
/*****************************************************************************
 *
 * Copyright (c) 2002-2017, Thomas J. Merritt
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>

#include <sys/time.h>

#define LGBLKSIZE      (64*1024)
#define FSBLKSIZE      4096
#define DSKBLKSIZE      512

int errs = 0;
int badsectors = 0;
off_t skipped = 0;
int fflag = 0;
int zflag = 0;

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

int
writebuf(int outfd, off_t *off, off_t *skipped, unsigned char *buf, int len)
{
    if (outfd < 0 || off == NULL)
        return 0;

    if (zflag)
        return write(outfd, buf, len);

//    printf("off %lld, len %d\n", (long long)*off, len);

    off_t x = *off;
    int cnt = 0;

    // align to multiple of block size
    if ((x % FSBLKSIZE) != 0)
    {
        int l = FSBLKSIZE - (x % FSBLKSIZE);
        printf("initial fragment, off %lld, len %d\n", x, l);

        if (l > len)
            l = len;

        int n = write(outfd, buf, l);

        if (n != l)
        {
            printf("write error: off %lld, len %d, ret %d\n", x, l, n);
            return n;
        }

        buf += n;
        x += n;
        cnt += n;
        len -= n;
    }

    int l = len;

    while (l >= FSBLKSIZE)
    {
        int i;

        for (i = 0; i < l; i++)
            if (buf[i] != 0)
                break;

        if (i >= FSBLKSIZE)
        {
            int blks = i / FSBLKSIZE;
            int n = blks * FSBLKSIZE;
//            printf("Skipping %d blocks, %d bytes\n", blks, n);

            // skip writing the zero blocks
            buf += n;
            x += n;
            cnt += n;
            l -= n;
            *skipped += n;

            off_t r = lseek(outfd, x, SEEK_SET);

            if (r < 0)
            {
                printf("seek error: off %lld, ret %lld\n", x, r);
                *off = x;
                return cnt > 0 ? cnt : -1;
            }

            continue;
        }

        // find the next zero block
        for (i = 0; i + FSBLKSIZE <= l; i += FSBLKSIZE)
        {
            int j = 0;

            for (j = 0; j < FSBLKSIZE; j++)
                if (buf[i + j] != 0)
                    break;

            if (j == FSBLKSIZE)
                break;
        }

        if (i > 0)
        {
            int n = write(outfd, buf, i);

            if (n != i)
            {
                printf("write error: off %lld, len %d, ret %d\n", x, i, n);

                if (n > 0)
                {
                    x += n;
                    cnt += n;
                }

                *off = x;
                return cnt > 0 ? cnt : n;
            }

            buf += n;
            x += n;
            cnt += n;
            l -= n;
        }
    }

    if (l > 0)
    {
        printf("tail fragment, off %lld, len %d\n", x, l);

        // l is less than a full block
        int n = write(outfd, buf, l);

        if (n != l)
            printf("write error: off %lld, len %d, ret %d\n", x, l, n);

        if (n > 0)
        {
            x += n;
            cnt += n;
        }

        *off = x;
        return cnt > 0 ? cnt : n;
    }

    *off = x;
    return cnt;
}

int
ddd(int fd, int outfd, off_t pos, off_t size)
{
    off_t outoff = 0;
    off_t lastdot = 0;
    unsigned char buf[LGBLKSIZE];
    int dots = pos / (1024*1024);
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
	    printf("lseek failed @%lld\n", (long long)pos);

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

	    if (writebuf(outfd, &outoff, &skipped, buf, act) != act)
		perror("write");

	    continue;
	}

	/* must be a bad block, see if we can narrow it down */
	printf("Bad %dK block @%lld\n", LGBLKSIZE / 1024, (long long)pos);
	printf("fd %d buf %p rsize %d act %d\n", fd, buf, rsize, act);
	perror("read");

	if (fflag)
	{
	    for (i = 0; i < rsize / DSKBLKSIZE; i++)
	    {
		if (lseek(fd, pos, SEEK_SET) < 0)
		    printf("lseek failed @%lld\n", (long long)pos);

		act = read(fd, buf, DSKBLKSIZE);

		if (act == 0)
		    break;

		if (act > 0)
		{
		    if (writebuf(outfd, &outoff, &skipped, buf, act) != act)
			perror("write");

		    pos += act;
		    continue;
		}

		/* we've narrowed down a bad block, report it */
		printf("Bad %d block @%lld\n", DSKBLKSIZE, (long long)pos);

		if (outfd >= 0)
		{
		    /*  write zeros into the output file in its place */
		    memset(buf, 0, DSKBLKSIZE);

		    if (writebuf(outfd, &outoff, &skipped, buf, DSKBLKSIZE) != DSKBLKSIZE)
			perror("write");
		}

		/* now skip it */
		pos += DSKBLKSIZE;
		errs++;
		badsectors++;
	    }
	}
	else
	{
	    if (outfd >= 0)
	    {
		/*  write zeros into the output file in its place */
		memset(buf, 0, rsize);

		if (writebuf(outfd, &outoff, &skipped, buf, rsize) != rsize)
		    perror("write");
	    }

	    pos += rsize;
	    errs++;
	}

	size -= rsize;
    }

    printf("\n");

    if ((dots % 50) != 0)
	printtime(dots);

    printf("\n");
    return 0;
}

void
usage()
{
    fprintf(stderr, "Usage: ddd [-f] [-o output] raw_device_file [start [size]]\n");
    exit(2);
}

int
main(int argc, char **argv)
{
    int fd;
    int outfd = -1;
    char *outfile = NULL;
    off_t pos = 0;
    off_t size = -1;
    char *rdev = NULL;
    int ch;

    while ((ch = getopt(argc, argv, "fo:")) != EOF)
        switch (ch)
        {
        case 'f':
            fflag = 1;
            break;
        case 'o':
            outfile = optarg;
            break;
        case 'z':
            zflag = 1;
            break;
        default:
            usage();
        }

    if (outfile != NULL)
    {
	outfd = open(outfile, O_WRONLY|O_CREAT, 0600);

	if (outfd < 0)
	{
	    perror(outfile);
	    exit(1);
	}
    }

    if (argc - optind < 1)
	usage();

    rdev = argv[optind];

    if (argc - optind > 1)
    {
        pos = getofft(argv[optind + 1]);

        if (argc - optind > 2)
            size = getofft(argv[optind + 2]);
    }

    fd = open(rdev, O_RDONLY);

    if (fd < 0)
	perror(rdev);

    ddd(fd, outfd, pos, size);

    close(fd);

    if (outfd >= 0)
	close(outfd);

    if (badsectors > 0)
	printf("Bad sectors: %d\n", badsectors);

    if (skipped > 0)
        printf("Skipped %lldGB\n", skipped / (1024LL * 1024 * 1024));

    exit(errs ? 1 : 0);
}

