/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors: Michael Zucchi <notzed@ximian.com>
 *
 * Copyright 2002 Ximian, Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

#include <pthread.h>

#include <glib.h>

#ifdef HAVE_NSS
#include <nspr.h>
#endif

#include "e-msgport.h"

#define m(x)			/* msgport debug */
#define t(x) 			/* thread debug */
#define c(x)			/* cache debug */

void e_dlist_init(EDList *v)
{
        v->head = (EDListNode *)&v->tail;
        v->tail = 0;
        v->tailpred = (EDListNode *)&v->head;
}

EDListNode *e_dlist_addhead(EDList *l, EDListNode *n)
{
        n->next = l->head;
        n->prev = (EDListNode *)&l->head;
        l->head->prev = n;
        l->head = n;
        return n;
}

EDListNode *e_dlist_addtail(EDList *l, EDListNode *n)
{
        n->next = (EDListNode *)&l->tail;
        n->prev = l->tailpred;
        l->tailpred->next = n;
        l->tailpred = n;
        return n;
}

EDListNode *e_dlist_remove(EDListNode *n)
{
        n->next->prev = n->prev;
        n->prev->next = n->next;
        return n;
}

EDListNode *e_dlist_remhead(EDList *l)
{
	EDListNode *n, *nn;

	n = l->head;
	nn = n->next;
	if (nn) {
		nn->prev = n->prev;
		l->head = nn;
		return n;
	}
	return NULL;
}

EDListNode *e_dlist_remtail(EDList *l)
{
	EDListNode *n, *np;

	n = l->tailpred;
	np = n->prev;
	if (np) {
		np->next = n->next;
		l->tailpred = np;
		return n;
	}
	return NULL;
}

int e_dlist_empty(EDList *l)
{
	return (l->head == (EDListNode *)&l->tail);
}

int e_dlist_length(EDList *l)
{
	EDListNode *n, *nn;
	int count = 0;

	n = l->head;
	nn = n->next;
	while (nn) {
		count++;
		n = nn;
		nn = n->next;
	}

	return count;
}

struct _EMCache {
	GMutex *lock;
	GHashTable *key_table;
	EDList lru_list;
	size_t node_size;
	int node_count;
	time_t timeout;
	GFreeFunc node_free;
};

/**
 * em_cache_new:
 * @timeout: 
 * @nodesize: 
 * @nodefree: 
 * 
 * Setup a new timeout cache.  @nodesize is the size of nodes in the
 * cache, and @nodefree will be called to free YOUR content.
 * 
 * Return value: 
 **/
EMCache *
em_cache_new(time_t timeout, size_t nodesize, GFreeFunc nodefree)
{
	struct _EMCache *emc;

	emc = g_malloc0(sizeof(*emc));
	emc->node_size = nodesize;
	emc->key_table = g_hash_table_new(g_str_hash, g_str_equal);
	emc->node_free = nodefree;
	e_dlist_init(&emc->lru_list);
	emc->lock = g_mutex_new();
	emc->timeout = timeout;

	return emc;
}

/**
 * em_cache_destroy:
 * @emc: 
 * 
 * destroy the cache, duh.
 **/
void
em_cache_destroy(EMCache *emc)
{
	em_cache_clear(emc);
	g_mutex_free(emc->lock);
	g_free(emc);
}

/**
 * em_cache_lookup:
 * @emc: 
 * @key: 
 * 
 * Lookup a cache node.  once you're finished with it, you need to
 * unref it.
 * 
 * Return value: 
 **/
EMCacheNode *
em_cache_lookup(EMCache *emc, const char *key)
{
	EMCacheNode *n;

	g_mutex_lock(emc->lock);
	n = g_hash_table_lookup(emc->key_table, key);
	if (n) {
		e_dlist_remove((EDListNode *)n);
		e_dlist_addhead(&emc->lru_list, (EDListNode *)n);
		n->stamp = time(0);
		n->ref_count++;
	}
	g_mutex_unlock(emc->lock);

	c(printf("looking up '%s' %s\n", key, n?"found":"not found"));

	return n;
}

/**
 * em_cache_node_new:
 * @emc: 
 * @key: 
 * 
 * Create a new key'd cache node.  The node will not be added to the
 * cache until you insert it.
 * 
 * Return value: 
 **/
