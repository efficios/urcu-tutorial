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

/*
 * Game configuration is protected against concurrent updates using a
 * mutex. It is read by many concurrent threads, using RCU to
 * synchronize.
 */
static
struct urcu_game_config *current_config;

static
pthread_mutex_t config_mutex = PTHREAD_MUTEX_INITIALIZER;

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

/*
 * config_mutex is held if urcu_game_config_update_begin() returns
 * non-NULL. It needs to be followed by a urcu_game_config_update_end().
 * Holding the mutex across begin and end ensures the configuration is
 * not modified concurrently.
 */
struct urcu_game_config *urcu_game_config_update_begin(void)
{
	struct urcu_game_config *new_config;

	new_config = malloc(sizeof(*new_config));
	if (!new_config) {
		return NULL;
	}
	pthread_mutex_lock(&config_mutex);
	if (current_config)
		memcpy(new_config, current_config, sizeof(*new_config));
	else
		memset(new_config, 0, sizeof(*new_config));
	return new_config;
}

void urcu_game_config_update_end(struct urcu_game_config *new_config)
{
	struct urcu_game_config *old_config;

	old_config = current_config;
	rcu_set_pointer(&current_config, new_config);
	pthread_mutex_unlock(&config_mutex);
	if (old_config)
		call_rcu(&old_config->rcu_head, config_free);
}

void urcu_game_config_update_abort(struct urcu_game_config *new_config)
{
	pthread_mutex_unlock(&config_mutex);
	free(new_config);
}

void init_game_config(void)
{
	struct urcu_game_config *new_config;

	new_config = urcu_game_config_update_begin();
	new_config->island_size = DEFAULT_ISLAND_SIZE;
	new_config->step_delay = DEFAULT_STEP_DELAY;
	new_config->gerbil.max_birth_stamina =
			DEFAULT_GERBIL_MAX_BIRTH_STAMINA;
	new_config->gerbil.animal = GERBIL;
	new_config->gerbil.diet = DIET_FLOWERS | DIET_TREES;
	new_config->gerbil.max_pregnant = 10;

	new_config->cat.max_birth_stamina =
			DEFAULT_CAT_MAX_BIRTH_STAMINA;
	new_config->cat.animal = CAT;
	new_config->cat.diet = DIET_GERBIL | DIET_FLOWERS;
	new_config->cat.max_pregnant = 4;

	new_config->snake.max_birth_stamina =
			DEFAULT_SNAKE_MAX_BIRTH_STAMINA;
	new_config->snake.animal = SNAKE;
	new_config->snake.diet = DIET_GERBIL | DIET_CAT;
	new_config->snake.max_pregnant = 1;

	urcu_game_config_update_end(new_config);
}
