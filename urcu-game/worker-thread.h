#ifndef WORKER_THREAD_H
#define WORKER_THREAD_H

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

#include <urcu/wfcqueue.h>
#include <urcu/compiler.h>
#include <pthread.h>

struct worker_thread {
	struct cds_wfcq_tail q_tail;	/* new work enqueued at tail */
	struct cds_wfcq_head q_head;	/* extracted from head */
	unsigned long id;
	pthread_t thread_id;

	/*
	 * Align thread structures on cache line size to eliminate
	 * false-sharing.
	 */
} __attribute__((aligned(CAA_CACHE_LINE_SIZE)));

/*
 * Work sent to worker threads.
 */
struct urcu_game_work {
	struct cds_wfcq_node q_node;	/* work queue node */

	uint64_t first_key;
	uint64_t second_key;

	int exit_thread;
	/*
	 * Align work on cache line size to eliminate false-sharing.
	 */
} __attribute__((aligned(CAA_CACHE_LINE_SIZE)));

int create_worker_threads(unsigned long nr_threads);

void stop_worker_threads(void);

int join_worker_threads(void);

int enqueue_work(unsigned long thread_nr, struct urcu_game_work *work);

unsigned long get_nr_worker_threads(void);

#endif /* WORKER_THREAD_H */
