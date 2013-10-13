#ifndef URCU_GAME_CONFIG_H
#define URCU_GAME_CONFIG_H

/*
 * urcu-game-config.h
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

#include <stdint.h>
#include <urcu-call-rcu.h>	/* rcu_head */
#include "urcu-game.h"

/*
 * urcu_game_config_get needs to be called within RCU read-side
 * critical section, and the returned pointer needs to be used within
 * that same RCU read-side critical section.
 */
struct urcu_game_config *urcu_game_config_get(void);

/*
 * An internal mutex is held if urcu_game_config_update_begin() returns
 * non-NULL. If begin returns non-NULL, it needs to be followed by a
 * urcu_game_config_update_end(). The configuration is updated when
 * urcu_game_config_update_end() is called.
 * urcu_game_config_update_abort() can be called to abort an update
 * (instead of calling "end").
 */
struct urcu_game_config *urcu_game_config_update_begin(void);
void urcu_game_config_update_end(struct urcu_game_config *new_config);
void urcu_game_config_update_abort(struct urcu_game_config *new_config);

#endif /* URCU_GAME_CONFIG_H */
