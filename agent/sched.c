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
 * Empower Agent internal scheduler logic.
 */

#include <stdlib.h>
#include <time.h>

#include <emage.h>
#include <emlog.h>
#include <emage/pb/main.pb-c.h>

#include "agent.h"
#include "sched.h"

#include "msg.h"
#include "emlist.h"
#include "net.h"

/*
 * Some states when processing jobs.
 */

#define JOB_NET_ERROR			-1
#define JOB_CONSUMED			0
#define JOB_NOT_ELAPSED			1
#define JOB_RESCHEDULE			2

/* Dif "b-a" two timespec structs and return such value in ms.*/
#define ts_diff_to_ms(a, b) 			\
	(((b->tv_sec - a->tv_sec) * 1000) +	\
	 ((b->tv_nsec - a->tv_nsec) / 1000000))

/******************************************************************************
 * Utilities:                                                                 *
 ******************************************************************************/

int sched_send_msg(struct agent * a, EmageMsg * msg) {
	char * buf = 0;

	int blen = 0;
	int sent = 0;

	msg->head->seq  = net_next_seq(&a->net);

	if(msg_parse(&buf, &blen, msg)) {
		return 0;
	}

	sent = net_send(&a->net, buf, blen);

	if(sent < 0) {
		return -1;
	}

	return 0;
}

/******************************************************************************
 * Jobs(not Steve ones):                                                      *
 ******************************************************************************/

int sched_perform_send(struct agent * a, struct sched_job * job) {
	char * buf = 0;

	int blen = 0;
	int sent = 0;

	EmageMsg * msg = (EmageMsg *)job->args;

	msg->head->seq  = net_next_seq(&a->net);

	if(msg_parse(&buf, &blen, msg)) {
		emage_msg__free_unpacked(msg, 0);
		return JOB_CONSUMED;
	}

	sent = net_send(&a->net, buf, blen);

	if(sent < 0) {
		EMDBG("Sending Hello failed!");
		return JOB_NET_ERROR;
	}

	return JOB_CONSUMED;
}

int sched_perform_hello(struct agent * a, struct sched_job * job) {
	char * buf = 0;
	int blen = 0;
	int sent = 0;

	if(msg_parse_hello(net_next_seq(&a->net), a->b_id, &buf, &blen)) {
		EMLOG("Could not parse Hello message.");
		return -1;
	}

	sent = net_send(&a->net, buf, blen);

	if(sent < 0) {
		EMDBG("Sending Hello failed!");
		return JOB_NET_ERROR;
	}

	return JOB_CONSUMED;
}

int sched_perform_UEs_report(struct agent * a, struct sched_job * job) {
	EmageMsg * msg = (EmageMsg *)job->args;
	EmageMsg * reply = 0;

	/* Inform the stack about the activation of the logging mechanism. */
	if(a->ops->UEs_ID_report) {
		a->ops->UEs_ID_report(msg, &reply);
	}

	/* Something to say at the controller? */
	if(reply) {
		sched_send_msg(a, reply);
		emage_msg__free_unpacked(reply, 0);
	}

	return JOB_CONSUMED;
}

#if 0
/* Configuration of eNB. */
int sched_perform_enbr(struct agent * a, struct sched_job * job) {
	char * buf = 0;
	int blen = 0;

	EmageMsg * msg = (EmageMsg *)job->args;
	EmageMsg * rpl = 0;

	if (a->ops->eNB_config_reply) {
		if (a->ops->eNB_config_reply(msg, &rpl)) {
			EMLOG("Technology eNB configuration reply failed");
			return JOB_CONSUMED;
		}

		if (!rpl) {
			EMLOG("Reply message is 0");
			return JOB_CONSUMED;
		}

		rpl->head->seq = net_next_seq(&a->net);
		rpl->head->t_id = job->id;

		if (msg_parse(&buf, &blen, rpl)) {
			emage_msg__free_unpacked(rpl, 0);
			return JOB_CONSUMED;
		}

		if (net_send(&a->net, buf, blen) < 0) {
			EMDBG("Sending eNB configuration reply failed!");

			emage_msg__free_unpacked(rpl, 0);
			free(buf);
			return JOB_NET_ERROR;
		}

		/* Dropping unused memory*/
		emage_msg__free_unpacked(rpl, 0);
		free(buf);
	}

	EMDBG("eNB configuration reply message sent.");

	return JOB_CONSUMED;
}

