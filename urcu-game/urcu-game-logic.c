/*
 * urcu-game-logic.c
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
#include <urcu.h>
#include <inttypes.h>
#include <string.h>
#include "urcu-game.h"
#include "urcu-game-config.h"
#include "ht-hash.h"

/*
 * Lock and test for existence pair of nodes.
 *
 * Returns 1 on success, or 0 (error) if either of the nodes don't
 * exist. Returns with both locks held on success, or no lock held on
 * error.
 * Always grab the locks in increasing order of key to ensure there are
 * no deadlocks.
 */
static
int lock_test_pair(struct animal *first, struct animal *second)
{
	struct animal *tmp;

	if (first->key > second->key) {
		tmp = first;
		first = second;
		second = tmp;
	}
	pthread_mutex_lock(&first->lock);
	if (cds_lfht_is_node_deleted(&first->all_node))
		goto error_first;
	pthread_mutex_lock(&second->lock);
	if (cds_lfht_is_node_deleted(&second->all_node))
		goto error_second;
	/* ok */
	return 1;

error_second:
	pthread_mutex_unlock(&second->lock);
error_first:
	pthread_mutex_unlock(&first->lock);
	return 0;	/* error */
}

static
void lock_pair(struct animal *first, struct animal *second)
{
	struct animal *tmp;

	if (first->key > second->key) {
		tmp = first;
		first = second;
		second = tmp;
	}
	pthread_mutex_lock(&first->lock);
	pthread_mutex_lock(&second->lock);
}

static
void unlock_pair(struct animal *first, struct animal *second)
{
	pthread_mutex_unlock(&second->lock);
	pthread_mutex_unlock(&first->lock);
}

static
int lock_test_single(struct animal *first)
{
	pthread_mutex_lock(&first->lock);
	if (cds_lfht_is_node_deleted(&first->all_node))
		goto error_first;
	/* ok */
	return 1;

error_first:
	pthread_mutex_unlock(&first->lock);
	return 0;	/* error */
}

static
void lock_single(struct animal *first)
{
	pthread_mutex_lock(&first->lock);
}


static
void unlock_single(struct animal *first)
{
	pthread_mutex_unlock(&first->lock);
}

/*
 * Called with RCU read-side lock held.
 */
int try_mate(struct animal *first, struct animal *second)
{
	struct animal *female;
	int ret = 0;

	/*
	 * We can test a variety of conditions without holding the lock.
	 */
	if (!second)
		return 0;
	if (first->kind.animal != second->kind.animal)
		return 0;
	if (first->animal_sex == second->animal_sex)
		return 0;
	if (first->animal_sex == ANIMAL_FEMALE)
		female = first;
	else
		female = second;

	if (lock_test_pair(first, second)) {
		if (!first->nr_pregnant && !second->nr_pregnant) {
			/*
			 * We check if already pregnant with locks held,
			 * since pregnancy state could otherwise change
			 * concurrently.
			 */
			female->nr_pregnant =
				rand_r(&thread_rand_seed)
					% female->kind.max_pregnant;
			ret = 1;
		}
		unlock_pair(first, second);
	}
	return ret;
}

static
void free_animal(struct rcu_head *head)
{
	struct animal *animal;

	animal = caa_container_of(head, struct animal, rcu_head);
	free(animal);
}

/*
 * Called with RCU read-side lock held.
 * Needs to be called with animal lock held, or as single thread during
 * apocalypse.
 */
void kill_animal(struct animal *animal)
{
	int delret;
	struct cds_lfht *ht;

	switch (animal->kind.animal) {
	case GERBIL:
		ht = live_animals.gerbil;
		break;
	case CAT:
		ht = live_animals.cat;
		break;
	case SNAKE:
		ht = live_animals.snake;
		break;
	default:
		abort();
	}
	delret = cds_lfht_del(ht, &animal->kind_node);
	assert(delret == 0);
	/*
	 * We need to remove animal from "all" hash table _after_
	 * removing it from the kind hash table, to match the fact that
	 * existence in the "all" hash table defines the life-time of
	 * the object. Removing in the reverse order can trigger an
	 * abort() in the check for the ht_kind add_unique.
	 */
	delret = cds_lfht_del(live_animals.all, &animal->all_node);
	assert(delret == 0);
	call_rcu(&animal->rcu_head, free_animal);
}

