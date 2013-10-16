/*
 * dispatch-thread.c
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
#include <urcu/system.h>
#include <urcu.h>
#include "urcu-game.h"
#include "urcu-game-config.h"
#include "worker-thread.h"

static
pthread_t dispatch_thread_id;

static
__thread unsigned int thread_rand_seed;

static
void do_dispatch(void)
{
	struct urcu_game_config *config;
	unsigned long i, nr_threads;
	uint64_t island_size;
	int ret;

	rcu_read_lock();
	config = urcu_game_config_get();
	island_size = config->island_size;
	rcu_read_unlock();

	nr_threads = get_nr_worker_threads();
	for (i = 0; i < nr_threads; i++) {
		struct urcu_game_work *work;

		work = calloc(1, sizeof(*work));
		if (!work)
			abort();
		work->first_key = rand_r(&thread_rand_seed) % island_size;
		work->second_key = rand_r(&thread_rand_seed) % island_size;
		ret = enqueue_work(i, work);
		if (ret)
			abort();
	}
}

static
void *dispatch_thread_fct(void *data)
{
	DBG("In user dispatch thread.");

	thread_rand_seed = time(NULL);

	/* Read keys typed by the user */
	while (!CMM_LOAD_SHARED(exit_program)) {
		DBG("Dispatch.");
		do_dispatch();
		sleep(1);	/* TODO */
	}

	/* Send worker thread stop message */
	stop_worker_threads();

	DBG("User dispatch thread exiting.");
	return NULL;
}

int create_dispatch_thread(void)
{
	int err;

	err = pthread_create(&dispatch_thread_id, NULL,
		dispatch_thread_fct, NULL);
	if (err)
		abort();
	return 0;
}

int join_dispatch_thread(void)
{
	int ret;
	void *tret;

	ret = pthread_join(dispatch_thread_id, &tret);
	if (ret)
		abort();
	return 0;
}
