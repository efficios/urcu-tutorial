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
#include <time.h>
#include <inttypes.h>
#include "urcu-game.h"
#include "urcu-game-config.h"
#include "worker-thread.h"

static
long nr_worker_threads = 8;

__thread unsigned int thread_rand_seed;

int verbose, exit_program, clear_screen_enable = 1;

struct live_animals live_animals;
struct vegetation vegetation = {
	.flowers = DEFAULT_VEGETATION_FLOWERS,
	.trees = DEFAULT_VEGETATION_TREES,
	.lock = PTHREAD_MUTEX_INITIALIZER,
};

void show_usage(int argc, char **argv)
{
	printf("Usage: %s <OPTIONS>\n", argv[0]);
	printf("OPTIONS:\n");
        printf("        [-v]             Verbose output.\n");
        printf("        [-c]             Disable clear screen.\n");
        printf("        [-w nr_threads]  Number of worker threads.\n");
	printf("        [-h]             Show this help.\n");
	printf("\n");
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
		case 'c':
			clear_screen_enable = 0;
			break;
		case 'h':
			err = 1;
			goto end;
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

static
void apocalypse(struct cds_lfht *ht)
{
	struct cds_lfht_iter iter;
	struct animal *animal;

	DBG("Apocalypse");
	cds_lfht_for_each_entry(ht, &iter, animal, all_node) {
		DBG("Kill animal %" PRIu64, animal->key);
		kill_animal(animal);
	}
}

int main(int argc, char **argv)
{
	int err;

	rcu_register_thread();

	err = parse_args(argc, argv);
	if (err < 0)
		goto end;
	else if (err > 0) {
		err = 0;
		goto end;
	}

	printf("Welcome to the Island of RCU\n\n");

	printf("Spawning %ld worker threads.\n",
		nr_worker_threads);

	init_game_config();

	live_animals.ht_seed = time(NULL);

	live_animals.all = cds_lfht_new(4096, 1, 0,
		CDS_LFHT_AUTO_RESIZE | CDS_LFHT_ACCOUNTING, NULL);
	if (!live_animals.all)
		abort();
	live_animals.gerbil = cds_lfht_new(4096, 1, 0,
		CDS_LFHT_AUTO_RESIZE | CDS_LFHT_ACCOUNTING, NULL);
	if (!live_animals.gerbil)
		abort();
	live_animals.cat = cds_lfht_new(4096, 1, 0,
		CDS_LFHT_AUTO_RESIZE | CDS_LFHT_ACCOUNTING, NULL);
	if (!live_animals.cat)
		abort();
	live_animals.snake = cds_lfht_new(4096, 1, 0,
		CDS_LFHT_AUTO_RESIZE | CDS_LFHT_ACCOUNTING, NULL);
	if (!live_animals.snake)
		abort();

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

	/*
	 * Kill all animals. After all threads have been joined.
	 */
	apocalypse(live_animals.all);

	err = cds_lfht_destroy(live_animals.snake, NULL);
	if (err)
		goto end;
	err = cds_lfht_destroy(live_animals.cat, NULL);
	if (err)
		goto end;
	err = cds_lfht_destroy(live_animals.gerbil, NULL);
	if (err)
		goto end;
	err = cds_lfht_destroy(live_animals.all, NULL);
	if (err)
		goto end;

	/*
	 * Clean exit: ensure in-flight call_rcu() work is completed.
	 */
	rcu_barrier();

	printf("Goodbye!\n");

end:
	rcu_unregister_thread();
	if (err) {
		fprintf(stderr, "Something went wrong!\n");
		exit(EXIT_FAILURE);
	} else {
		exit(EXIT_SUCCESS);
	}
}
