/*
 * urcu-game.c
 *
 * Copyright (C) 2013  Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *
 * THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
 * OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
 *
 * Permission is hereby granted to use or copy this program for any
 * purpose,  provided the above notices are retained on all copies.
 * Permission to modify the code and to distribute modified code is
 * granted, provided the above notices are retained, and a notice that
 * the code was modified is included with the above copyright notice.
 */

#include <stdio.h>
#include <urcu.h>
#include "urcu-game.h"
#include "worker-thread.h"

static
long nr_worker_threads = 8;

int verbose, exit_program;

void show_usage(int argc, char **argv)
{
	printf("Usage: %s <options>\n", argv[0]);
	printf("OPTIONS:\n");
	printf("	[-v] Verbose output.\n");
	printf("	[-w nr_threads] number of worker threads.\n");
}

int parse_args(int argc, char **argv)
{
	int i, err = 0;

	for (i = 1; i < argc; i++) {
		if (argv[i][0] != '-') {
			err = -1;
			goto end;
		}
		switch(argv[i][1]) {
		case 'w':
			if (argc < i + 2) {
				err = -1;
				goto end;
			}
			nr_worker_threads = atol(argv[++i]);
			if (nr_worker_threads <= 0) {
				printf("Please specify a positive and non-zero number of worker threads.\n");
				err = -1;
				goto end;
			}
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			printf("Unrecognised option %s\n", argv[i]);
			err = -1;
			goto end;
		}
	}
end:
	if (err)
		show_usage(argc, argv);
	return err;
}

int main(int argc, char **argv)
{
	int err;

	err = parse_args(argc, argv);
	if (err)
		goto end;

	printf("Welcome to the Island of RCU\n\n");

	printf("Spawning %ld worker threads.\n",
		nr_worker_threads);

	err = create_worker_threads(nr_worker_threads);
	if (err)
		goto end;

	err = create_input_thread();
	if (err)
		goto end;

	err = create_output_thread();
	if (err)
		goto end;

	err = create_dispatch_thread();
	if (err)
		goto end;

	err = join_dispatch_thread();
	if (err)
		goto end;

	err = join_output_thread();
	if (err)
		goto end;

	err = join_input_thread();
	if (err)
		goto end;

	err = join_worker_threads();
	if (err)
		goto end;

	printf("Goodbye!\n");

end:
	if (err) {
		fprintf(stderr, "Something went wrong!\n");
		exit(EXIT_FAILURE);
	} else {
		exit(EXIT_SUCCESS);
	}
}