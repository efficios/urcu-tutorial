/*
 * print-output.c
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

#include <unistd.h>
#include <stdio.h>
#include <inttypes.h>
#include <urcu/system.h>
#include <urcu.h>
#include <urcu/rculfhash.h>
#include "urcu-game.h"
#include "urcu-game-config.h"

int hide_output;
/* Protect output to screen */
pthread_mutex_t print_output_mutex = PTHREAD_MUTEX_INITIALIZER;

static
pthread_t output_thread_id;

static
void do_print_output(void)
{
	struct animal *animal;
	struct cds_lfht_iter iter;
	uint64_t count;
	struct urcu_game_config *config;

	rcu_read_lock();

	config = urcu_game_config_get();

	printf("\n");
	printf("---------------- RCU Island Summary ------------------\n");
	printf("Island size: %" PRIu64 "\n", config->island_size);
	count = 0;
	cds_lfht_for_each_entry(live_animals.gerbil, &iter,
			animal, kind_node)
		count++;
	printf("Number of gerbils: %" PRIu64 "\n", count);

	count = 0;
	cds_lfht_for_each_entry(live_animals.cat, &iter,
			animal, kind_node)
		count++;
	printf("Number of cats: %" PRIu64 "\n", count);

	count = 0;
	cds_lfht_for_each_entry(live_animals.snake, &iter,
			animal, kind_node)
		count++;
	printf("Number of snakes: %" PRIu64 "\n", count);

	pthread_mutex_lock(&vegetation.lock);
	printf("Flowers: %" PRIu64 "\n", vegetation.flowers);
	printf("Trees: %" PRIu64 "\n", vegetation.trees);
	pthread_mutex_unlock(&vegetation.lock);
	printf("-------- (type 'm' for menu, 'q' to quit game) -------\n");

	rcu_read_unlock();
}

static
void *output_thread_fct(void *data)
{
	DBG("In user output thread.");
	rcu_register_thread();

	/* Read keys typed by the user */
	while (!CMM_LOAD_SHARED(exit_program)) {
		if (!CMM_LOAD_SHARED(hide_output)) {
			pthread_mutex_lock(&print_output_mutex);
			DBG("Refresh screen.");

			do_print_output();

			fflush(stdout);
			pthread_mutex_unlock(&print_output_mutex);
		}
		sleep(URCU_GAME_REFRESH_PERIOD);
	}

	rcu_unregister_thread();
	DBG("User output thread exiting.");
	return NULL;
}

int create_output_thread(void)
{
	int err;

	err = pthread_create(&output_thread_id, NULL,
		output_thread_fct, NULL);
	if (err)
		abort();
	return 0;
}

int join_output_thread(void)
{
	int ret;
	void *tret;

	ret = pthread_join(output_thread_id, &tret);
	if (ret)
		abort();
	return 0;
}
