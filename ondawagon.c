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
		printf("%s\n", dongle_serial(list[i]));
		dongle_close(list[i]);
	}

	free(list);

	return EXIT_SUCCESS;
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
	}

	return ret;
}
