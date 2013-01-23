/*
 * netinfo.c - Network information monitor for XFCE genmon plugin.
 * Copyright (C) 2013 Digirium, see <https://github.com/Digirium/>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
static char *prog = "netinfo";
static char *vers = "1.0.0";

#include <assert.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

/* Option parsing */
static int debug = 0;
static char iconfile[256];
static char *interface = NULL;
static int showbps = 0;
static int showicon = 1;

static void
show_version (void)
{
	printf ("%s %s - (C) 2013 Digirium, see <https://github.com/Digirium>\n", prog, vers);
	printf ("Released under the GNU GPL.\n\n");
}

static void
show_usage (void)
{
	printf ("Usage: %s [options] <networkinterface>\n", prog);
}

static void
show_help (void)
{
	show_version ();
	show_usage ();
	
	printf ("\n-b --bitspersec		Display rates in bits/second.\n");
	printf ("-d --debug		Display debugging output.\n");
	printf ("-h --help		Display this help.\n");
	printf ("-i[FILE] --icon[=FILE]	Set the icon filename, or disable the icon.\n");
	printf ("-v --version		Display version information.\n");

	printf ("\nLong options may be passed with a single dash.\n\n");
}

static void
get_options (int argc, char *argv[])
{
	char *home = getenv ("HOME");
	assert (home != NULL);

	sprintf (iconfile, "%s/.genmon-icon/%s.png", home, prog);

	static struct option long_opts[] =
	{
		{ "bitspersec",	no_argument,		0, 'b' },
		{ "debug",	no_argument,		0, 'd' },
		{ "help",	no_argument,		0, 'h' },
		{ "icon",	optional_argument,	0, 'i' },
		{ "version",	no_argument,		0, 'v' },
		{ 0,0,0,0 }
	};

	int opt, opti;

	while ((opt = getopt_long (argc, argv, "bdhi::v", long_opts, &opti)))
	{
		if (opt == EOF) break;

		switch (opt)
		{
		case 'b':
			showbps = 1;
			break;

		case 'd':
			debug = 1;
			break;

		case 'h':
			show_help ();
			exit (0);

		case 'i':
			if (!optarg)
			{
				showicon = 0;
				break;
			}

			if (*optarg == '/')	strcpy (iconfile, optarg);
			else			sprintf (iconfile, "%s/.genmon-icon/%s", home, optarg);

			break;

		case 'v':
			show_version ();
			exit (0);

		default:
			exit (1);
		}
	}

	if (optind >= argc)
	{
		show_usage ();
		exit (1);
	}

	interface = argv[optind];
}

enum RXTX2S { RX = 1, TX = 0 };

static void
rxtx2s (char *line, float rate, unsigned long long int total, int rxflag)
{
	static char *Rx = "Rx", *Tx = "Tx";

	if (rate == 0.0)	/* Show totals up/down in units of 1,048,576 bytes */
	{
		float gb = total / 1073741824.0;
		sprintf (line, "%6.3fG", gb);
	}
	else if (showbps)	/* Show rates in decimal units of 1000 or 1000000 bits/second */
	{
		if (rate < 1000.0)	sprintf (line, "%s %3dk", rxflag ? Rx : Tx, (int)rate);
		else			sprintf (line, "%6.3fm", rate / 1000.0);
	}
	else			/* Show rates in binary units of 1024 bytes/second */
	{
		if (rate < 1000.0)	sprintf (line, "%s %3dK", rxflag ? Rx : Tx, (int)rate);
		else			sprintf (line, "%6dK", (int)rate);
	}
}

