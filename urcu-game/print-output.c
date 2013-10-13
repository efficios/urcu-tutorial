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
#include <urcu/system.h>
#include "urcu-game.h"

int hide_output;
/* Protect output to screen */
pthread_mutex_t print_output_mutex = PTHREAD_MUTEX_INITIALIZER;

static
pthread_t output_thread_id;

static
void *output_thread_fct(void *data)
{
	DBG("In user output thread.");

	/* Read keys typed by the user */
	while (!CMM_LOAD_SHARED(exit_program)) {
		if (!CMM_LOAD_SHARED(hide_output)) {
			pthread_mutex_lock(&print_output_mutex);
			DBG("Refresh screen.");
			/* TODO */

			fflush(stdout);
			pthread_mutex_unlock(&print_output_mutex);
		}
		sleep(URCU_GAME_REFRESH_PERIOD);
	}

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
