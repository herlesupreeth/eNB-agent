/* Copyright (c) 2016 Kewin Rausch
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
 *  Empower Agent messages handling procedures.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <netinet/in.h>

#include <emlog.h>
#include <emage/pb/main.pb-c.h>

#include "msg.h"

/* Reserved area for every message to define the message length. */
#define EM_MSG_LENGTH_HEADROOM		4

int msg_parse(char ** buf, int * size, EmageMsg * msg) {
	char * rb = 0;

	int hr = 0;
	int ms = emage_msg__get_packed_size(msg);

	hr = htonl(ms);

	/*
	 * Buffer creation and fill up.
	 */

	rb = malloc(sizeof(char) * (ms + EM_MSG_LENGTH_HEADROOM));

	if(!rb) {
		EMLOG("No more memory!");
		return -1;
	}

	/* Data to the buffer. */
	memcpy(rb, &hr, EM_MSG_LENGTH_HEADROOM);
	emage_msg__pack(msg, rb + EM_MSG_LENGTH_HEADROOM);

	*buf = rb;
	*size = ms + EM_MSG_LENGTH_HEADROOM;

	return 0;
}

int msg_parse_hello(int seq, int bid, char ** buf, int * size) {
	char * rb = 0;
	int ms = 0;
	int hr = 0;

	EmageMsg msg    = EMAGE_MSG__INIT;
	Header hdr      = HEADER__INIT;
	SingleEvent se  = SINGLE_EVENT__INIT;
	Hello hello     = HELLO__INIT;
	HelloRepl hrepl = HELLO_REPL__INIT;

	hdr.vers = 1;
	hdr.seq  = seq;
	hdr.t_id = 0;
	hdr.b_id = bid;

	hrepl.period = 1000;

	hello.hello_m_case = HELLO__HELLO_M_REPL;
	hello.repl = &hrepl;

	se.events_case = SINGLE_EVENT__EVENTS_M_HELLO;
	se.mhello = &hello;

	msg.head = &hdr;
	msg.se = &se;
	msg.event_types_case = EMAGE_MSG__EVENT_TYPES_SE;

	/*
	 * Buffer creation and fill up.
	 */

	return msg_parse(buf, size, &msg);
}