int
main (int argc, char *argv[])
{
	get_options (argc, argv);

	/* Obtain network statistics for the interface that was specified. These
	 * will be compared with previous statistics kept in the cache and used
	 * to work out network speeds.
	 */
	enum RXTX { Bytes = 0, Packets, Errs, Drop, Fifo, Frame, Compressed, Multicast };
	unsigned long long int rx[8], tx[8];

	FILE *file = fopen ("/proc/net/dev", "r");
	assert (file != NULL);

	char buffer[1024], format[1024];
	int loop, ret, down = 1;

	for (loop = 0; loop < 8; loop++) rx[loop] = tx[loop] = 0;

	if (file)
	{
		sprintf (format, " %5s:", interface);

		for (loop = 0; loop < 16; loop++) strcat (format, " %llu");

		while (fgets (buffer, 1024, file))
			if (strstr (buffer, interface))
			{
				ret = sscanf (buffer, format,
					rx, rx + 1, rx + 2, rx + 3, rx + 4, rx + 5, rx + 6, rx + 7,
					tx, tx + 1, tx + 2, tx + 3, tx + 4, tx + 5, tx + 6, tx + 7);

				assert (ret == 16);
				down = 0;
				break;
			}

		fclose (file);
	}

	/* If the pseudo-file did not mention the interface, indicate that the
	 * network connection is down in the generic monitor and quit.
	 */
	if (down)
	{
		if (showicon) printf ("<img>%s</img>\n", iconfile);
		printf ("<txt>   Down\n</txt>\n");
		printf ("<tool>%s is down</tool>", interface);
		unlink (format);
		return 3;
	}

	/* Need to know elapsed time to work out data rates. Find current time and
	 * get the previous time from the cache.
	 */
	struct timespec ts;
	clock_gettime (CLOCK_MONOTONIC_RAW, &ts);
	unsigned long long int nanos = ts.tv_sec * 1000000000LL + ts.tv_nsec;
	unsigned long long int prevrx, prevtx, prevnanos;
	float raterx = 0.0, ratetx = 0.0;

	/* Prepare to read and update the cache. It contains three previous values,
	 * bytes read and written and the time.
	 */
	sprintf (format, "/dev/shm/netinfo.%s.%d", interface, getuid ());

	if ((file = fopen (format, "r+")))
	{
		if (fgets (buffer, 1024, file))
		{
			ret = sscanf (buffer, "%llu %llu %llu", &prevrx, &prevtx, &prevnanos);
			assert (ret == 3);

			if (showbps)
			{
				/* Calculate kilobits per second */
				float elapsed = (nanos - prevnanos) / 1000000.0;
				raterx = ((rx[Bytes] - prevrx) * 8.0) / elapsed;
				ratetx = ((tx[Bytes] - prevtx) * 8.0) / elapsed;
			}
			else
			{
				/* Calculate kilobytes per second */
				float elapsed = (nanos - prevnanos) / 1000000000.0;
				raterx = (((float)(rx[Bytes] - prevrx)) / elapsed) / 1024.0;
				ratetx = (((float)(tx[Bytes] - prevtx)) / elapsed) / 1024.0;
			}
		}

		fseek (file, 0, SEEK_SET);
	}
	else	file = fopen (format, "w");

	/* Update the cache */
	fprintf (file, "%llu %llu %llu\n", rx[Bytes], tx[Bytes], nanos);
	fclose (file);

	/** XFCE GENMON XML **/

	/* Icon */

	printf ("<img>%s</img>\n", iconfile);

	/* If NIC is inactive, or close to inactive, show totals instead */
	if (raterx < 1.0 && ratetx < 1.0) raterx = ratetx = 0.0;

	char in[64], out[64];

	/* Text */

	rxtx2s (in,  raterx, rx[Bytes], RX);
	rxtx2s (out, ratetx, tx[Bytes], TX);

	printf ("<txt>%s\n%s</txt>\n", in, out);

	/* Tool tip */

	rxtx2s (in,  0.0, rx[Bytes], RX);
	rxtx2s (out, 0.0, tx[Bytes], TX);

	printf ("<tool>Network interface: %s\n", interface);
	printf ("Total data received: %s\n", in);
	printf ("Total data sent: %s</tool>\n", out);

	return 0;
}
