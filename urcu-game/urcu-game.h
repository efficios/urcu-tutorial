#ifndef URCU_GAME_H
#define URCU_GAME_H

/*
 * urcu-game.h
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
#include <stdio.h>
#include <urcu/rculfhash.h>

#define URCU_GAME_REFRESH_PERIOD	1	/* seconds */

enum animal_types {
	GERBIL,
	CAT,
	SNAKE,
};

enum diet_mask {
	DIET_GERBIL =	(1U << 0),
	DIET_CAT =	(1U << 1),
	DIET_SNAKE =	(1U << 2),
	DIET_FLOWERS =	(1U << 3),
	DIET_TREES =	(1U << 4),
};

struct animal_kind {
	uint64_t max_birth_stamina;	/* Animal healtiness on birth */
	enum animal_types animal;
	unsigned int diet;
};

struct vegetation {
	uint64_t flowers;
	uint64_t trees;
};

struct urcu_game_config {
	uint64_t island_size;	/* max number of animals on the island */

	/* configuration for each animal type newborn */
	struct animal_kind gerbil;
	struct animal_kind cat;
	struct animal_kind snake;

	/* food available on the island */
	struct vegetation vegetation;

	struct rcu_head rcu_head;	/* Delayed reclaim */
};

/*
 * Animal struct existence is guaranteed by RCU. Mutual exclusion
 * against concurrent updaters is done by holding the lock. Holding the
 * lock and testing whether the structure is still within the "all
 * animals" hash table ensures we don't touch a dead animal.
 */
struct animal {
	struct animal_kind kind;

	uint64_t stamina;
	uint64_t hungriness;

	pthread_mutex_t lock;		/* mutual exclusion on animal */
	struct cds_lfht_node kind_node;	/* node in kind hash table */
	struct cds_lfht_node all_node;	/* node in all animals hash table */
};

/*
 * Data structure containing live animals.
 * Nodes are "owned" by the "all animals" hash table, and have an extra
 * reference from the per-kind hash table.
 */
struct live_animals {
	struct cds_lfht *gerbil;
	struct cds_lfht *cat;
	struct cds_lfht *snake;

	struct cds_lfht *all;
};

/* Threads */

extern int exit_program;
extern int hide_output;
extern pthread_mutex_t print_output_mutex;

int create_input_thread(void);
int join_input_thread(void);
int create_output_thread(void);
int join_output_thread(void);
int create_dispatch_thread(void);
int join_dispatch_thread(void);

/* Helpers */

extern int verbose;

#define DBG(fmt, args...)						\
	do {								\
		if (verbose)						\
			printf("[debug %s()@" __FILE__ ":%d] " fmt "\n",\
				__func__, __LINE__, ## args);	\
	} while (0)

#endif /* URCU_GAME_H */
