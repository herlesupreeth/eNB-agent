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
 * Empower Agent internal network listener logic.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <emage.h>
#include <emlog.h>
#include <emage/pb/main.pb-c.h>

#include "agent.h"
#include "msg.h"
#include "net.h"
#include "sched.h"

#include "emlist.h"
#include "sched.h"

/* Default buffer size. */
#define EM_BUF_SIZE			4096

/******************************************************************************
 * Network procedures.                                                        *
 ******************************************************************************/

/* Common operations done when it successfully connects again. */
int net_connected(struct net_context * net) {
	struct agent * a = container_of(net, struct agent, net);
	struct sched_job * h = 0;

	EMDBG("Connected to controller %s:%d", net->addr, net->port);
	net->status = EM_STATUS_CONNECTED;

	h = malloc(sizeof(struct sched_job));

	if(!h) {
		EMLOG("No more memory!");
		return -1;
	}

	INIT_LIST_HEAD(&h->next);
	h->elapse = 1000;
	h->type = JOB_TYPE_HELLO;
	h->reschedule = -1;

	/* Add the Hello message. */
	sched_add_job(h, &a->sched);

	return 0;
}

int net_next_seq(struct net_context * net) {
	int ret = 0;

/****** LOCK ******************************************************************/
	pthread_spin_lock(&net->lock);
	ret = net->seq++;
	pthread_spin_unlock(&net->lock);
/****** UNLOCK ****************************************************************/

	return ret;
}

/* Turn the socket in an non-blocking one. */
int net_noblock_socket(int sockfd) {
	int flags = fcntl(sockfd, F_GETFL, 0);

	if(flags < 0) {
		return -1;
	}

	return fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
}

/* Apply the desired personalization to the socket. */
int net_nodelay_socket(int sockfd) {
	int flag = 1; /* Enable no delay... */
	int result = setsockopt(
		sockfd,
		SOL_TCP,
		TCP_NODELAY,
		(char *) &flag,
		sizeof(int));

	if (result < 0) {
		EMLOG("Could not personalize the socket!");
		perror("setsockopt");

		return -1;
	}

	return 0;
}

int net_not_connected(struct net_context * net) {
	EMDBG("No more connected with controller!");

	close(net->sockfd);
	net->sockfd = 0;

	net->status = EM_STATUS_NOT_CONNECTED;
	net->seq = 0;

	return 0;
}

/* Connect to the controller. Returns the open socket file descriptor if a
 * socket for the controller has been open, otherwise loop until a connection
 * has been established.
 *
 * Returns a negative number on error.
 */
int net_connect_to_controller(struct net_context * net) {
	int status = 0;

	struct sockaddr_in srvaddr = {0};
	struct hostent * ctrli = 0;

	if(net->sockfd == 0) {
		status = socket(AF_INET, SOCK_STREAM, 0);
	}

	if(status < 0) {
		EMLOG("Could not create the socket, error=%d", net->sockfd);
		perror("socket");

		return -1;
	}
	/* Socket has been created now. */
	else if (status > 0) {
		net->sockfd = status;
		net_nodelay_socket(net->sockfd);
	}

	EMDBG("Connecting to %s:%d...", net->addr, net->port);

	ctrli = gethostbyname(net->addr);

	if(!ctrli) {
		EMLOG("Could not resolve controller!");
		perror("gethostbyname");

		return -1;
	}

	srvaddr.sin_family = AF_INET;
	memcpy(
		&srvaddr.sin_addr.s_addr,
		ctrli->h_addr,
		ctrli->h_length);
	srvaddr.sin_port = htons(net->port);

	status = connect(
		net->sockfd,
		(struct sockaddr *)&srvaddr,
		sizeof(struct sockaddr));

	if(status < 0) {
		EMDBG("Error while connecting to %s, error=%d",
			net->addr,
			status);

		return -1;
	}

	return 0;
}

/* Receive data. */
int net_recv(struct net_context * context, char * buf, unsigned int size) {
	return recv(context->sockfd, buf, size, MSG_DONTWAIT | MSG_NOSIGNAL);
}

