/*
 * user-input.c
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

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <termios.h>
#include <inttypes.h>
#include <limits.h>
#include <urcu.h>
#include <urcu/system.h>
#include "urcu-game.h"
#include "urcu-game-config.h"

static
pthread_t input_thread_id;

/*
 * Read characters from terminal, without awaiting for newline and
 * without echo. Return 0 if OK, -1 on end of file or error.
 */
static
int getch(char *_key)
{
	char key = 0;
	struct termios old = { 0 };
	ssize_t len;
	int ret = 0;

	if (tcgetattr(0, &old) < 0) {
		perror("tcsetattr()");
		ret = -1;
		goto end;
	}
	old.c_lflag &= ~ICANON;
	old.c_lflag &= ~ECHO;
	old.c_cc[VMIN] = 1;
	old.c_cc[VTIME] = 0;
	if (tcsetattr(0, TCSANOW, &old) < 0) {
		perror("tcsetattr ICANON");
		ret = -1;
		goto end;
	}
	do {
		len = read(0, &key, sizeof(key));
	} while (len < 0 && errno == EINTR);
	if (len < 0) {
		perror("read()");
		ret = -1;
	}
	if (len == 0) {
		/* End of file */
		ret = -1;
	}
	old.c_lflag |= ICANON;
	old.c_lflag |= ECHO;
	if (tcsetattr(0, TCSADRAIN, &old) < 0) {
		perror("tcsetattr ~ICANON");
		ret = -1;
	}
	*_key = key;
end:
	return ret;
}

static
int readline_unbuf(int fd, char *buffer, size_t maxlen)
{
	char key;
	size_t pos = 0;
	int ret = 0;

	for (;;) {
		ssize_t len;

		if (pos >= maxlen) {
			ret = -1;
			goto end;
		}
		do {
			len = read(fd, &key, sizeof(key));
		} while (len < 0 && errno == EINTR);
		if (len < 0) {
			perror("read()");
			ret = -1;
			goto end;
		}
		if (len == 0) {
			/* End of file */
			ret = -1;
			goto end;
		}
		if (key == '\n')
			break;
		buffer[pos++] = key;
	}
	buffer[pos] = '\0';
end:
	return ret;
}

static
void get_config_entry_uint64(const char *name,
		uint64_t *value)
{
	uint64_t new_size;
	char read_buf[4096];
	int ret;

	fflush(stdin);
	printf("Enter new %s\n", name);
	ret = readline_unbuf(0, read_buf, sizeof(read_buf));
	if (ret < 0) {
		fprintf(stderr, "Error: expected digital input for %s\n",
			name);
		return;
	}
	errno = 0;
	ret = sscanf(read_buf, "%" SCNu64, &new_size);
	if (ret != 1 || errno != 0) {
		perror("fscanf");
		fprintf(stderr, "Error: expected digital input for %s\n",
			name);
		return;
	}
	*value = new_size;
}

static
void do_config(void)
{
	struct urcu_game_config *new_config;
	char key;
	int ret;

	new_config = urcu_game_config_update_begin();
	if (!new_config)
		abort();

	for (;;) {
		printf("\n");
		printf("[ root > configuration ]\n");
		printf("Enter the config field you wish to update:\n");
		printf(" key	Description\n");
		printf("---------------------------------\n");
		printf("  q	Cancel update\n");
		printf("  x	Save update and exit configuration menu\n");
		printf("  i	Island size (%" PRIu64 ")\n", new_config->island_size);
		printf("  d	Step delay (%d ms)\n", new_config->step_delay);
		printf("  g	Gerbil max birth stamina (%" PRIu64 ")\n",
				new_config->gerbil.max_birth_stamina);
		printf("  c	Cat max birth stamina (%" PRIu64 ")\n",
				new_config->cat.max_birth_stamina);
		printf("  s	Snake max birth stamina (%" PRIu64 ")\n",
				new_config->snake.max_birth_stamina);

		ret = getch(&key);
		if (ret < 0)
			goto end;

		DBG("User input: \'%c\'", key);

		switch(key) {
		case 'x':	/* save and quit */
			printf("Configuration saved.\n");
			urcu_game_config_update_end(new_config);
			goto end;
		case 'q':	/* cancel and quit */
			printf("Configuration update cancelled.\n");
			urcu_game_config_update_abort(new_config);
			goto end;
		case 'i':	/* island size */
		{
			uint64_t new_size;

			get_config_entry_uint64("island size (increase only)",
				&new_size);
			if (new_size <= new_config->island_size) {
				printf("Error: Island size can only be increased.\n");
				break;
			}
			new_config->island_size = new_size;
			break;
		}
		case 'd':	/* step delay */
		{
			uint64_t new_delay;

			get_config_entry_uint64("step delay (ms)",
				&new_delay);
			if (new_delay > INT_MAX) {
				printf("Error: delay specified is too large.\n");
				break;
			}
			new_config->step_delay = (int) new_delay;
			break;
		}
		case 'g':	/* gerbil stamina */
			get_config_entry_uint64("gerbil stamina",
				&new_config->gerbil.max_birth_stamina);
			break;
		case 'c':	/* cat stamina */
			get_config_entry_uint64("cat stamina",
				&new_config->cat.max_birth_stamina);
			break;
		case 's':	/* snake stamina */
			get_config_entry_uint64("snake stamina",
				&new_config->snake.max_birth_stamina);
			break;
		default:
			printf("Unknown key: \'%c\'\n", key);
			break;
		}
	}
end:
	return;
}