EMCacheNode *
em_cache_node_new(EMCache *emc, const char *key)
{
	EMCacheNode *n;

	/* this could use memchunks, but its probably overkill */
	n = g_malloc0(emc->node_size);
	n->key = g_strdup(key);

	return n;
}

/**
 * em_cache_node_unref:
 * @emc: 
 * @n: 
 * 
 * unref a cache node, you can only unref nodes which have been looked
 * up.
 **/
void
em_cache_node_unref(EMCache *emc, EMCacheNode *n)
{
	g_mutex_lock(emc->lock);
	g_assert(n->ref_count > 0);
	n->ref_count--;
	g_mutex_unlock(emc->lock);
}

/**
 * em_cache_add:
 * @emc: 
 * @n: 
 * 
 * Add a cache node to the cache, once added the memory is owned by
 * the cache.  If there are conflicts and the old node is still in
 * use, then the new node is not added, otherwise it is added and any
 * nodes older than the expire time are flushed.
 **/
void
em_cache_add(EMCache *emc, EMCacheNode *n)
{
	EMCacheNode *old, *prev;
	EDList old_nodes;

	e_dlist_init(&old_nodes);

	g_mutex_lock(emc->lock);
	old = g_hash_table_lookup(emc->key_table, n->key);
	if (old != NULL) {
		if (old->ref_count == 0) {
			g_hash_table_remove(emc->key_table, old->key);
			e_dlist_remove((EDListNode *)old);
			e_dlist_addtail(&old_nodes, (EDListNode *)old);
			goto insert;
		} else {
			e_dlist_addtail(&old_nodes, (EDListNode *)n);
		}
	} else {
		time_t now;
	insert:
		now = time(0);
		g_hash_table_insert(emc->key_table, n->key, n);
		e_dlist_addhead(&emc->lru_list, (EDListNode *)n);
		n->stamp = now;
		emc->node_count++;

		c(printf("inserting node %s\n", n->key));

		old = (EMCacheNode *)emc->lru_list.tailpred;
		prev = old->prev;
		while (prev && old->stamp < now - emc->timeout) {
			if (old->ref_count == 0) {
				c(printf("expiring node %s\n", old->key));
				g_hash_table_remove(emc->key_table, old->key);
				e_dlist_remove((EDListNode *)old);
				e_dlist_addtail(&old_nodes, (EDListNode *)old);
			}
			old = prev;
			prev = prev->prev;
		}
	}

	g_mutex_unlock(emc->lock);

	while ((old = (EMCacheNode *)e_dlist_remhead(&old_nodes))) {
		emc->node_free(old);
		g_free(old->key);
		g_free(old);
	}
}

/**
 * em_cache_clear:
 * @emc: 
 * 
 * clear the cache.  just for api completeness.
 **/
void
em_cache_clear(EMCache *emc)
{
	EMCacheNode *node;
	EDList old_nodes;

	e_dlist_init(&old_nodes);
	g_mutex_lock(emc->lock);
	while ((node = (EMCacheNode *)e_dlist_remhead(&emc->lru_list)))
		e_dlist_addtail(&old_nodes, (EDListNode *)node);
	g_mutex_unlock(emc->lock);

	while ((node = (EMCacheNode *)e_dlist_remhead(&old_nodes))) {
		emc->node_free(node);
		g_free(node->key);
		g_free(node);
	}
}

struct _EMsgPort {
	EDList queue;
	int condwait;		/* how many waiting in condwait */
	union {
		int pipe[2];
		struct {
			int read;
			int write;
		} fd;
	} pipe;
#ifdef HAVE_NSS
	struct {
		PRFileDesc *read;
		PRFileDesc *write;
	} prpipe;
#endif
	/* @#@$#$ glib stuff */
	GCond *cond;
	GMutex *lock;
};

EMsgPort *e_msgport_new(void)
{
	EMsgPort *mp;

	mp = g_malloc(sizeof(*mp));
	e_dlist_init(&mp->queue);
	mp->lock = g_mutex_new();
	mp->cond = g_cond_new();
	mp->pipe.fd.read = -1;
	mp->pipe.fd.write = -1;
#ifdef HAVE_NSS
	mp->prpipe.read = NULL;
	mp->prpipe.write = NULL;
#endif
	mp->condwait = 0;

	return mp;
}

