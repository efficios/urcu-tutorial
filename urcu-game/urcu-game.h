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
#include <urcu-call-rcu.h>

#define URCU_GAME_REFRESH_PERIOD	1	/* seconds */

enum animal_types {
	GERBIL = 	0,
	CAT = 		1,
	SNAKE = 	2,
};

enum diet_mask {
	DIET_GERBIL =	(1U << GERBIL),
	DIET_CAT =	(1U << CAT),
	DIET_SNAKE =	(1U << SNAKE),

	DIET_FLOWERS =	(1U << 3),
	DIET_TREES =	(1U << 4),
};

struct animal_kind {
	uint64_t max_birth_stamina;	/* Animal healtiness on birth */
	uint64_t max_pregnant;
	enum animal_types animal;
	unsigned int diet;
};

#define DEFAULT_ISLAND_SIZE			\
	2 * (DEFAULT_VEGETATION_FLOWERS + DEFAULT_VEGETATION_TREES)
#define DEFAULT_STEP_DELAY			1000
#define DEFAULT_GERBIL_MAX_BIRTH_STAMINA	70
#define DEFAULT_CAT_MAX_BIRTH_STAMINA		80
#define DEFAULT_SNAKE_MAX_BIRTH_STAMINA		30
#define DEFAULT_VEGETATION_FLOWERS		1000
#define DEFAULT_VEGETATION_TREES		200

struct urcu_game_config {
	uint64_t island_size;		/* max number of animals on the island */
	unsigned int step_delay;	/* game step delay, in ms */

	/* configuration for each animal type newborn */
	struct animal_kind gerbil;
	struct animal_kind cat;
	struct animal_kind snake;
};

enum animal_sex {
	ANIMAL_MALE,
	ANIMAL_FEMALE,
};

/*
 * Animal struct existence is guaranteed by RCU. Mutual exclusion
 * against concurrent updaters is done by holding the lock. Holding the
 * lock and testing whether the structure is still within the "all
 * animals" hash table ensures we don't touch a dead animal.
 */
struct animal {
	struct animal_kind kind;

	enum animal_sex animal_sex;
	uint64_t key;			/* animal key in hash table */
	uint64_t stamina;

	uint64_t nr_pregnant;

	pthread_mutex_t lock;		/* mutual exclusion on animal */
	struct cds_lfht_node kind_node;	/* node in kind hash table */
	struct cds_lfht_node all_node;	/* node in all animals hash table */
	struct rcu_head rcu_head;	/* Delayed reclaim */
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

	unsigned long ht_seed;
};

struct vegetation {
	uint64_t flowers;
	uint64_t trees;
	pthread_mutex_t lock;
};

/* Game logic */
extern struct live_animals live_animals;
extern struct vegetation vegetation;

int try_birth(struct animal *parent, uint64_t new_key, int god);
void kill_animal(struct animal *animal);
int try_eat(struct animal *first, struct animal *second);
int try_birth(struct animal *parent, uint64_t new_key, int god);
int try_mate(struct animal *first, struct animal *second);
struct animal *find_animal(uint64_t key);

/* Threads */
extern int exit_program;
extern int hide_output;
extern pthread_mutex_t print_output_mutex;

extern __thread unsigned int thread_rand_seed;

int create_input_thread(void);
int join_input_thread(void);
int create_output_thread(void);
int join_output_thread(void);
int create_dispatch_thread(void);
int join_dispatch_thread(void);

/* Helpers */

extern int verbose, clear_screen_enable;

/*
 * This is a terminal hack. Should use ncurse, but trying to avoid extra
 * dependencies. We start with a newline just in case the escape code
 * does not work, so at least we're adding a white line between menus.
 */
static inline
void clear_screen(void)
{
	printf("\n");
	if (clear_screen_enable)
		printf("%c[2J%c[;H", (char) 27, (char) 27);
}

#define DBG(fmt, args...)						\
	do {								\
		if (verbose)						\
			printf("[debug %s()@" __FILE__ ":%d] " fmt "\n",\
				__func__, __LINE__, ## args);	\
	} while (0)

#endif /* URCU_GAME_H */
