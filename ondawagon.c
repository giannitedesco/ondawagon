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

#include "ondawagon.h"

static int do_list(void)
{
	dongle_t list;
	size_t i, nmemb;

	if ( !dongle_list_all(&list, &nmemb) )
		return EXIT_FAILURE;

	for(i = 0; i < nmemb; i++) {
	}

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

	for(i = 1; i < argc; i++) {
		if ( !strcmp(argv[i], "--list") ) {
			ret = do_list();
			break;
		}
	}

	return EXIT_SUCCESS;
}