void e_msgport_destroy(EMsgPort *mp)
{
	g_mutex_free(mp->lock);
	g_cond_free(mp->cond);
	if (mp->pipe.fd.read != -1) {
		close(mp->pipe.fd.read);
		close(mp->pipe.fd.write);
	}
#ifdef HAVE_NSS
	if (mp->prpipe.read) {
		PR_Close(mp->prpipe.read);
		PR_Close(mp->prpipe.write);
	}
#endif
	g_free(mp);
}

/* get a fd that can be used to wait on the port asynchronously */
int e_msgport_fd(EMsgPort *mp)
{
	int fd;

	g_mutex_lock(mp->lock);
	fd = mp->pipe.fd.read;
	if (fd == -1) {
		pipe(mp->pipe.pipe);
		fd = mp->pipe.fd.read;
	}
	g_mutex_unlock(mp->lock);

	return fd;
}

#ifdef HAVE_NSS
PRFileDesc *e_msgport_prfd(EMsgPort *mp)
{
	PRFileDesc *fd;

	g_mutex_lock(mp->lock);
	fd = mp->prpipe.read;
	if (fd == NULL) {
		PR_CreatePipe(&mp->prpipe.read, &mp->prpipe.write);
		fd = mp->prpipe.read;
	}
	g_mutex_unlock(mp->lock);

	return fd;
}
#endif

void e_msgport_put(EMsgPort *mp, EMsg *msg)
{
	int fd;
#ifdef HAVE_NSS
	PRFileDesc *prfd;
#endif

	m(printf("put:\n"));
	g_mutex_lock(mp->lock);
	e_dlist_addtail(&mp->queue, &msg->ln);
	if (mp->condwait > 0) {
		m(printf("put: condwait > 0, waking up\n"));
		g_cond_signal(mp->cond);
	}
	fd = mp->pipe.fd.write;
#ifdef HAVE_NSS
	prfd = mp->prpipe.write;
#endif
	g_mutex_unlock(mp->lock);

	if (fd != -1) {
		m(printf("put: have pipe, writing notification to it\n"));
		write(fd, "", 1);
	}

#ifdef HAVE_NSS
	if (prfd != NULL) {
		m(printf("put: have pr pipe, writing notification to it\n"));
		PR_Write(prfd, "", 1);
	}
#endif
	m(printf("put: done\n"));
}

static void
msgport_cleanlock(void *data)
{
	EMsgPort *mp = data;

	g_mutex_unlock(mp->lock);
}

EMsg *e_msgport_wait(EMsgPort *mp)
{
	EMsg *msg;

	m(printf("wait:\n"));
	g_mutex_lock(mp->lock);
	while (e_dlist_empty(&mp->queue)) {
		if (mp->pipe.fd.read != -1) {
			fd_set rfds;
			int retry;

			m(printf("wait: waitng on pipe\n"));
			g_mutex_unlock(mp->lock);
			do {
				FD_ZERO(&rfds);
				FD_SET(mp->pipe.fd.read, &rfds);
				retry = select(mp->pipe.fd.read+1, &rfds, NULL, NULL, NULL) == -1 && errno == EINTR;
				pthread_testcancel();
			} while (retry);
			g_mutex_lock(mp->lock);
			m(printf("wait: got pipe\n"));
#ifdef HAVE_NSS
		} else if (mp->prpipe.read != NULL) {
			PRPollDesc polltable[1];
			int retry;

			m(printf("wait: waitng on pr pipe\n"));
			g_mutex_unlock(mp->lock);
			do {
				polltable[0].fd = mp->prpipe.read;
				polltable[0].in_flags = PR_POLL_READ|PR_POLL_ERR;
				retry = PR_Poll(polltable, 1, PR_INTERVAL_NO_TIMEOUT) == -1 && PR_GetError() == PR_PENDING_INTERRUPT_ERROR;
				pthread_testcancel();
			} while (retry);
			g_mutex_lock(mp->lock);
			m(printf("wait: got pr pipe\n"));
#endif /* HAVE_NSS */
		} else {
			m(printf("wait: waiting on condition\n"));
			mp->condwait++;
			/* if we are cancelled in the cond-wait, then we need to unlock our lock when we cleanup */
			pthread_cleanup_push(msgport_cleanlock, mp);
			g_cond_wait(mp->cond, mp->lock);
			pthread_cleanup_pop(0);
			m(printf("wait: got condition\n"));
			mp->condwait--;
		}
	}
	msg = (EMsg *)mp->queue.head;
	m(printf("wait: message = %p\n", msg));
	g_mutex_unlock(mp->lock);
	m(printf("wait: done\n"));
	return msg;
}