/*
 * Called with RCU read-side lock held.
 */
int try_eat(struct animal *first, struct animal *second)
{
	int ret = 0;

	if (!second) {
		if (first->kind.diet & DIET_FLOWERS) {
			if (lock_test_single(first)) {
				pthread_mutex_lock(&vegetation.lock);
				if (vegetation.flowers) {
					first->stamina++;
					vegetation.flowers--;
					ret = 1;
				}
				pthread_mutex_unlock(&vegetation.lock);
				unlock_single(first);
			}
		}
		if (!ret && first->kind.diet & DIET_TREES) {
			if (lock_test_single(first)) {
				pthread_mutex_lock(&vegetation.lock);
				if (vegetation.trees) {
					first->stamina++;
					vegetation.trees--;
					ret = 1;
				}
				pthread_mutex_unlock(&vegetation.lock);
				unlock_single(first);
			}
		}
	} else {
		/* First animal has effect of surprise */
		switch (second->kind.animal) {
		case GERBIL:
			if (first->kind.diet & DIET_GERBIL) {
				if (lock_test_pair(first, second)) {
					kill_animal(second);
					first->stamina++;
					ret = 1;
					unlock_pair(first, second);
				}
			}
			break;
		case CAT:
			if (first->kind.diet & DIET_CAT) {
				if (lock_test_pair(first, second)) {
					kill_animal(second);
					first->stamina++;
					ret = 1;
					unlock_pair(first, second);
				}
			}
			break;
		case SNAKE:
			if (first->kind.diet & DIET_SNAKE) {
				if (lock_test_pair(first, second)) {
					kill_animal(second);
					first->stamina++;
					ret = 1;
					unlock_pair(first, second);
				}
			}
			break;
		}

		switch (first->kind.animal) {
		case GERBIL:
			if (second->kind.diet & DIET_GERBIL) {
				if (lock_test_pair(first, second)) {
					kill_animal(first);
					second->stamina++;
					ret = 1;
					unlock_pair(first, second);
				}
			}
			break;
		case CAT:
			if (second->kind.diet & DIET_CAT) {
				if (lock_test_pair(first, second)) {
					kill_animal(first);
					second->stamina++;
					ret = 1;
					unlock_pair(first, second);
				}
			}
			break;
		case SNAKE:
			if (second->kind.diet & DIET_SNAKE) {
				if (lock_test_pair(first, second)) {
					kill_animal(first);
					second->stamina++;
					ret = 1;
					unlock_pair(first, second);
				}
			}
			break;
		}
	}
	/*
	 * If none of the animals involved in the encounter can eat,
	 * decrement their stamina.
	 */
	if (!ret) {
		if (lock_test_single(first)) {
			first->stamina--;
			if (!first->stamina)
				kill_animal(first);
			unlock_single(first);
		}
		if (second && lock_test_single(second)) {
			second->stamina--;
			if (!second->stamina)
				kill_animal(second);
			unlock_single(second);
		}
	}
	return ret;
}

static
int animal_match_all(struct cds_lfht_node *node, const void *_key)
{
	const uint64_t *key = _key;
	const struct animal *animal;

	animal = caa_container_of(node, const struct animal, all_node);
	DBG("match all compare %" PRIu64 " with %" PRIu64,
		*key, animal->key);
	return *key == animal->key;
}

static
int animal_match_kind(struct cds_lfht_node *node, const void *_key)
{
	const uint64_t *key = _key;
	const struct animal *animal;

	animal = caa_container_of(node, const struct animal, kind_node);
	DBG("match kind compare %" PRIu64 " with %" PRIu64,
		*key, animal->key);
	return *key == animal->key;
}

static
unsigned long animal_hash(uint64_t key)
{
	unsigned long ret;

	ret = hash_u64(&key, live_animals.ht_seed);
	DBG("hash: %" PRIu64 " with seed %lu, result: %lu",
		key, live_animals.ht_seed, ret);
	return ret;
}

/*
 * If "god" is non-zero, the animal is spontaneously created.
 * Called with RCU read-side lock held.
 */
