/*
 * worker-thread.h
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

#include <stdlib.h>
#include <unistd.h>
#include <poll.h>
#include <time.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <urcu.h>
#include "worker-thread.h"
#include "urcu-game.h"
#include "urcu-game-config.h"
#include "ht-hash.h"

static
struct worker_thread *worker_threads;

static
unsigned long nr_worker_threads;

unsigned long get_nr_worker_threads(void)
{
	return nr_worker_threads;
}

static
int do_work(struct urcu_game_work *work)
{
	struct animal *first, *second;

	if (work->exit_thread)
		return 1;

	DBG("do work: key1 %" PRIu64 ", key2 %" PRIu64,
		work->first_key, work->second_key);

	rcu_read_lock();

	first = find_animal(work->first_key);
	second = find_animal(work->second_key);

	/*
	 * If only one of the nodes is non-null, it is the first.
	 */
	if (!first) {
		first = second;
		if (!first)
			goto end;
		/*
		 * Cannot have twice the same animal.
		 */
		if (first == second) {
			second = NULL;
		}
	}

	if (try_birth(first, work->second_key, 0))
		DBG("birth success");
	if (try_eat(first, second))
		DBG("eat success");
	if (try_mate(first, second))
		DBG("mate success");

end:
	rcu_read_unlock();
	return 0;
}

static
void *worker_thread_fct(void *data)
{
	struct worker_thread *wt = data;
	int exit_thread = 0;

	DBG("In worker thread id=%lu.", wt->id);

	rcu_register_thread();

	thread_rand_seed = time(NULL) ^ wt->id;

	while (!exit_thread) {
		struct cds_wfcq_node *node;
		struct urcu_game_work *work;

		node = __cds_wfcq_dequeue_blocking(&wt->q_head, &wt->q_tail);
		if (!node) {
			/* Wait for work */
			poll(NULL, 0, 100);	/* 100ms delay */
			continue;
		}
		work = caa_container_of(node, struct urcu_game_work, q_node);
		exit_thread = do_work(work);
		free(work);
	}

	rcu_unregister_thread();

	DBG("Worker thread id=%lu exiting.", wt->id);
	return NULL;
}

int create_worker_threads(unsigned long nr_threads)
{
	unsigned long i;
	int err;

	worker_threads = calloc(nr_threads, sizeof(*worker_threads));
	if (!worker_threads)
		return -1;
	for (i = 0; i < nr_threads; i++) {
		struct worker_thread *worker;

		worker = &worker_threads[i];
		cds_wfcq_init(&worker->q_head, &worker->q_tail);
		worker->id = i;
		err = pthread_create(&worker->thread_id, NULL,
			worker_thread_fct, worker);
		if (err)
			abort();
	}
	nr_worker_threads = nr_threads;

	return 0;
}

static
void stop_thread(struct worker_thread *thread)
{
	struct urcu_game_work *work;
	int ret;

	work = calloc(1, sizeof(*work));
	if (!work)
		abort();
	work->exit_thread = 1;
	ret = enqueue_work(thread->id, work);
	if (ret)
		abort();
}

void stop_worker_threads(void)
{
	unsigned long i;

	for (i = 0; i < nr_worker_threads; i++) {
		struct worker_thread *worker;

		worker = &worker_threads[i];
		stop_thread(worker);
	}
}

int join_worker_threads(void)
{
	unsigned long i;

	for (i = 0; i < nr_worker_threads; i++) {
		struct worker_thread *worker;
		void *tret;
		int ret;

		worker = &worker_threads[i];
		ret = pthread_join(worker->thread_id, &tret);
		if (ret)
			abort();
	}
	free(worker_threads);
	return 0;
}

int enqueue_work(unsigned long thread_nr, struct urcu_game_work *work)
{
	struct worker_thread *worker;
	bool was_non_empty;

	if (thread_nr >= nr_worker_threads)
		return -1;

	worker = &worker_threads[thread_nr];
	cds_wfcq_node_init(&work->q_node);
	was_non_empty = cds_wfcq_enqueue(&worker->q_head,
			&worker->q_tail, &work->q_node);
	if (!was_non_empty) {
		/*
		 * TODO: currently using polling scheme. Could do a
		 * wakeup scheme with sys_futex instead.
		 */
	}
	return 0;
}