EMsg *e_msgport_get(EMsgPort *mp)
{
	EMsg *msg;
	char dummy[1];

	g_mutex_lock(mp->lock);
	msg = (EMsg *)e_dlist_remhead(&mp->queue);
	if (msg) {
		if (mp->pipe.fd.read != -1)
			read(mp->pipe.fd.read, dummy, 1);
#ifdef HAVE_NSS
		if (mp->prpipe.read != NULL) {
			int c;
			c = PR_Read(mp->prpipe.read, dummy, 1);
			g_assert(c == 1);
		}
#endif
	}
	m(printf("get: message = %p\n", msg));
	g_mutex_unlock(mp->lock);

	return msg;
}

void e_msgport_reply(EMsg *msg)
{
	if (msg->reply_port) {
		e_msgport_put(msg->reply_port, msg);
	}
	/* else lost? */
}

struct _thread_info {
	pthread_t id;
	int busy;
};

struct _EThread {
	struct _EThread *next;
	struct _EThread *prev;

	EMsgPort *server_port;
	EMsgPort *reply_port;
	pthread_mutex_t mutex;
	e_thread_t type;
	int queue_limit;

	int waiting;		/* if we are waiting for a new message, count of waiting processes */
	pthread_t id;		/* id of our running child thread */
	GList *id_list;		/* if THREAD_NEW, then a list of our child threads in thread_info structs */

	EThreadFunc destroy;
	void *destroy_data;

	EThreadFunc received;
	void *received_data;

	EThreadFunc lost;
	void *lost_data;
};

/* All active threads */
static EDList ethread_list = E_DLIST_INITIALISER(ethread_list);
static pthread_mutex_t ethread_lock = PTHREAD_MUTEX_INITIALIZER;

#define E_THREAD_NONE ((pthread_t)~0)
#define E_THREAD_QUIT_REPLYPORT ((struct _EMsgPort *)~0)

static void thread_destroy_msg(EThread *e, EMsg *m);

static struct _thread_info *thread_find(EThread *e, pthread_t id)
{
	GList *node;
	struct _thread_info *info;

	node = e->id_list;
	while (node) {
		info = node->data;
		if (info->id == id)
			return info;
		node = node->next;
	}
	return NULL;
}

#if 0
static void thread_remove(EThread *e, pthread_t id)
{
	GList *node;
	struct _thread_info *info;

	node = e->id_list;
	while (node) {
		info = node->data;
		if (info->id == id) {
			e->id_list = g_list_remove(e->id_list, info);
			g_free(info);
		}
		node = node->next;
	}
}
#endif

EThread *e_thread_new(e_thread_t type)
{
	EThread *e;

	e = g_malloc0(sizeof(*e));
	pthread_mutex_init(&e->mutex, 0);
	e->type = type;
	e->server_port = e_msgport_new();
	e->id = E_THREAD_NONE;
	e->queue_limit = INT_MAX;

	pthread_mutex_lock(&ethread_lock);
	e_dlist_addtail(&ethread_list, (EDListNode *)e);
	pthread_mutex_unlock(&ethread_lock);

	return e;
}

