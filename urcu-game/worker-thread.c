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
#include "worker-thread.h"
#include "urcu-game.h"

static
struct worker_thread *worker_threads;

static
long nr_worker_threads;

static
int do_work(struct urcu_game_work *work)
{
	if (work->exit_thread)
		return 1;

	/* TODO */


	return 0;
}

static
void *worker_thread_fct(void *data)
{
	struct worker_thread *wt = data;
	int exit_thread = 0;

	DBG("In worker thread id=%lu.", wt->id);

	while (!exit_thread) {
		struct cds_wfcq_node *node;
		struct urcu_game_work *work;

		node = __cds_wfcq_dequeue_blocking(&wt->q_head, &wt->q_tail);
		if (!node) {
			/* Wait for work */
			DBG("Thread id=%lu waiting for work.", wt->id);
			sleep(1);	/* TODO */
			continue;
		}
		work = caa_container_of(node, struct urcu_game_work, q_node);
		exit_thread = do_work(work);
		free(work);
	}

	DBG("Worker thread id=%lu exiting.", wt->id);
	return NULL;
}

int create_worker_threads(long nr_threads)
{
	long i;
	int err;

	if (nr_threads < 0)
		return -1;

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
	long i;

	for (i = 0; i < nr_worker_threads; i++) {
		struct worker_thread *worker;

		worker = &worker_threads[i];
		stop_thread(worker);
	}
}

int join_worker_threads(void)
{
	long i;

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
	was_non_empty = cds_wfcq_enqueue(&worker->q_head,
			&worker->q_tail, &work->q_node);
	if (!was_non_empty) {
		/* TODO: wakeup */
	}
	return 0;
}