/* Configuration of UE. */
int sched_perform_uer(struct agent * a, struct sched_job * job) {
	char * buf = 0;
	int blen = 0;

	EmageMsg * msg = (EmageMsg *)job->args;
	EmageMsg * rpl = 0;

	if (a->ops->UE_config_reply) {
		if (a->ops->UE_config_reply(msg, &rpl)) {
			EMLOG("Technology UE configuration reply failed");
			return JOB_CONSUMED;
		}

		if (!rpl) {
			EMLOG("Reply message is 0");
			return JOB_CONSUMED;
		}

		rpl->head->seq = net_next_seq(&a->net);
		rpl->head->t_id = job->id;

		if (msg_parse(&buf, &blen, rpl)) {
			emage_msg__free_unpacked(rpl, 0);
			return JOB_CONSUMED;
		}

		if (net_send(&a->net, buf, blen) < 0) {
			EMDBG("Sending UE configuration reply failed!");

			emage_msg__free_unpacked(rpl, 0);
			free(buf);
			return JOB_NET_ERROR;
		}

		/* Dropping unused memory*/
		emage_msg__free_unpacked(rpl, 0);
		free(buf);
	}

	EMDBG("UE configuration reply message sent.");

	return JOB_CONSUMED;
}

/* Layer 2 statistic request.*/
int sched_perform_L2sr(struct agent * a, struct sched_job * job) {
	char * buf = 0;
	int blen = 0;

	EmageMsg * msg = (EmageMsg *)job->args;
	EmageMsg * rpl = 0;

	if(a->ops->L2_stat_reply) {
		if(a->ops->L2_stat_reply(msg, &rpl)) {
			EMLOG("Technology L2 reply failed");
			return JOB_CONSUMED;
		}

		if(!rpl) {
			EMLOG("Reply message is 0");
			return JOB_CONSUMED;
		}

		rpl->head->seq  = net_next_seq(&a->net);
		rpl->head->t_id = job->id;

		if(msg_parse(&buf, &blen, rpl)) {
			emage_msg__free_unpacked(rpl, 0);
			return JOB_CONSUMED;
		}

		if(net_send(&a->net, buf, blen) < 0) {
			EMDBG("Sending L2 failed!");

			emage_msg__free_unpacked(rpl, 0);
			free(buf);
			return JOB_NET_ERROR;
		}

		/* Dropping unused memory*/
		emage_msg__free_unpacked(rpl, 0);
		free(buf);
	}

	EMDBG("L2 reply message sent.");

	return JOB_CONSUMED;
}
#endif

int sched_release_job(struct sched_job * job) {
	EmageMsg * msg = 0;

	EMDBG("Releasing a %d job", job->type);

	if(job->args) {
		if(job->type == JOB_TYPE_SEND) {
			msg = (EmageMsg *)job->args;

			emage_msg__free_unpacked(msg, 0);
			job->args = 0;
		}
	}

	free(job);

	return 0;
}

/******************************************************************************
 * Generic procedures:                                                        *
 ******************************************************************************/

int sched_add_job(struct sched_job * job, struct sched_context * sched) {
	int status = 0;

	clock_gettime(CLOCK_REALTIME, &job->issued);

/****** LOCK ******************************************************************/
	pthread_spin_lock(&sched->lock);

	/* Perform the job if the context is not stopped. */
	if(!sched->stop) {
		list_add(&job->next, &sched->jobs);
	} else {
		status = -1;
	}

	pthread_spin_unlock(&sched->lock);
/****** UNLOCK ****************************************************************/

	EMDBG("Scheduled a %d job for %d msec", job->type, job->elapse);

	return status;
}