/* close down the threads & resources etc */
void e_thread_destroy(EThread *e)
{
	int busy = FALSE;
	EMsg *msg;
	struct _thread_info *info;
	GList *l;

	/* make sure we soak up all the messages first */
	while ( (msg = e_msgport_get(e->server_port)) ) {
		thread_destroy_msg(e, msg);
	}

	pthread_mutex_lock(&e->mutex);

	switch(e->type) {
	case E_THREAD_QUEUE:
	case E_THREAD_DROP:
		/* if we have a thread, 'kill' it */
		if (e->id != E_THREAD_NONE) {
			pthread_t id = e->id;

			t(printf("Sending thread '%d' quit message\n", id));

			e->id = E_THREAD_NONE;

			msg = g_malloc0(sizeof(*msg));
			msg->reply_port = E_THREAD_QUIT_REPLYPORT;
			e_msgport_put(e->server_port, msg);

			pthread_mutex_unlock(&e->mutex);
			t(printf("Joining thread '%d'\n", id));
			pthread_join(id, 0);
			t(printf("Joined thread '%d'!\n", id));
			pthread_mutex_lock(&e->mutex);
		}
		busy = e->id != E_THREAD_NONE;
		break;
	case E_THREAD_NEW:
		/* first, send everyone a quit message */
		l = e->id_list;
		while (l) {
			info = l->data;
			t(printf("Sending thread '%d' quit message\n", info->id));
			msg = g_malloc0(sizeof(*msg));
			msg->reply_port = E_THREAD_QUIT_REPLYPORT;
			e_msgport_put(e->server_port, msg);
			l = l->next;			
		}

		/* then, wait for everyone to quit */
		while (e->id_list) {
			info = e->id_list->data;
			e->id_list = g_list_remove(e->id_list, info);
			pthread_mutex_unlock(&e->mutex);
			t(printf("Joining thread '%d'\n", info->id));
			pthread_join(info->id, 0);
			t(printf("Joined thread '%d'!\n", info->id));
			pthread_mutex_lock(&e->mutex);
			g_free(info);
		}
		busy = g_list_length(e->id_list) != 0;
		break;
	}

	pthread_mutex_unlock(&e->mutex);

	/* and clean up, if we can */
	if (busy) {
		g_warning("threads were busy, leaked EThread");
		return;
	}

	pthread_mutex_lock(&ethread_lock);
	e_dlist_remove((EDListNode *)e);
	pthread_mutex_unlock(&ethread_lock);

	pthread_mutex_destroy(&e->mutex);
	e_msgport_destroy(e->server_port);
	g_free(e);
}

/* set the queue maximum depth, what happens when the queue
   fills up depends on the queue type */
void e_thread_set_queue_limit(EThread *e, int limit)
{
	e->queue_limit = limit;
}

/* set a msg destroy callback, this can not call any e_thread functions on @e */
void e_thread_set_msg_destroy(EThread *e, EThreadFunc destroy, void *data)
{
	pthread_mutex_lock(&e->mutex);
	e->destroy = destroy;
	e->destroy_data = data;
	pthread_mutex_unlock(&e->mutex);
}

/* set a message lost callback, called if any message is discarded */
void e_thread_set_msg_lost(EThread *e, EThreadFunc lost, void *data)
{
	pthread_mutex_lock(&e->mutex);
	e->lost = lost;
	e->lost_data = lost;
	pthread_mutex_unlock(&e->mutex);
}

/* set a reply port, if set, then send messages back once finished */
void e_thread_set_reply_port(EThread *e, EMsgPort *reply_port)
{
	e->reply_port = reply_port;
}

/* set a received data callback */
void e_thread_set_msg_received(EThread *e, EThreadFunc received, void *data)
{
	pthread_mutex_lock(&e->mutex);
	e->received = received;
	e->received_data = data;
	pthread_mutex_unlock(&e->mutex);
}

/* find out if we're busy doing any work, e==NULL, check for all work */
int e_thread_busy(EThread *e)
{
	int busy = FALSE;

	if (e == NULL) {
		pthread_mutex_lock(&ethread_lock);
		e = (EThread *)ethread_list.head;
		while (e->next && !busy) {
			busy = e_thread_busy(e);
			e = e->next;
		}
		pthread_mutex_unlock(&ethread_lock);
	} else {
		pthread_mutex_lock(&e->mutex);
		switch (e->type) {
		case E_THREAD_QUEUE:
		case E_THREAD_DROP:
			busy = e->waiting != 1 && e->id != E_THREAD_NONE;
			break;
		case E_THREAD_NEW:
			busy = e->waiting != g_list_length(e->id_list);
			break;
		}
		pthread_mutex_unlock(&e->mutex);
	}

	return busy;
}