/*
 * Try to create at most "nr" animals. No guarantee of success.
 */
static
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

static
void do_god(void)
{
	char key;
	int ret;

	for (;;) {
		printf("\n");
		printf("[ root > god ]\n");
		printf("Enter the animal or vegetation you wish to modify:\n");
		printf("Modifications take effect immediately.\n");
		printf(" key	Description\n");
		printf("---------------------------------\n");
		printf("  x	Exit menu\n");
		printf("  f	Number of flowers\n");
		printf("  t	Number of trees\n");
		printf("  g	Create gerbils\n");
		printf("  c	Create cats\n");
		printf("  s	Create snakes\n");

		ret = getch(&key);
		if (ret < 0)
			goto end;

		DBG("User input: \'%c\'", key);

		switch(key) {
		case 'x':	/* exit menu */
			goto end;
		case 'f':	/* flowers */
		{
			uint64_t value;

			get_config_entry_uint64("number of flowers",
				&value);
			pthread_mutex_lock(&vegetation.lock);
			vegetation.flowers = value;
			pthread_mutex_unlock(&vegetation.lock);
			break;
		}
		case 't':	/* trees */
		{
			uint64_t value;

			get_config_entry_uint64("number of trees",
				&value);
			pthread_mutex_lock(&vegetation.lock);
			vegetation.trees = value;
			pthread_mutex_unlock(&vegetation.lock);
			break;
		}
		case 'g':	/* create gerbils */
		{
			uint64_t value;

			get_config_entry_uint64("amount of gerbils to try creating",
				&value);
			create_animals(GERBIL, value);
			break;
		}
		case 'c':	/* create cats */
		{
			uint64_t value;

			get_config_entry_uint64("amount of cats to try creating",
				&value);
			create_animals(CAT, value);
			break;
		}
		case 's':	/* create snakes */
		{
			uint64_t value;

			get_config_entry_uint64("amount of snakes to try creating",
				&value);
			create_animals(SNAKE, value);
			break;
		}
		default:
			printf("Unknown key: \'%c\'\n", key);
			break;
		}
	}
end:
	return;
}

static
void show_menu(void)
{
	printf("\n");
	printf("[ root ]\n");
	printf(" key	Description\n");
	printf("---------------------------------\n");
	printf("  c	Configuration menu\n");
	printf("  g	Play god\n");
	printf("  x	Exit root menu\n");
}

static
void do_root_menu(void)
{
	char key;
	int ret;

	CMM_STORE_SHARED(hide_output, 1);	/* hide normal output */
	pthread_mutex_lock(&print_output_mutex);

	for (;;) {
		show_menu();

		ret = getch(&key);
		if (ret < 0)
			goto end;

		DBG("User input: \'%c\'", key);

		switch(key) {
		case 'x':	/* exit menu */
			goto end;
		case 'c':	/* config */
			do_config();
			break;
		case 'g':
			do_god();
			break;
		default:
			printf("Unknown key: \'%c\'\n", key);
			break;
		}
	}
end:
	fflush(stdout);
	pthread_mutex_unlock(&print_output_mutex);
	CMM_STORE_SHARED(hide_output, 0);	/* show normal output */
}

static
void *input_thread_fct(void *data)
{
	DBG("In user input thread.");
	rcu_register_thread();

	thread_rand_seed = time(NULL);

	/* Read keys typed by the user */
	for (;;) {
		char key;
		int ret;

		ret = getch(&key);
		if (ret < 0)
			goto end;

		DBG("User input: \'%c\'", key);

		switch(key) {
		case 'q':	/* quit */
			CMM_STORE_SHARED(exit_program, 1);
			goto end;
		case 'm':	/* show menu */
			do_root_menu();
			break;
		default:
			printf("Unknown key: \'%c\'\n", key);
			break;
		}
	}

end:
	rcu_unregister_thread();
	DBG("User input thread exiting.");
	return NULL;
}

int create_input_thread(void)
{
	int err;

	err = pthread_create(&input_thread_id, NULL,
		input_thread_fct, NULL);
	if (err)
		abort();
	return 0;
}

int join_input_thread(void)
{
	int ret;
	void *tret;

	ret = pthread_join(input_thread_id, &tret);
	if (ret)
		abort();
	return 0;
}
