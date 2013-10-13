/*
 * urcu-game-config.c
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

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <urcu.h>
#include <urcu/compiler.h>

#include "urcu-game-config.h"

static
struct urcu_game_config *current_config;

static
void config_free(struct rcu_head *head)
{
	struct urcu_game_config *config;

	config = caa_container_of(head, struct urcu_game_config, rcu_head);
	free(config);
}

struct urcu_game_config *urcu_game_config_get(void)
{
	assert(rcu_read_ongoing());

	return rcu_dereference(current_config);
}

int urcu_game_config_set(const struct urcu_game_config *config)
{
	struct urcu_game_config *old_config, *new_config;

	new_config = malloc(sizeof(*new_config));
	if (!new_config) {
		return -1;
	}
	memcpy(new_config, config, sizeof(*new_config));
	old_config = rcu_xchg_pointer(&current_config, new_config);
	if (old_config)
		call_rcu(&old_config->rcu_head, config_free);
	return 0;
}
