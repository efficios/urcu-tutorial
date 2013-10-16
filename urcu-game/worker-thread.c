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
#include <string.h>
#include <urcu.h>
#include "worker-thread.h"
#include "urcu-game.h"
#include "urcu-game-config.h"
#include "ht-hash.h"

static
struct worker_thread *worker_threads;

static
long nr_worker_threads;

static
__thread unsigned int thread_rand_seed;

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
void unlock_single(struct animal *first)
{
	pthread_mutex_unlock(&first->lock);
}

/*
 * Called with RCU read-side lock held.
 */
static
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


/*
 * Called with RCU read-side lock held.
 * Needs to be called with animal lock held.
 */
static
void kill_animal(struct animal *animal)
{
	int delret;
	struct cds_lfht *ht;

	delret = cds_lfht_del(live_animals.all, &animal->all_node);
	assert(delret == 0);
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
}

/*
 * Called with RCU read-side lock held.
 */
static
int try_eat(struct animal *first, struct animal *second)
{
	int ret = 0;

	if (!second) {
		if (first->kind.diet & DIET_FLOWERS) {
			if (lock_test_single(first)) {
				first->stamina++;
				unlock_single(first);
			}
			/* TODO: decrement flowers */
			ret = 1;
		}
		if (first->kind.diet & DIET_TREES) {
			if (lock_test_single(first)) {
				first->stamina++;
				unlock_single(first);
			}
			/* TODO: decrement trees */
			ret = 1;
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
			unlock_single(first);
		}
		if (second && lock_test_single(second)) {
			second->stamina--;
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
	return *key == animal->key;
}

static
int animal_match_kind(struct cds_lfht_node *node, const void *_key)
{
	const uint64_t *key = _key;
	const struct animal *animal;

	animal = caa_container_of(node, const struct animal, kind_node);
	return *key == animal->key;
}

static
unsigned long animal_hash(uint64_t key)
{
	return hash_u64(&key, live_animals.ht_seed);
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
		break;
	case CAT:
		memcpy(&child->kind, &config->cat, sizeof(child->kind));
		break;
	case SNAKE:
		memcpy(&child->kind, &config->snake, sizeof(child->kind));
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

	/*
	 * We need to lock the parent to ensure it is not killed
	 * concurrently before giving birth.
	 */
	if (!god && !lock_test_single(parent)) {
		free(child);
		return 0;
	}

	node = cds_lfht_add_unique(live_animals.all,
		animal_hash(new_key),
		animal_match_all,
		&new_key,
		&child->all_node);
	if (node == &child->all_node) {
		/* Successfully added */
		parent->nr_pregnant--;
		if (!god)
			unlock_single(parent);
		return 1;
	} else {
		/* Another node already present */
		free(child);
		if (!god)
			unlock_single(parent);
		return 0;
	}
}

static
int do_work(struct urcu_game_work *work)
{
	struct cds_lfht_node *first_node, *second_node;
	struct animal *first = NULL, *second = NULL;
	struct cds_lfht_iter iter;

	if (work->exit_thread)
		return 1;

	rcu_read_lock();

	cds_lfht_lookup(live_animals.all,
			animal_hash(work->first_key),
			animal_match_all,
			&work->first_key,
			&iter);
	first_node = cds_lfht_iter_get_node(&iter);
	if (first_node)
		first = caa_container_of(first_node,
			struct animal, all_node);

	cds_lfht_lookup(live_animals.all,
			animal_hash(work->second_key),
			animal_match_all,
			&work->second_key,
			&iter);
	second_node = cds_lfht_iter_get_node(&iter);
	if (second_node)
		second = caa_container_of(second_node,
			struct animal, all_node);

	/*
	 * If only one of the nodes is non-null, it is the first.
	 */
	if (!first) {
		first = second;
		if (!first)
			goto end;
	}

	if (try_birth(first, work->second_key, 0))
		goto end;
	if (try_eat(first, second))
		goto end;
	if (try_mate(first, second))
		goto end;

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
		/*
		 * TODO: currently using polling scheme. Could do a
		 * wakeup scheme with sys_futex instead.
		 */
	}
	return 0;
}