static void
thread_destroy_msg(EThread *e, EMsg *m)
{
	EThreadFunc func;
	void *func_data;

	/* we do this so we never get an incomplete/unmatched callback + data */
	pthread_mutex_lock(&e->mutex);
	func = e->destroy;
	func_data = e->destroy_data;
	pthread_mutex_unlock(&e->mutex);
	
	if (func)
		func(e, m, func_data);
}

static void
thread_received_msg(EThread *e, EMsg *m)
{
	EThreadFunc func;
	void *func_data;

	/* we do this so we never get an incomplete/unmatched callback + data */
	pthread_mutex_lock(&e->mutex);
	func = e->received;
	func_data = e->received_data;
	pthread_mutex_unlock(&e->mutex);
	
	if (func)
		func(e, m, func_data);
	else
		g_warning("No processing callback for EThread, message unprocessed");
}

static void
thread_lost_msg(EThread *e, EMsg *m)
{
	EThreadFunc func;
	void *func_data;

	/* we do this so we never get an incomplete/unmatched callback + data */
	pthread_mutex_lock(&e->mutex);
	func = e->lost;
	func_data = e->lost_data;
	pthread_mutex_unlock(&e->mutex);
	
	if (func)
		func(e, m, func_data);
}

/* the actual thread dispatcher */
static void *
thread_dispatch(void *din)
{
	EThread *e = din;
	EMsg *m;
	struct _thread_info *info;
	pthread_t self = pthread_self();

	t(printf("dispatch thread started: %ld\n", pthread_self()));

	while (1) {
		pthread_mutex_lock(&e->mutex);
		m = e_msgport_get(e->server_port);
		if (m == NULL) {
			/* nothing to do?  If we are a 'new' type thread, just quit.
			   Otherwise, go into waiting (can be cancelled here) */
			info = NULL;
			switch (e->type) {
			case E_THREAD_NEW:
			case E_THREAD_QUEUE:
			case E_THREAD_DROP:
				info = thread_find(e, self);
				if (info)
					info->busy = FALSE;
				e->waiting++;
				pthread_mutex_unlock(&e->mutex);
				e_msgport_wait(e->server_port);
				pthread_mutex_lock(&e->mutex);
				e->waiting--;
				pthread_mutex_unlock(&e->mutex);
				break;
#if 0
			case E_THREAD_NEW:
				e->id_list = g_list_remove(e->id_list, (void *)pthread_self());
				pthread_mutex_unlock(&e->mutex);
				return 0;
#endif
			}

			continue;
		} else if (m->reply_port == E_THREAD_QUIT_REPLYPORT) {
			t(printf("Thread %d got quit message\n", self));
			/* Handle a quit message, say we're quitting, free the message, and break out of the loop */
			info = thread_find(e, self);
			if (info)
				info->busy = 2;
			pthread_mutex_unlock(&e->mutex);
			g_free(m);
			break;
		} else {
			info = thread_find(e, self);
			if (info)
				info->busy = TRUE;
		}
		pthread_mutex_unlock(&e->mutex);

		t(printf("got message in dispatch thread\n"));

		/* process it */
		thread_received_msg(e, m);

		/* if we have a reply port, send it back, otherwise, lose it */
		if (m->reply_port) {
			e_msgport_reply(m);
		} else {
			thread_destroy_msg(e, m);
		}
	}

	return NULL;
}

