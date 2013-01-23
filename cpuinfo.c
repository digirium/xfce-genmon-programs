/*
 * cpuinfo.c - CPU information monitor for XFCE genmon plugin.
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
static char *prog = "cpuinfo";
static char *vers = "1.0.0";

#include <assert.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Option parsing */
static int cpuusage = 0;
static int debug = 0;
static char iconfile[256];
static int showfarenheit = 0;
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
	
	printf ("-c --cpuusage		Display CPU usage.\n");
	printf ("-d --debug		Display debugging output.\n");
	printf ("-F --farenheit		Display temperature in farenheit.\n");
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

	if (argc == 1) return;

	static struct option long_opts[] =
	{
		{ "cpuusage",	no_argument,		0, 'c' },
		{ "debug",	no_argument,		0, 'd' },
		{ "farenheit",	no_argument,		0, 'F' },
		{ "help",	no_argument,		0, 'h' },
		{ "icon",	optional_argument,	0, 'i' },
		{ "version",	no_argument,		0, 'v' },
		{ 0,0,0,0 }
	};

	int opt, opti;

	while ((opt = getopt_long (argc, argv, "cdfhi::v", long_opts, &opti)))
	{
		if (opt == EOF) break;

		switch (opt)
		{
		case 'c':
			cpuusage = 1;
			break;

		case 'd':
			debug = 1;
			break;

		case 'F':
			showfarenheit = 1;
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

		case 'v':
			show_version ();
			exit (0);

		default:
			exit (1);
		}
	}
}

static char *
p2s (int percent) /* Percent to string */
{
	char *buffer = (char *)malloc (16);

	if (percent < 100)	sprintf (buffer, "%2d%%", percent);
	else			strcpy (buffer, "100");

	return buffer;
}

int
main (int argc, char *argv[])
{
	get_options (argc, argv);

	/* Monitor was written for a CPU containing four cores and makes
	 * assumptions about how to get usage statistics, temperature and
	 * cpu PWM fan speed. May require editing to get working.
	 */
	int cpus = sysconf (_SC_NPROCESSORS_CONF);
	assert (cpus == 4);

	/* Allocate some storage for the previous total and idle values.
	 */
	int size = sizeof (unsigned long long int) * cpus * 2;
	unsigned long long int *prev = (unsigned long long int *)malloc (size);
	(void)memset (prev, 0, size);

	/* If cache file exists, read the previous total and idle usage statistics
	 * and then read the previous maximum temperature and fan speed.
	 */
	char format[256], buffer[256];
	sprintf (format, "/dev/shm/cpuinfo.%d", getuid ());
	FILE *shm = fopen (format, "r");
	int maxrpm = 0, n, ret;
	float maxtemp = 0.0;

	if (shm)
	{
		for (n = 0; n < cpus; n++)
		{
			fgets (buffer, 256, shm);
			ret = sscanf (buffer, "%llu %llu", prev + 2*n, prev + 2*n + 1);
			assert (ret == 2);
		}

		fgets (buffer, 256, shm);
		ret = sscanf (buffer, "%f %d", &maxtemp, &maxrpm);
		assert (ret == 2);
		fclose (shm);
	}

	/* Prepare to write a new cache. The total and idle statistics are always
	 * increasing so the cache is updated every run.
	 */
	shm = fopen (format, "w");
	assert (shm != NULL);

	enum _PROC { User = 0, Nice, System, Idle, IO, Irq, Soft, Steal, Guest };
	int *percent = (int *)malloc (sizeof (int) * cpus);
	unsigned long long int proc[10];

	/* Prepare to read CPU statistics from the pseudo-filesystem. With the values
	 * saved in the cache, the interval values for total and idle can be calculated
	 * and the CPU usage obtained.
	 */
	FILE *file = fopen ("/proc/stat", "r");
	int i, id = -1;

	for (n = 0; n <= cpus; n++)
	{
		unsigned long long int total = 0;

		fgets (buffer, 256, file);

		if (n == 0) continue;

		ret = sscanf (buffer, "cpu%d %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
			&id, proc + 0, proc + 1, proc + 2, proc + 3, proc + 4,
			proc + 5, proc + 6, proc + 7, proc + 8, proc + 9);

		assert (ret == 11);

		/* User + Nice + System + Idle */
		for (i = 0; i < 4; i++) total += proc[i];

		int cpu = n - 1;
		unsigned long long int intervaltotal = total - *(prev + 2*cpu);
		unsigned long long int intervalidle  = proc[Idle] - *(prev + 2*cpu + 1);

		*(percent + cpu) = (int)(100.0 * ((float)(intervaltotal - intervalidle)
			/ (float)intervaltotal));

		fprintf (shm, "%llu %llu\n", total, proc[Idle]);
	}
	fclose (file);

	/* Prepare to get the CPU temperature and PWM fan speed. Going to open
	 * a read pipe to the sensors program to get these values.
	 */
	file = popen ("/usr/bin/sensors", "r");
	assert (file != NULL);

	float temp = 0.0;
	int rpm = 0;

	while (fgets (buffer, 256, file))
	{
		switch (buffer[0])
		{
		/* CPU temperature */
		case 't':
			if (strstr (buffer, "temp1:") == buffer)
			{
				ret = sscanf (buffer, "temp1: +%f", &temp);
				assert (ret == 1);
			}

			break;
				
		/* CPU Fan Speed */
		case 'C':
			if (strstr (buffer, "CPU Fan Speed:") == buffer)
			{
				ret = sscanf (buffer, "CPU Fan Speed: %d RPM", &rpm);
				assert (ret == 1);
			}

			break;
		}
	}
	pclose (file);

	/* Write the maximum values seen so far for temperatue and rpm to the
	 * end of the cache.
	 */
	if (temp > maxtemp)	maxtemp = temp;
	if (rpm > maxrpm)	maxrpm = rpm;

	fprintf (shm, "%.1f %d\n", maxtemp, maxrpm);
	fclose (shm);

	/* Recalculate temperatures as farenheit */
	char CF = 'C';
	if (showfarenheit)
	{
		CF = 'F';
		temp = (temp * 1.8) + 32.0;
		maxtemp = (maxtemp * 1.8) + 32.0;
	}


	/** XFCE GENMON XML **/

	/* Icon */
	if (showicon) printf ("<img>%s</img>\n", iconfile);

	/* Text */
	char line1[128], line2[128], tempbuf[32], rpmbuf[32];
	sprintf (tempbuf, "%3.1f°%c", temp, CF);
	sprintf (rpmbuf, "%-4drpm", rpm);

	if (cpuusage)
	{
		sprintf (line1, "%8s %s %s", tempbuf, p2s(*(percent + 0)), p2s(*(percent + 1)));
		sprintf (line2, "%7s %s %s", rpmbuf, p2s(*(percent + 2)), p2s(*(percent + 3)));
	}

	if (cpuusage)	printf ("<txt>%s\n%s</txt>\n", line1, line2);
	else		printf ("<txt>%8s\n%7s</txt>\n", tempbuf, rpmbuf);
		
	/* Tool tip */
	printf ("<tool>Maximum temperature observed: %.1f°%c\n", maxtemp, CF);
	printf ("Maximum RPM observed: %drpm</tool>/n", maxrpm);

	return 0;
}