int sched_perform_job(
	struct agent * a, struct sched_job * job, struct timespec * now) {

	int status = 0;
	struct timespec * is = &job->issued;

	/* Job not to be performed now. */
	if(ts_diff_to_ms(is, now) < job->elapse) {
		return JOB_NOT_ELAPSED;
	}

	switch(job->type) {
	case JOB_TYPE_SEND:
		status = sched_perform_send(a, job);
		break;
	case JOB_TYPE_HELLO:
		status = sched_perform_hello(a, job);
		break;
	case JOB_TYPE_UEs_LOG_TRIGGER:
		status = sched_perform_UEs_report(a, job);
		break;
#if 0
	case JOB_TYPE_L2_STAT_REQ:
		status = sched_perform_L2sr(a, job);
		break;
	case JOB_TYPE_ENB_CONFIG_REQ:
		status = sched_perform_enbr(a, job);
		break;
	case JOB_TYPE_UE_CONFIG_REQ:
		status = sched_perform_uer(a, job);
		break;
#endif
	default:
		EMDBG("Unknown job cannot be performed, type=%d", job->type);
	}

	/* The job has to be rescheduled? */
	if(status == 0 && job->reschedule != 0) {
		return JOB_RESCHEDULE;
	}

	return status;
}

int sched_consume(struct sched_context * sched) {
	struct agent * a = container_of(sched, struct agent, sched);
	struct net_context * net = &a->net;
	struct sched_job * job = 0;
	struct sched_job * tmp = 0;
	struct timespec now;

	int op = 0;
	int nj = 1;	/* New job to consume. */
	int ne = 0;	/* Network error. */

	while(nj) {
/****** LOCK ******************************************************************/
		pthread_spin_lock(&sched->lock);

		/* Nothing to to? Go to sleep. */
		if(list_empty(&sched->jobs)) {
			nj = 0;
		}

		if(nj) {
			job = list_first_entry(
				&sched->jobs,
				struct sched_job,
				next);

			list_del(&job->next);
		}

		pthread_spin_unlock(&sched->lock);
/****** UNLOCK ****************************************************************/

		/* Nothing to do... out! */
		if(!nj) {
			break;
		}

		clock_gettime(CLOCK_REALTIME, &now);

		op = sched_perform_job(a, job, &now);

/****** LOCK ******************************************************************/
		pthread_spin_lock(&sched->lock);

		/* Possible outcomes. */
		switch(op) {
		case JOB_NOT_ELAPSED:
			list_add(&job->next, &sched->todo);
			break;
		case JOB_RESCHEDULE:
			job->issued.tv_sec  = now.tv_sec;
			job->issued.tv_nsec = now.tv_nsec;
			list_add(&job->next, &sched->todo);

			/* Consume one reschedule credit. */
			if(job->reschedule > 0) {
				job->reschedule--;
			}

			break;
		case JOB_CONSUMED:
			sched_release_job(job);
			break;
		case JOB_NET_ERROR:
			sched_release_job(job);
			ne = 1;
			break;
		}

		if(ne) {
			/* Dump job to process again. */
			list_for_each_entry_safe(job, tmp, &sched->todo, next) {
				list_del(&job->next);
				sched_release_job(job);
			}

			/* Free ANY remaining job still to process. */
			list_for_each_entry_safe(job, tmp, &sched->jobs, next) {
				list_del(&job->next);
				sched_release_job(job);
			}

			pthread_spin_unlock(&sched->lock);
/****** UNLOCK ****************************************************************/

			/* Signal the connection down now.
			 * We do it here since we are sure we cleaned all the
			 * jobs, and eventual new job(from a new successful
			 * connection) don't get deleted.
			 */
			net_not_connected(net);

			return 0;
		}

		pthread_spin_unlock(&sched->lock);
/****** UNLOCK ****************************************************************/
	}

	/* All the jobs marked as to process again are moved to the official
	 * job queue.
	 */
/****** LOCK ******************************************************************/
	/* Dump all the rescheduled jobs in the queue again. */
	pthread_spin_lock(&sched->lock);
	list_for_each_entry_safe(job, tmp, &sched->todo, next) {
		list_del(&job->next);
		list_add(&job->next, &sched->jobs);
	}
	pthread_spin_unlock(&sched->lock);
/****** UNLOCK ****************************************************************/

	return 0;
}