/* Send data. */
int net_send(struct net_context * context, char * buf, unsigned int size) {
#ifdef EM_DEBUG_SEND
	int i = 0;

	for(i = 0; i < size; i++) {
		if(i % 16 == 0) {
			printf("\n");
		}

		printf("%02x ", (unsigned char)buf[i]);
	}

	printf("\n");
#endif

	/* NOTE:
	 * Since sending on a dead socket can cause a signal to be issued to the
	 * application (SIGPIPE), we don't want that the host get disturbed by
	 * this, and so we ask not to notify the error.
	 */
	return send(context->sockfd, buf, size, MSG_DONTWAIT | MSG_NOSIGNAL);
}

int net_sched_job(
	struct agent * a,
	unsigned int id,
	int type,
	int interval,
	int res,
	void * args) {

	struct sched_job * job = malloc(sizeof(struct sched_job));

	if(!job) {
		EMLOG("Not enough memory!");
		return -1;
	}

	memset(job, 0, sizeof(struct sched_job));

	INIT_LIST_HEAD(&job->next);
	job->type = type;
	job->args = args;
	job->id = id;
	job->elapse = interval;
	job->reschedule = res;

	sched_add_job(job, &a->sched);

	return 0;
}

/******************************************************************************
 * Message specific procedures.                                               *
 ******************************************************************************/

int net_se_ctrl_cmd(struct net_context * net, EmageMsg * msg) {
	struct agent * a = container_of(net, struct agent, net);

	if(msg->se->mctrl_cmds->controller_commands_m_case !=
			CONTROLLER_COMMANDS__CONTROLLER_COMMANDS_M_REQ) {

		EMDBG("Invalid command reply received.");
		return 0;

	}

	return net_sched_job(
		a, msg->head->t_id, JOB_TYPE_CTRL_COMMAND, 1, 0, msg);
}

int net_se_enb_cells(struct net_context * net, EmageMsg * msg) {
	struct agent * a = container_of(net, struct agent, net);

	if(msg->se->menb_cells->e_nb_cells_m_case !=
		E_NB_CELLS__E_NB_CELLS_M_REQ) {

		EMDBG("Invalid eNB cell reply received.");
		return 0;

	}

	return net_sched_job(
		a, msg->head->t_id, JOB_TYPE_ENB_CELLS, 1, 0, msg);
}

int net_se_ran_sh(struct net_context * net, EmageMsg * msg) {
	struct agent * a = container_of(net, struct agent, net);

	if(msg->se->mran_sharing_ctrl->ran_sharing_ctrl_m_case !=
		RAN_SHARING_CTRL__RAN_SHARING_CTRL_M_REQ) {

		EMDBG("Invalid RAN sharing reply received.");
		return 0;

	}

	return net_sched_job(
		a, msg->head->t_id, JOB_TYPE_RAN_SHARING, 1, 0, msg);
}

int net_sc_enb_cells(struct net_context * net, EmageMsg * msg) {
	struct agent * a = container_of(net, struct agent, net);

	if(msg->sche->menb_cells->e_nb_cells_m_case !=
		E_NB_CELLS__E_NB_CELLS_M_REQ) {

		EMDBG("Invalid eNB cell reply received.");
		return 0;
	}

	/* Remove any job with such id. */
	if(msg->sche->action == EVENT_ACTION__EA_DEL) {
		sched_remove_job(
			msg->head->t_id,
			JOB_TYPE_ENB_CELLS,
			&a->sched);

		return 0;
	}

	return net_sched_job(
		a,
		msg->head->t_id,
		JOB_TYPE_ENB_CELLS,
		/* In case of no interval, schedule every second. */
		msg->sche->has_interval ? msg->sche->interval : 1000,
		-1,
		msg);
}

/* Schedule an UE ids trigger job. */
int net_te_usid(struct net_context * net, EmageMsg * msg) {
	struct agent * a = container_of(net, struct agent, net);
	struct trigger * t = 0;

	if(!msg->te->mues_id) {
		EMDBG("Malformed UEs trigger message!\n");
		return -1;
	}

	if(msg->te->mues_id->ues_id_m_case == UES_ID__UES_ID_M_REQ) {
		if(!msg->te->has_action) {
			EMDBG("Wrong UEs_ID request format: no action.");
			return -1;
		}

		if(msg->te->action == EVENT_ACTION__EA_DEL) {
			return tr_rem(
				&a->trig,
				msg->head->t_id,
				EM_UEs_ID_REPORT_TRIGGER);
		} else {
			t = tr_add(
				&a->trig,
				msg->head->t_id,
				EM_UEs_ID_REPORT_TRIGGER,
				msg);

			if(!t) {
				return -1;
			}

			net_sched_job(
				a,
				msg->head->t_id,
				JOB_TYPE_UEs_LOG_TRIGGER,
				1,
				0,
				(void *)t);
		}
	}

	return 0;
}