int try_birth(struct animal *parent, uint64_t new_key, int god)
{
	struct cds_lfht_node *node;
	struct animal *child;
	struct urcu_game_config *config;
	struct cds_lfht *kind_ht;

	if (!god && !parent->nr_pregnant)
		return 0;

	child = calloc(1, sizeof(*child));
	if (!child)
		abort();

	config = urcu_game_config_get();

	/*
	 * Update child kind with current configuration.
	 */
	switch (parent->kind.animal) {
	case GERBIL:
		memcpy(&child->kind, &config->gerbil, sizeof(child->kind));
		kind_ht = live_animals.gerbil;
		break;
	case CAT:
		memcpy(&child->kind, &config->cat, sizeof(child->kind));
		kind_ht = live_animals.cat;
		break;
	case SNAKE:
		memcpy(&child->kind, &config->snake, sizeof(child->kind));
		kind_ht = live_animals.snake;
		break;
	default:
		abort();
	}

	child->animal_sex = (rand_r(&thread_rand_seed) & 1) ?
		ANIMAL_FEMALE : ANIMAL_MALE;
	child->key = new_key;
	child->stamina = rand_r(&thread_rand_seed)
		% child->kind.max_birth_stamina;
	child->nr_pregnant = 0;
	pthread_mutex_init(&child->lock, NULL);

	assert(child->kind.max_pregnant > 0);

	/*
	 * We need to lock the parent to ensure it is not killed
	 * concurrently before giving birth.
	 *
	 * We hold the child lock while adding into the hash tables to
	 * ensure that adding into the kind hash table will always
	 * succeed whenever adding into the "all" hash table did
	 * succeed. This blocks any "kill" action on the child while
	 * being added only to one hash table, therefore ensuring
	 * consistency.
	 */
	if (!god) {
		lock_pair(parent, child);
		if (cds_lfht_is_node_deleted(&parent->all_node)) {
			unlock_pair(parent, child);
			free(child);
			return 0;
		}
	} else {
		lock_single(child);
	}

	node = cds_lfht_add_unique(live_animals.all,
		animal_hash(new_key),
		animal_match_all,
		&new_key,
		&child->all_node);
	if (node == &child->all_node) {
		node = cds_lfht_add_unique(kind_ht,
			animal_hash(new_key),
			animal_match_kind,
			&new_key,
			&child->kind_node);
		if (node != &child->kind_node)
			abort();
		/* Successfully added */
		parent->nr_pregnant--;
		if (!god)
			unlock_pair(parent, child);
		else
			unlock_single(child);
		return 1;
	} else {
		/* Another node already present */
		if (!god)
			unlock_pair(parent, child);
		else
			unlock_single(child);
		free(child);
		return 0;
	}
}

/*
 * Called from RCU read-side critical section. RCU read-side critical
 * section should encompass use of returned struct animal pointer.
 */
struct animal *find_animal(uint64_t key)
{
	struct cds_lfht_iter iter;
	struct cds_lfht_node *node;
	struct animal *animal = NULL;

	cds_lfht_lookup(live_animals.all,
			animal_hash(key),
			animal_match_all,
			&key,
			&iter);
	node = cds_lfht_iter_get_node(&iter);
	if (node)
		animal = caa_container_of(node,
			struct animal, all_node);
	return animal;
}

void apocalypse(void)
{
	struct cds_lfht *ht = live_animals.all;
	struct cds_lfht_iter iter;
	struct animal *animal;

	DBG("Apocalypse");
	rcu_read_lock();
	cds_lfht_for_each_entry(ht, &iter, animal, all_node) {
		DBG("Kill animal %" PRIu64, animal->key);
		pthread_mutex_lock(&animal->lock);
		if (!cds_lfht_is_node_deleted(&animal->all_node))
			kill_animal(animal);
		pthread_mutex_unlock(&animal->lock);
	}
	rcu_read_unlock();
}

/*
 * Try to create at most "nr" animals. No guarantee of success.
 */
void create_animals(enum animal_types type, uint64_t nr)
{
	uint64_t i;
	struct animal parent;
	struct urcu_game_config *config;

	rcu_read_lock();
	config = urcu_game_config_get();
	/*
	 * When we create animal as god, we only care about animal type.
	 * The rest is derived from the current configuration.
	 */
	parent.kind.animal = type;

	for (i = 0; i < nr; i++) {
		uint64_t child_key =
			rand_r(&thread_rand_seed) % config->island_size;
		int ret;

		ret = try_birth(&parent, child_key, 1);
		DBG("God create animal %d, return: %d",
			type, ret);
	}
	rcu_read_unlock();
}
