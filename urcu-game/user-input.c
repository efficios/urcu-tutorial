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

	CMM_STORE_SHARED(hide_output, 1);	/* hide normal output */
	pthread_mutex_lock(&print_output_mutex);

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
		printf("  s	Save update and exit configuration menu\n");
		printf("  i	Island size (%" PRIu64 ")\n", new_config->island_size);
		printf("  f	Flowers (%" PRIu64 ")\n", new_config->vegetation.flowers);
		printf("  t	Trees (%" PRIu64 ")\n", new_config->vegetation.trees);

		ret = getch(&key);
		if (ret < 0)
			goto end;

		DBG("User input: \'%c\'", key);

		switch(key) {
		case 's':	/* save and quit */
			printf("Configuration saved.\n");
			urcu_game_config_update_end(new_config);
			goto end;
		case 'q':	/* cancel and quit */
			printf("Configuration update cancelled.\n");
			urcu_game_config_update_abort(new_config);
			goto end;
		case 'i':	/* island size */
			get_config_entry_uint64("island size",
				&new_config->island_size);
			break;
		case 'f':	/* flowers */
			get_config_entry_uint64("flower vegetation",
				&new_config->vegetation.flowers);
			break;
		case 't':	/* trees */
			get_config_entry_uint64("tree vegetation",
				&new_config->vegetation.trees);
			break;

			/* TODO other keys */
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
void show_menu(void)
{
	pthread_mutex_lock(&print_output_mutex);
	printf("\n");
	printf("[ root ]\n");
	printf(" key	Description\n");
	printf("---------------------------------\n");
	printf("  m	Show menu\n");
	printf("  c	Configuration menu\n");
	printf("  q	Quit game\n");
	pthread_mutex_unlock(&print_output_mutex);
}


static
void *input_thread_fct(void *data)
{
	DBG("In user input thread.");

	/* Read keys typed by the user */
	for (;;) {
		char key;
		int ret;

		show_menu();

		ret = getch(&key);
		if (ret < 0)
			goto end;

		DBG("User input: \'%c\'", key);

		switch(key) {
		case 'q':	/* quit */
			CMM_STORE_SHARED(exit_program, 1);
			goto end;
		case 'c':	/* config */
			do_config();
			break;
		case 'm':
			break;	/* show menu */
			/* TODO other keys */
		default:
			printf("Unknown key: \'%c\'\n", key);
			break;
		}
	}

end:
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