/* Schedule an RRC measurement trigger job. */
int net_te_rrc_meas(struct net_context * net, EmageMsg * msg) {
	struct agent * a = container_of(net, struct agent, net);
	struct trigger * t = 0;

	if(!msg->te->mrrc_meas) {
		EMDBG("Malformed RRC measurement trigger message!\n");
		return -1;
	}

	if(!msg->te->has_action) {
		EMDBG("Wrong UEs_ID request format: no action.");
		return -1;
	}

	if(msg->te->action == EVENT_ACTION__EA_DEL) {
		return tr_rem(
			&a->trig,
			msg->head->t_id,
			EM_RRC_MEAS_TRIGGER);
	} else {
		t = tr_add(
			&a->trig,
			msg->head->t_id,
			EM_RRC_MEAS_TRIGGER,
			msg);

		if(!t) {
			return -1;
		}

		net_sched_job(
			a,
			msg->head->t_id,
			JOB_TYPE_RRC_MEAS_TRIGGER,
			1,
			0,
			(void *)t);
	}

	return 0;
}

/* Schedule an RRC measurement configuration trigger job. */
int net_te_rrc_mcon(struct net_context * net, EmageMsg * msg) {
	struct agent * a = container_of(net, struct agent, net);
	struct trigger * t = 0;

	if(!msg->te->mrrc_meas) {
		EMDBG("Malformed RRC measurement trigger message!\n");
		return -1;
	}

	if(!msg->te->has_action) {
		EMDBG("Wrong UEs_ID request format: no action.");
		return -1;
	}

	if(msg->te->action == EVENT_ACTION__EA_DEL) {
		return tr_rem(
			&a->trig,
			msg->head->t_id,
			EM_RRC_MEAS_CONF_TRIGGER);
	} else {
		t = tr_add(
			&a->trig,
			msg->head->t_id,
			EM_RRC_MEAS_CONF_TRIGGER,
			msg);

		if(!t) {
			return -1;
		}

		net_sched_job(
			a,
			msg->head->t_id,
			JOB_TYPE_RRC_MCON_TRIGGER,
			1,
			0,
			(void *)t);
	}

	return 0;
}

/* Schedule a cell statistics request trigger job. */
int net_te_cell_stats(struct net_context * net, EmageMsg * msg) {
	struct agent * a = container_of(net, struct agent, net);
	struct trigger * t = 0;

	if(!msg->te->mcell_stats) {
		EMDBG("Malformed cell statistics trigger message!\n");
		return -1;
	}

	if(msg->te->action == EVENT_ACTION__EA_DEL) {
		return tr_rem(
			&a->trig,
			msg->head->t_id,
			EM_CELL_STATS_TRIGGER);
	} else {
		t = tr_add(
			&a->trig,
			msg->head->t_id,
			EM_CELL_STATS_TRIGGER,
			msg);

		if(!t) {
			return -1;
		}

		net_sched_job(
			a,
			msg->head->t_id,
			JOB_TYPE_CELL_STATS_TRIGGER,
			1,
			0,
			(void *)t);
	}

	return 0;
}

/******************************************************************************
 * Top-level message handlers.                                                *
 ******************************************************************************/

int net_process_sched_event(struct net_context * net, EmageMsg * msg) {
	ScheduleEvent * ce = msg->sche;

	switch(ce->events_case) {
	case SCHEDULE_EVENT__EVENTS_M_ENB_CELLS:
		return net_sc_enb_cells(net, msg);
	default:
		EMDBG("Unknown scheduled event, type=%d", ce->events_case);
		break;
	}

	return 0;
}

int net_process_single_event(struct net_context * net, EmageMsg * msg) {
	SingleEvent * se = msg->se;

	switch(se->events_case) {
	case SINGLE_EVENT__EVENTS_M_CTRL_CMDS:
		return net_se_ctrl_cmd(net, msg);
	case SINGLE_EVENT__EVENTS_M_ENB_CELLS:
		return net_se_enb_cells(net, msg);
	case SINGLE_EVENT__EVENTS_M_RAN_SHARING_CTRL:
		return net_se_ran_sh(net, msg);
	default:
		EMDBG("Unknown single event, type=%d", se->events_case);
		break;
	}

	return 0;
}