/* send a message to the thread, start thread if necessary */
void e_thread_put(EThread *e, EMsg *msg)
{
	pthread_t id;
	EMsg *dmsg = NULL;

	pthread_mutex_lock(&e->mutex);

	/* the caller forgot to tell us what to do, well, we can't do anything can we */
	if (e->received == NULL) {
		pthread_mutex_unlock(&e->mutex);
		g_warning("EThread called with no receiver function, no work to do!");
		thread_destroy_msg(e, msg);
		return;
	}

	msg->reply_port = e->reply_port;

	switch(e->type) {
	case E_THREAD_QUEUE:
		/* if the queue is full, lose this new addition */
		if (e_dlist_length(&e->server_port->queue) < e->queue_limit) {
			e_msgport_put(e->server_port, msg);
		} else {
			printf("queue limit reached, dropping new message\n");
			dmsg = msg;
		}
		break;
	case E_THREAD_DROP:
		/* if the queue is full, lose the oldest (unprocessed) message */
		if (e_dlist_length(&e->server_port->queue) < e->queue_limit) {
			e_msgport_put(e->server_port, msg);
		} else {
			printf("queue limit reached, dropping old message\n");
			e_msgport_put(e->server_port, msg);
			dmsg = e_msgport_get(e->server_port);
		}
		break;
	case E_THREAD_NEW:
		/* it is possible that an existing thread can catch this message, so
		   we might create a thread with no work to do.
		   but that doesn't matter, the other alternative that it be lost is worse */
		e_msgport_put(e->server_port, msg);
		if (e->waiting == 0
		    && g_list_length(e->id_list) < e->queue_limit
		    && pthread_create(&id, NULL, thread_dispatch, e) == 0) {
			struct _thread_info *info = g_malloc0(sizeof(*info));
			t(printf("created NEW thread %ld\n", id));
			info->id = id;
			info->busy = TRUE;
			e->id_list = g_list_append(e->id_list, info);
		}
		pthread_mutex_unlock(&e->mutex);
		return;
	}

	/* create the thread, if there is none to receive it yet */
	if (e->id == E_THREAD_NONE) {
		int err;

		if ((err = pthread_create(&e->id, NULL, thread_dispatch, e)) != 0) {
			g_warning("Could not create dispatcher thread, message queued?: %s", strerror(err));
			e->id = E_THREAD_NONE;
		}
	}

	pthread_mutex_unlock(&e->mutex);

	if (dmsg) {
		thread_lost_msg(e, dmsg);
		thread_destroy_msg(e, dmsg);
	}
}

/* yet-another-mutex interface */
struct _EMutex {
	int type;
	pthread_t owner;
	short waiters;
	short depth;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
};

/* sigh, this is just painful to have to need, but recursive
   read/write, etc mutexes just aren't very common in thread
   implementations */
/* TODO: Just make it use recursive mutexes if they are available */
EMutex *e_mutex_new(e_mutex_t type)
{
	struct _EMutex *m;

	m = g_malloc(sizeof(*m));
	m->type = type;
	m->waiters = 0;
	m->depth = 0;
	m->owner = E_THREAD_NONE;

	switch (type) {
	case E_MUTEX_SIMPLE:
		pthread_mutex_init(&m->mutex, 0);
		break;
	case E_MUTEX_REC:
		pthread_mutex_init(&m->mutex, 0);
		pthread_cond_init(&m->cond, 0);
		break;
		/* read / write ?  flags for same? */
	}

	return m;
}

int e_mutex_destroy(EMutex *m)
{
	int ret = 0;

	switch (m->type) {
	case E_MUTEX_SIMPLE:
		ret = pthread_mutex_destroy(&m->mutex);
		if (ret == -1)
			g_warning("EMutex destroy failed: %s", strerror(errno));
		g_free(m);
		break;
	case E_MUTEX_REC:
		ret = pthread_mutex_destroy(&m->mutex);
		if (ret == -1)
			g_warning("EMutex destroy failed: %s", strerror(errno));
		ret = pthread_cond_destroy(&m->cond);
		if (ret == -1)
			g_warning("EMutex destroy failed: %s", strerror(errno));
		g_free(m);

	}
	return ret;
}

int e_mutex_lock(EMutex *m)
{
	pthread_t id;
	int err;

	switch (m->type) {
	case E_MUTEX_SIMPLE:
		return pthread_mutex_lock(&m->mutex);
	case E_MUTEX_REC:
		id = pthread_self();
		if ((err = pthread_mutex_lock(&m->mutex)) != 0)
			return err;
		while (1) {
			if (m->owner == E_THREAD_NONE) {
				m->owner = id;
				m->depth = 1;
				break;
			} else if (id == m->owner) {
				m->depth++;
				break;
			} else {
				m->waiters++;
				if ((err = pthread_cond_wait(&m->cond, &m->mutex)) != 0)
					return err;
				m->waiters--;
			}
		}
		return pthread_mutex_unlock(&m->mutex);
	}

	return EINVAL;
}