int sched_remove_job(unsigned int id, struct sched_context * sched) {
	int found = 0;

	struct sched_job * job = 0;
	struct sched_job * tmp = 0;

	/* Dump the job from wherever it could be listed. */
/****** LOCK ******************************************************************/
	pthread_spin_lock(&sched->lock);
	list_for_each_entry_safe(job, tmp, &sched->jobs, next) {
		if(job->id == id) {
			found = 1;
			list_del(&job->next);

			break;
		}
	}

	/* Where is it? Already performed? */
	if(!found) {
		list_for_each_entry_safe(job, tmp, &sched->todo, next) {
			if(job->id == id) {
				found = 1;
				list_del(&job->next);

				break;
			}
		}
	}
	pthread_spin_unlock(&sched->lock);
/****** UNLOCK ****************************************************************/

	if(!found) {
		return -1;
	}

	EMDBG("Job %d removed from the scheduler", job->id);

	sched_release_job(job);

	return 0;
}

/******************************************************************************
 * Scheduler procedures.                                                      *
 ******************************************************************************/

void * sched_loop(void * args) {
	struct sched_context * s = (struct sched_context *)args;

	unsigned int wi = s->interval;
	struct timespec wt = {0};
	struct timespec td = {0};

	struct sched_job * job = 0;
	struct sched_job * tmp = 0;

	/* Convert the wait interval in a timespec struct. */
	while(wi >= 1000) {
		wi -= 1000;
		wt.tv_sec += 1;
	}
	wt.tv_nsec = wi * 1000000;

	EMDBG("Scheduling loop starting, interval=%d", s->interval);

	while(!s->stop) {
		/* Job scheduling logic. */
		sched_consume(s);

		/* Relax the CPU. */
		nanosleep(&wt, &td);
	}

/****** LOCK ******************************************************************/
	pthread_spin_lock(&s->lock);
	/* Dump job to process again. */
	list_for_each_entry_safe(job, tmp, &s->todo, next) {
		list_del(&job->next);
		sched_release_job(job);
	}

	/* Free ANY remaining job still to process. */
	list_for_each_entry_safe(job, tmp, &s->jobs, next) {
		list_del(&job->next);
		sched_release_job(job);
	}
	pthread_spin_unlock(&s->lock);
/****** UNLOCK ****************************************************************/

	/*
	 * If execution arrives here, then a stop has been issued.
	 */
out:
	EMDBG("Scheduling loop is terminating...\n");
	return 0;
}

int sched_start(struct sched_context * sched) {
	sched->interval = 100;

	INIT_LIST_HEAD(&sched->jobs);
	INIT_LIST_HEAD(&sched->todo);
	pthread_spin_init(&sched->lock, 0);

	/* Create the context where the agent scheduler will run on. */
	if(pthread_create(&sched->thread, NULL, sched_loop, sched)) {
		EMLOG("Failed to create the scheduler thread.");
		return -1;
	}

	return 0;
}

int sched_stop(struct sched_context * sched) {
	/* Stop and wait for it... */
	sched->stop = 1;
	pthread_join(sched->thread, 0);

	pthread_spin_destroy(&sched->lock);

	return 0;
}

