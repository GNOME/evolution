

#include <glib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <stdio.h>

#include "e-msgport.h"

#include <pthread.h>

#define m(x)			/* msgport debug */
#define t(x) x			/* thread debug */

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

	return 0;
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

void e_msgport_put(EMsgPort *mp, EMsg *msg)
{
	m(printf("put:\n"));
	g_mutex_lock(mp->lock);
	e_dlist_addtail(&mp->queue, &msg->ln);
	if (mp->condwait > 0) {
		m(printf("put: condwait > 0, waking up\n"));
		g_cond_signal(mp->cond);
	}
	if (mp->pipe.fd.write != -1) {
		m(printf("put: have pipe, writing notification to it\n"));
		write(mp->pipe.fd.write, "", 1);
	}
	g_mutex_unlock(mp->lock);
	m(printf("put: done\n"));
}

EMsg *e_msgport_wait(EMsgPort *mp)
{
	EMsg *msg;

	m(printf("wait:\n"));
	g_mutex_lock(mp->lock);
	while (e_dlist_empty(&mp->queue)) {
		if (mp->pipe.fd.read == -1) {
			m(printf("wait: waiting on condition\n"));
			mp->condwait++;
			g_cond_wait(mp->cond, mp->lock);
			m(printf("wait: got condition\n"));
			mp->condwait--;
		} else {
			fd_set rfds;

			m(printf("wait: waitng on pipe\n"));
			FD_ZERO(&rfds);
			FD_SET(mp->pipe.fd.read, &rfds);
			g_mutex_unlock(mp->lock);
			select(mp->pipe.fd.read+1, &rfds, NULL, NULL, NULL);
			pthread_testcancel();
			g_mutex_lock(mp->lock);
			m(printf("wait: got pipe\n"));
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
	if (msg && mp->pipe.fd.read != -1)
		read(mp->pipe.fd.read, dummy, 1);
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

#define E_THREAD_NONE ((pthread_t)~0)

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

	return e;
}

/* close down the threads & resources etc */
void e_thread_destroy(EThread *e)
{
	int tries = 0;
	int busy = FALSE;
	EMsg *msg;
	struct _thread_info *info;

	/* make sure we soak up all the messages first */
	while ( (msg = e_msgport_get(e->server_port)) ) {
		thread_destroy_msg(e, msg);
	}

	pthread_mutex_lock(&e->mutex);

	switch(e->type) {
	case E_THREAD_QUEUE:
	case E_THREAD_DROP:
		/* if we have a thread, 'kill' it */
		while (e->id != E_THREAD_NONE && tries < 5) {
			if (e->waiting > 0) {
				pthread_t id = e->id;
				e->id = E_THREAD_NONE;
				pthread_mutex_unlock(&e->mutex);
				if (pthread_cancel(id) == 0)
					pthread_join(id, 0);
				pthread_mutex_lock(&e->mutex);
			} else {
				(printf("thread still active, waiting for it to finish\n"));
				pthread_mutex_unlock(&e->mutex);
				sleep(1);
				pthread_mutex_lock(&e->mutex);
			}
			tries++;
		}
		busy = e->id != E_THREAD_NONE;
		break;
	case E_THREAD_NEW:
		while (e->id_list && tries < 5) {
			info = e->id_list->data;
			if (!info->busy) {
				e->id_list = g_list_remove(e->id_list, info);
				printf("cleaning up pool thread %d\n", info->id);
				pthread_mutex_unlock(&e->mutex);
				if (pthread_cancel(info->id) == 0)
					/*pthread_join(info->id, 0);*/
				pthread_mutex_lock(&e->mutex);
				printf("cleaned up ok\n");
				g_free(info);
			} else {
				(printf("thread(s) still active, waiting for it to finish\n"));
				tries++;
				pthread_mutex_unlock(&e->mutex);
				sleep(1);
				pthread_mutex_lock(&e->mutex);
			}
		}
#if 0
		while (g_list_length(e->id_list) && tries < 5) {
			(printf("thread(s) still active, waiting for them to finish\n"));
			pthread_mutex_unlock(&e->mutex);
			sleep(1);
			pthread_mutex_lock(&e->mutex);
		}
#endif
		busy = g_list_length(e->id_list) != 0;
		break;
	}

	pthread_mutex_unlock(&e->mutex);

	/* and clean up, if we can */
	if (busy) {
		g_warning("threads were busy, leaked EThread");
		return;
	}

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
				info = thread_find(e, pthread_self());
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
		} else {
			info = thread_find(e, pthread_self());
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

	/* if we run out of things to process we could conceivably 'hang around' for a bit,
	   but to do this we need to use the fd interface of the msgport, and its utility
	   is probably debatable anyway */

#if 0
	/* signify we are no longer running */
	/* This code isn't used yet, but would be if we ever had a 'quit now' message implemented */
	pthread_mutex_lock(&e->mutex);
	switch (e->type) {
	case E_THREAD_QUEUE:
	case E_THREAD_DROP:
		e->id = E_THREAD_NONE;
		break;
	case E_THREAD_NEW:
		e->id_list = g_list_remove(e->id_list, (void *)pthread_self());
		break;
	}
	pthread_mutex_unlock(&e->mutex);
#endif

	return 0;
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
			printf("created NEW thread %ld\n", id);
			info->id = id;
			info->busy = TRUE;
			e->id_list = g_list_append(e->id_list, info);
		}
		pthread_mutex_unlock(&e->mutex);
		return;
	}

	/* create the thread, if there is none to receive it yet */
	if (e->id == E_THREAD_NONE) {
		if (pthread_create(&e->id, NULL, thread_dispatch, e) == -1) {
			g_warning("Could not create dispatcher thread, message queued?: %s", strerror(errno));
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

	switch (m->type) {
	case E_MUTEX_SIMPLE:
		return pthread_mutex_lock(&m->mutex);
	case E_MUTEX_REC:
		id = pthread_self();
		if (pthread_mutex_lock(&m->mutex) == -1)
			return -1;
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
				if (pthread_cond_wait(&m->cond, &m->mutex) == -1)
					return -1;
				m->waiters--;
			}
		}
		return pthread_mutex_unlock(&m->mutex);
	}

	errno = EINVAL;
	return -1;
}

int e_mutex_unlock(EMutex *m)
{
	switch (m->type) {
	case E_MUTEX_SIMPLE:
		return pthread_mutex_unlock(&m->mutex);
	case E_MUTEX_REC:
		if (pthread_mutex_lock(&m->mutex) == -1)
			return -1;
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
