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
#include <urcu/system.h>
#include "urcu-game.h"

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
void *input_thread_fct(void *data)
{
	DBG("In user input thread.");

	/* Read keys typed by the user */
	for (;;) {
		char key;
		int ret;

		ret = getch(&key);
		if (ret < 0)
			goto end;

		DBG("User input: \'%c\'", key);

		switch(key) {
		case 'q':
			CMM_STORE_SHARED(exit_program, 1);
			goto end;
			/* TODO other keys */
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