int e_mutex_unlock(EMutex *m)
{
	int err;

	switch (m->type) {
	case E_MUTEX_SIMPLE:
		return pthread_mutex_unlock(&m->mutex);
	case E_MUTEX_REC:
		if ((err = pthread_mutex_lock(&m->mutex)) != 0)
			return err;
		g_assert(m->owner == pthread_self());

		m->depth--;
		if (m->depth == 0) {
			m->owner = E_THREAD_NONE;
			if (m->waiters > 0)
				pthread_cond_signal(&m->cond);
		}
		return pthread_mutex_unlock(&m->mutex);
	}

	errno = EINVAL;
	return -1;
}

void e_mutex_assert_locked(EMutex *m)
{
	g_return_if_fail (m->type == E_MUTEX_REC);
	pthread_mutex_lock(&m->mutex);
	g_assert(m->owner == pthread_self());
	pthread_mutex_unlock(&m->mutex);
}

int e_mutex_cond_wait(void *vcond, EMutex *m)
{
	int ret;
	pthread_cond_t *cond = vcond;

	switch(m->type) {
	case E_MUTEX_SIMPLE:
		return pthread_cond_wait(cond, &m->mutex);
	case E_MUTEX_REC:
		if ((ret = pthread_mutex_lock(&m->mutex)) != 0)
			return ret;
		g_assert(m->owner == pthread_self());
		ret = pthread_cond_wait(cond, &m->mutex);
		g_assert(m->owner == pthread_self());
		pthread_mutex_unlock(&m->mutex);
		return ret;
	default:
		g_return_val_if_reached(-1);
	}
}

#ifdef STANDALONE
EMsgPort *server_port;


void *fdserver(void *data)
{
	int fd;
	EMsg *msg;
	int id = (int)data;
	fd_set rfds;

	fd = e_msgport_fd(server_port);

	while (1) {
		int count = 0;

		printf("server %d: waiting on fd %d\n", id, fd);
		FD_ZERO(&rfds);
		FD_SET(fd, &rfds);
		select(fd+1, &rfds, NULL, NULL, NULL);
		printf("server %d: Got async notification, checking for messages\n", id);
		while ((msg = e_msgport_get(server_port))) {
			printf("server %d: got message\n", id);
			sleep(1);
			printf("server %d: replying\n", id);
			e_msgport_reply(msg);
			count++;
		}
		printf("server %d: got %d messages\n", id, count);
	}
}

void *server(void *data)
{
	EMsg *msg;
	int id = (int)data;

	while (1) {
		printf("server %d: waiting\n", id);
		msg = e_msgport_wait(server_port);
		msg = e_msgport_get(server_port);
		if (msg) {
			printf("server %d: got message\n", id);
			sleep(1);
			printf("server %d: replying\n", id);
			e_msgport_reply(msg);
		} else {
			printf("server %d: didn't get message\n", id);
		}
	}
}

void *client(void *data)
{
	EMsg *msg;
	EMsgPort *replyport;
	int i;

	replyport = e_msgport_new();
	msg = g_malloc0(sizeof(*msg));
	msg->reply_port = replyport;
	for (i=0;i<10;i++) {
		/* synchronous operation */
		printf("client: sending\n");
		e_msgport_put(server_port, msg);
		printf("client: waiting for reply\n");
		e_msgport_wait(replyport);
		e_msgport_get(replyport);
		printf("client: got reply\n");
	}

	printf("client: sleeping ...\n");
	sleep(2);
	printf("client: sending multiple\n");

	for (i=0;i<10;i++) {
		msg = g_malloc0(sizeof(*msg));
		msg->reply_port = replyport;
		e_msgport_put(server_port, msg);
	}

	printf("client: receiving multiple\n");
	for (i=0;i<10;i++) {
		e_msgport_wait(replyport);
		msg = e_msgport_get(replyport);
		g_free(msg);
	}

	printf("client: done\n");
}

int main(int argc, char **argv)
{
	pthread_t serverid, clientid;

	g_thread_init(NULL);

	server_port = e_msgport_new();

	/*pthread_create(&serverid, NULL, server, (void *)1);*/
	pthread_create(&serverid, NULL, fdserver, (void *)1);
	pthread_create(&clientid, NULL, client, NULL);

	sleep(60);

	return 0;
}
#endif
