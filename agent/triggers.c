/* Copyright (c) 2016 Kewin Rausch <kewin.rausch@create-net.org>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Empower Agent internal triggers logic.
 */

#include <stdlib.h>

#include <emlog.h>

#include "triggers.h"

struct trigger * tr_add(
	struct tr_context * tc, int id, int type, EmageMsg * req) {

	struct trigger * t = tr_has_trigger(tc, id, type);

	if(t) {
		EMDBG("Trigger %d already exists", type);
		return t;
	}

	t = malloc(sizeof(struct trigger));

	if(!t) {
		EMLOG("Not enough memory for new trigger!");
		return 0;
	}

	INIT_LIST_HEAD(&t->next);
	t->id = id;
	t->type = type;
	t->req = req;

/****** LOCK ******************************************************************/
	pthread_spin_lock(&tc->lock);
	list_add(&t->next, &tc->ts);
	pthread_spin_unlock(&tc->lock);
/****** UNLOCK ****************************************************************/

	EMDBG("New trigger enabled, id=%d, type=%d", id, type);

	return t;
}

struct trigger * tr_has_trigger(struct tr_context * tc, int id, int type) {
	struct trigger * t = 0;
	int found = 0;

/****** LOCK ******************************************************************/
	pthread_spin_lock(&tc->lock);
	list_for_each_entry(t, &tc->ts, next) {
		if(t->type == type && t->id == id) {
			found = 1;
			break;
		}
	}
	pthread_spin_unlock(&tc->lock);
/****** UNLOCK ****************************************************************/

	if(!found) {
		return 0;
	}

	return t;
}

int tr_flush(struct tr_context * tc) {
	struct trigger * t = 0;
	struct trigger * u = 0;

/****** LOCK ******************************************************************/
	pthread_spin_lock(&tc->lock);
	list_for_each_entry_safe(t, u, &tc->ts, next) {
		EMDBG("Flushing out trigger %d", t->id);

		list_del(&t->next);
		free(t);
	}
	pthread_spin_unlock(&tc->lock);
/****** UNLOCK ****************************************************************/

	return 0;
}

int tr_rem(struct tr_context * tc, int id) {
	struct trigger * t = 0;
	struct trigger * u = 0;

/****** LOCK ******************************************************************/
	pthread_spin_lock(&tc->lock);
	list_for_each_entry_safe(t, u, &tc->ts, next) {
		if(t->id == id) {
			EMDBG("Removing trigger %d", t->id);

			list_del(&t->next);

			if(t->req) {
				emage_msg__free_unpacked(t->req, 0);
			}

			free(t);
		}
	}
	pthread_spin_unlock(&tc->lock);
/****** UNLOCK ****************************************************************/

	return 0;
}
