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
#include "urcu-game.h"
#include "worker-thread.h"

static
pthread_t dispatch_thread_id;

static
void *dispatch_thread_fct(void *data)
{
	DBG("In user dispatch thread.");

	/* Read keys typed by the user */
	while (!CMM_LOAD_SHARED(exit_program)) {
		DBG("Dispatch.");
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
