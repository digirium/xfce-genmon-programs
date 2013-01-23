/*
 * meminfo.c - Memory information monitor for XFCE genmon plugin.
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
static char *prog = "meminfo";
static char *vers = "1.0.0";

#include <assert.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Option parsing */
static char iconfile[256];
static int debug = 0;
static int showbar = 0;
static int showicon = 1;

static void
show_version (void)
{
	printf ("%s %s - (C) 2013 Digirium, see <https://github.com/Digirium>\n", prog, vers);
	printf ("Released under the GNU GPL.\n\n");
}

static void
show_help (void)
{
	show_version ();
	
	printf ("-d --debug		Display debugging output.\n");
	printf ("-h --help		Display this help.\n");
	printf ("-i[FILE] --icon[=FILE]	Set the icon filename, or disable the icon.\n");
	printf ("-p --percentbar		Display the percent bar.\n");
	printf ("-v --version		Display version information.\n");

	printf ("\nLong options may be passed with a single dash.\n\n");
}

static void
get_options (int argc, char *argv[])
{
	char *home = getenv ("HOME");
	assert (home != NULL);

	sprintf (iconfile, "%s/.genmon-icon/%s.png", home, prog);

	if (argc == 1) return;

	static struct option long_opts[] =
	{
		{ "debug",	no_argument,		0, 'd' },
		{ "help",	no_argument,		0, 'h' },
		{ "icon",	optional_argument,	0, 'i' },
		{ "percentbar",	no_argument,		0, 'p' },
		{ "version",	no_argument,		0, 'v' },
		{ 0,0,0,0 }
	};

	int opt, opti;

	while ((opt = getopt_long (argc, argv, "dhi::pv", long_opts, &opti)))
	{
		if (opt == EOF) break;

		switch (opt)
		{
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

			if (*optarg == '/')
				strcpy (iconfile, optarg);
			else
				sprintf (iconfile, "%s/.genmon-icon/%s", home, optarg);

			break;

		case 'p':
			showbar = 1;
			break;

		case 'v':
			show_version ();
			exit (0);

		default:
			exit (1);
		}
	}
}

static int
get_fw (unsigned int val, int fw)
{
	int myfw; /* Field width */

	if	(val > 9999)	myfw = 5;
	else if (val > 999)	myfw = 4;
	else if (val > 99)	myfw = 3;
	else			myfw = 2;
	
	return (myfw > fw) ? myfw : fw;
}

int
main (int argc, char *argv[])
{
	get_options (argc, argv);

	unsigned long long int memtotal = 0;
	unsigned long long int memfree = 0;
	unsigned long long int membuffers = 0;
	unsigned long long int memcached = 0;
	unsigned long long int memused;
	int k = 1024, ret, found = 0;
	char buffer[128];

	/* Obtain memory usage values. This is done by parsing memory usage statistics from
	 * the psuedo-filesystem and just four lines from the output are used.
	 */
	FILE *file = fopen ("/proc/meminfo", "r");
	assert (file != NULL);

	while (fgets (buffer, 128, file))
	{
		switch (buffer[0])
		{
		case 'M':
			/* MemTotal */
			if (strstr (buffer, "MemTotal:") == buffer)
			{
				ret = sscanf (buffer, "MemTotal: %llu kB", &memtotal);
				assert (ret == 1);
				found++;
			}

			/* MemFree */
			else if (strstr (buffer, "MemFree:") == buffer)
			{
				ret = sscanf (buffer, "MemFree: %llu kB", &memfree);
				assert (ret == 1);
				found++;
			}

			break;

		case 'B':
			/* Buffers */
			if (strstr (buffer, "Buffers:") == buffer)
			{
				ret = sscanf (buffer, "Buffers: %llu kB", &membuffers);
				assert (ret == 1);
				found++;
			}

			break;

		case 'C':
			/* Cached */
			if (strstr (buffer, "Cached:") == buffer)
			{
				ret = sscanf (buffer, "Cached: %llu kB", &memcached);
				assert (ret == 1);
				found++;
			}

			break;
		}

		/* The output may begin with the four values that need to be found. In any
		 * case, when the 4th value is found quit the parsing loop.
		 */
		if (found == 4)
			break;
	}

	/* Process memory usage */
	memused = memtotal - memfree - membuffers - memcached;
	fclose (file);

	/** XFCE GENMON XML **/

	/* Icon */
	if (showicon) printf ("<img>%s</img>\n", iconfile);

	/* Pseudo-filesystem gave us values in KB, convert to MB */
	unsigned long long int kused	= memused / k;
	unsigned long long int kcached	= memcached / k;
	unsigned long long int kbuffer	= membuffers / k;
	unsigned int percent		= (memused * 100) / memtotal;

	/* Text */
	int fw = get_fw(kused, get_fw(kcached, 1));
	printf ("<txt>%*lluM %d%%\n%*lluM %lluM</txt>\n", fw, kused, percent, fw, kcached, kbuffer);

	/* Tool tip */
	percent = ((memtotal - memfree) * 100) / memtotal;
	printf ("<tool>Total memory available: %lluM\n", memtotal / k);
	printf ("Memory currently being used: %lluM (%d%%)</tool>\n", (memtotal - memfree) / k, percent);

	/* Percentage bar */
	if (showbar) printf ("<bar>%d</bar>\n", percent);

	return 0;
}