int net_process_trigger_event(struct net_context * net, EmageMsg * msg) {
	TriggerEvent * te = msg->te;

	if(!te) {
		EMDBG("Malformed trigger event message!\n");
		return -1;
	}

	switch(te->events_case) {
	case TRIGGER_EVENT__EVENTS_M_UES_ID:
		return net_te_usid(net, msg);
	case TRIGGER_EVENT__EVENTS_M_RRC_MEAS:
		return net_te_rrc_meas(net, msg);
	case TRIGGER_EVENT__EVENTS_M_UE_RRC_MEAS_CONF:
		return net_te_rrc_mcon(net, msg);
	case TRIGGER_EVENT__EVENTS_M_CELL_STATS:
		return net_te_cell_stats(net, msg);
	default:
		EMDBG("Unknown trigger event, type=%d", te->events_case);
		break;
	}

	return 0;
}

/* Process incoming messages. */
int net_process_message(struct net_context * net, EmageMsg * msg) {
	switch(msg->event_types_case) {
	/* Single events messages. */
	case EMAGE_MSG__EVENT_TYPES_SE:
		return net_process_single_event(net, msg);
	/* Scheduled events messages. */
	case EMAGE_MSG__EVENT_TYPES_SCHE:
		return net_process_sched_event(net, msg);
	/* Triggered events messages. */
	case EMAGE_MSG__EVENT_TYPES_TE:
		return net_process_trigger_event(net, msg);
	default:
		EMDBG("Unknown message received!");
		break;
	}

	return 0;
}

/******************************************************************************
 * Network listener logic.                                                    *
 ******************************************************************************/

void * net_loop(void * args) {
	struct net_context * net = (struct net_context *)args;

	int bread = 0;
	int mlen = 0;

	char buf[EM_BUF_SIZE] = {0};
	EmageMsg * msg = 0;

	unsigned int wi = net->interval;
	struct timespec wt = {0};	/* Wait time. */
	struct timespec td = {0};

	/* Convert the wait interval in a timespec struct. */
	while(wi >= 1000) {
		wi -= 1000;
		wt.tv_sec += 1;
	}
	wt.tv_nsec = wi * 1000000;

	while(!net->stop) {
		if(net->status == EM_STATUS_NOT_CONNECTED) {
			if(net_connect_to_controller(net) == 0) {
				net_connected(net);
			}
		}

		bread = net_recv(net, buf, 4);

		/* Something has been received.
		 * NOTE: Message must be at least 4 bytes long.
		 */
		if(bread == 4) {
			memcpy(&mlen, buf, 4);
			mlen = ntohl(mlen);

			bread = net_recv(net, buf, mlen);

			if (bread != mlen) {
				EMLOG("Malformed message received, "
					"msg=%d, recv=%d", mlen, bread);

				/* Ok, this is serious, since we can loose the
				 * alignment of the 4 firsts bytes which
				 * contains the message size.
				 *
				 * Cloasing the socket here cause the Hello to
				 * fail on sending, and this triggers the right
				 * NOT_CONNECTED condition(cleanup of the
				 * scheduler).
				 */
				close(net->sockfd);

				goto sleep;
			}

			msg = emage_msg__unpack(0, mlen, buf);

			if(msg) {
				net_process_message(net, msg);
			} else {
				EMLOG("Failed to decode the message!");
			}
		}

sleep:
		/* Relax the CPU. */
		nanosleep(&wt, &td);
	}

	EMDBG("Listening loop is terminating...\n");

	/*
	 * If you need to release 'net' specific resources, do it here!
	 */

	return 0;
}

int net_start(struct net_context * net) {
	/* 1 second interval by default. */
	net->interval = 100;

	pthread_spin_init(&net->lock, 0);

	/* Create the context where the agent scheduler will run on. */
	if(pthread_create(
		(pthread_t *)&net->thread, NULL, net_loop, net)) {

		EMLOG("Failed to create the listener agent thread.");
		return -1;
	}

	return 0;
}

int net_stop(struct net_context * net) {
	/* Stop and wait for it... */
	net->stop = 1;
	pthread_join(net->thread, 0);

	pthread_spin_destroy(&net->lock);

	return 0;
}
