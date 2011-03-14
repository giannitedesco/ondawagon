/*
 * This file is part of ondawagon
 * Copyright (c) 2011 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/

#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "ondawagon.h"

const char *system_err(void)
{
	return strerror(errno);
}

static int do_list(void)
{
	dongle_t *list;
	size_t i, nmemb;

	if ( !dongle_list_all(&list, &nmemb) ) {
		fprintf(stderr, "%s: there were some errors\n", odw_cmd);
	}

	for(i = 0; i < nmemb; i++) {
		printf("%s: %s: %s\n",
			dongle_serial(list[i]),
			dongle_needs_ready(list[i]) ? "ZEROCD" : "READY",
			dongle_label(list[i]));
		dongle_close(list[i]);
	}

	free(list);

	return EXIT_SUCCESS;
}

static int do_ready(const char *ser)
{
	dongle_t d;

	d = dongle_open(ser);
	if ( NULL == d ) {
		fprintf(stderr, "%s: dongle: %s: not found\n", odw_cmd, ser);
		return EXIT_FAILURE;
	}

	if ( !dongle_ready(d) ) {
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

static void usage(FILE *f)
{
	fprintf(f, "%s: ONDA 3G dongle driver\n", odw_cmd);
	fprintf(f, "\n");
	fprintf(f, "Copyright (c) 2011 Gianni Tedesco\n");
	fprintf(f, "This program is free software, "
		"distributed under the terms of the GNU GPL v3\n");
	fprintf(f, "Dear goddess we wrote this program just for you, "
		"as an offering.\n");
	fprintf(f, "Can you hear us now?\n");
	fprintf(f, "\n");
	fprintf(f, "Usage:\n");
	fprintf(f, " --list             List all dongles\n");
	fprintf(f, " --ready <serial>   Switch dongle in to 3G mode\n");
	fprintf(f, " --help, -h         Display this massage\n");
	fprintf(f, "\n");
}

const char *odw_cmd;
int main(int argc, char **argv)
{
	int i, ret;

	if ( argc < 1 ) {
		fprintf(stderr, "ondawagon: Couldn't determine command name\n");
		return EXIT_FAILURE;
	}

	odw_cmd = argv[0];

	for(ret = EXIT_FAILURE, i = 1; i < argc; i++) {
		if ( !strcmp(argv[i], "--list") ) {
			ret = do_list();
			break;
		}
		if ( !strcmp(argv[i], "--ready") && i + 1 < argc ) {
			const char *ser = argv[i + 1];
			ret = do_ready(ser);
			break;
		}
		if ( !strcmp(argv[i], "--help") ||
			!strcmp(argv[i], "-h") ) {
			usage(stdout);
			ret = EXIT_SUCCESS;
			break;
		}
		fprintf(stderr, "%s: Unrecognized option: %s\n",
			odw_cmd, argv[i]);
		usage(stderr);
		ret = EXIT_FAILURE;
		break;
	}

	return ret;
}
