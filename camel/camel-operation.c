#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#ifdef ENABLE_THREADS
#include <pthread.h>
#endif

#include <glib.h>
#include "camel-operation.h"
#include "e-util/e-msgport.h"

#define d(x)

/* ********************************************************************** */

struct _CamelOperation {
	pthread_t id;		/* id of running thread */
	guint32 flags;		/* cancelled ? */
	int blocked;		/* cancellation blocked depth */
	int refcount;

	CamelOperationStatusFunc status;
	void *status_data;
	time_t status_update;

	/* stack of status messages (char *) */
	GSList *status_stack;

#ifdef ENABLE_THREADS
	EMsgPort *cancel_port;
	int cancel_fd;
	pthread_mutex_t lock;
#endif
};

#define CAMEL_OPERATION_CANCELLED (1<<0)

#ifdef ENABLE_THREADS
#define CAMEL_OPERATION_LOCK(cc) pthread_mutex_lock(&cc->lock)
#define CAMEL_OPERATION_UNLOCK(cc) pthread_mutex_unlock(&cc->lock)
#define CAMEL_ACTIVE_LOCK() pthread_mutex_lock(&operation_active_lock)
#define CAMEL_ACTIVE_UNLOCK() pthread_mutex_unlock(&operation_active_lock)
static pthread_mutex_t operation_active_lock = PTHREAD_MUTEX_INITIALIZER;
#else
#define CAMEL_OPERATION_LOCK(cc)
#define CAMEL_OPERATION_UNLOCK(cc)
#define CAMEL_ACTIVE_LOCK()
#define CAMEL_ACTIVE_UNLOCK()
#endif

static GHashTable *operation_active;

typedef struct _CamelOperationMsg {
	EMsg msg;
} CamelOperationMsg ;

/**
 * camel_operation_new:
 * @status: Callback for receiving status messages.
 * @status_data: User data.
 * 
 * Create a new camel operation handle.  Camel operation handles can
 * be used in a multithreaded application (or a single operation
 * handle can be used in a non threaded appliation) to cancel running
 * operations and to obtain notification messages of the internal
 * status of messages.
 * 
 * Return value: A new operation handle.
 **/
CamelOperation *camel_operation_new(CamelOperationStatusFunc status, void *status_data)
{
	CamelOperation *cc;

	cc = g_malloc0(sizeof(*cc));

	cc->flags = 0;
	cc->blocked = 0;
	cc->refcount = 1;
	cc->status = status;
	cc->status_data = status_data;
#ifdef ENABLE_THREADS
	cc->id = ~0;
	cc->cancel_port = e_msgport_new();
	cc->cancel_fd = e_msgport_fd(cc->cancel_port);
	pthread_mutex_init(&cc->lock, NULL);
#endif

	return cc;
}

/**
 * camel_operation_reset:
 * @cc: 
 * 
 * Resets an operation cancel state and message.
 **/
void camel_operation_reset(CamelOperation *cc)
{
	GSList *n;

#ifdef ENABLE_THREADS
	CamelOperationMsg *msg;

	while ((msg = (CamelOperationMsg *)e_msgport_get(cc->cancel_port)))
		g_free(msg);
#endif

	n = cc->status_stack;
	while (n) {
		g_free(n->data);
		n = n->next;
	}
	g_slist_free(cc->status_stack);
	cc->status_stack = NULL;

	cc->flags = 0;
	cc->blocked = 0;
}

/**
 * camel_operation_ref:
 * @cc: 
 * 
 * Add a reference to the CamelOperation @cc.
 **/
void camel_operation_ref(CamelOperation *cc)
{
	CAMEL_OPERATION_LOCK(cc);
	cc->refcount++;
	CAMEL_OPERATION_UNLOCK(cc);
}

/**
 * camel_operation_unref:
 * @cc: 
 * 
 * Unref and potentially free @cc.
 **/
void camel_operation_unref(CamelOperation *cc)
{
	GSList *n;
#ifdef ENABLE_THREADS
	CamelOperationMsg *msg;

	if (cc->refcount == 1) {
		while ((msg = (CamelOperationMsg *)e_msgport_get(cc->cancel_port)))
			g_free(msg);

		e_msgport_destroy(cc->cancel_port);
#endif
		n = cc->status_stack;
		while (n) {
			g_warning("Camel operation status stack non empty: %s", (char *)n->data);
			g_free(n->data);
			n = n->next;
		}
		g_slist_free(cc->status_stack);

		g_free(cc);
	} else {
		CAMEL_OPERATION_LOCK(cc);
		cc->refcount--;
		CAMEL_OPERATION_UNLOCK(cc);
	}
}

/**
 * camel_operation_cancel_block:
 * @cc: 
 * 
 * Block cancellation for this operation.  If @cc is NULL, then the
 * current thread is blocked.
 **/
void camel_operation_cancel_block(CamelOperation *cc)
{
	CAMEL_ACTIVE_LOCK();
	if (operation_active == NULL)
		operation_active = g_hash_table_new(NULL, NULL);

	if (cc == NULL)
		cc = g_hash_table_lookup(operation_active, (void *)pthread_self());
	CAMEL_ACTIVE_UNLOCK();

	if (cc) {
		CAMEL_OPERATION_LOCK(cc);
		cc->blocked++;
		CAMEL_OPERATION_UNLOCK(cc);
	}
}

/**
 * camel_operation_cancel_unblock:
 * @cc: 
 * 
 * Unblock cancellation, when the unblock count reaches the block
 * count, then this operation can be cancelled.  If @cc is NULL, then
 * the current thread is unblocked.
 **/
void camel_operation_cancel_unblock(CamelOperation *cc)
{
	CAMEL_ACTIVE_LOCK();
	if (operation_active == NULL)
		operation_active = g_hash_table_new(NULL, NULL);

	if (cc == NULL)
		cc = g_hash_table_lookup(operation_active, (void *)pthread_self());
	CAMEL_ACTIVE_UNLOCK();

	if (cc) {
		CAMEL_OPERATION_LOCK(cc);
		cc->blocked--;
		CAMEL_OPERATION_UNLOCK(cc);
	}
}

static void
cancel_thread(void *key, CamelOperation *cc, void *data)
{
	if (cc)
		camel_operation_cancel(cc);
}

/**
 * camel_operation_cancel:
 * @cc: 
 * 
 * Cancel a given operation.  If @cc is NULL then all outstanding
 * operations are cancelled.
 **/
void camel_operation_cancel(CamelOperation *cc)
{
	CamelOperationMsg *msg;

	if (cc == NULL) {
		if (operation_active) {
			CAMEL_ACTIVE_LOCK();
			g_hash_table_foreach(operation_active, (GHFunc)cancel_thread, NULL);
			CAMEL_ACTIVE_UNLOCK();
		}
	} else if ((cc->flags & CAMEL_OPERATION_CANCELLED) == 0) {
		d(printf("cancelling thread %d\n", cc->id));

		CAMEL_OPERATION_LOCK(cc);
		msg = g_malloc0(sizeof(*msg));
		e_msgport_put(cc->cancel_port, (EMsg *)msg);
		cc->flags |= CAMEL_OPERATION_CANCELLED;
		CAMEL_OPERATION_UNLOCK(cc);
	}
}

/**
 * camel_operation_register:
 * @cc: 
 * 
 * Register a thread or the main thread for cancellation through @cc.
 * If @cc is NULL, then a new cancellation is created for this thread,
 * but may only be cancelled from the same thread.
 *
 * All calls to operation_register() should be matched with calls to
 * operation_unregister(), or resources will be lost.
 **/
void camel_operation_register(CamelOperation *cc)
{
	pthread_t id = pthread_self();

	CAMEL_ACTIVE_LOCK();

	if (operation_active == NULL)
		operation_active = g_hash_table_new(NULL, NULL);

	if (cc == NULL) {
		cc = g_hash_table_lookup(operation_active, (void *)id);
		if (cc == NULL) {
			cc = camel_operation_new(NULL, NULL);
		}
	}

	cc->id = id;
	g_hash_table_insert(operation_active, (void *)id, cc);

	d(printf("registering thread %ld for cancellation\n", id));

	CAMEL_ACTIVE_UNLOCK();

	camel_operation_ref(cc);
}

/**
 * camel_operation_unregister:
 * @cc: 
 * 
 * Unregister a given operation from being cancelled.  If @cc is NULL,
 * then the current thread is used.
 **/
void camel_operation_unregister(CamelOperation *cc)
{
	CAMEL_ACTIVE_LOCK();

	if (operation_active == NULL)
		operation_active = g_hash_table_new(NULL, NULL);

	if (cc == NULL) {
		cc = g_hash_table_lookup(operation_active, (void *)pthread_self());
		if (cc == NULL) {
			g_warning("Trying to unregister a thread that was never registered for cancellation");
		}
	}

	if (cc)
		g_hash_table_remove(operation_active, (void *)cc->id);

	CAMEL_ACTIVE_UNLOCK();

	d({if (cc) printf("unregistering thread %d for cancellation\n", cc->id);});

	if (cc)
		camel_operation_unref(cc);
}

/**
 * camel_operation_cancel_check:
 * @cc: 
 * 
 * Check if cancellation has been applied to @cc.  If @cc is NULL,
 * then the CamelOperation registered for the current thread is used.
 * 
 * Return value: TRUE if the operation has been cancelled.
 **/
gboolean camel_operation_cancel_check(CamelOperation *cc)
{
	CamelOperationMsg *msg;

	d(printf("checking for cancel in thread %d\n", pthread_self()));

	if (cc == NULL) {
		if (operation_active) {
			CAMEL_ACTIVE_LOCK();
			cc = g_hash_table_lookup(operation_active, (void *)pthread_self());
			CAMEL_ACTIVE_UNLOCK();
		}
		if (cc == NULL)
			return FALSE;
	}

	if (cc->blocked > 0) {
		d(printf("ahah!  cancellation is blocked\n"));
		return FALSE;
	}

	if (cc->flags & CAMEL_OPERATION_CANCELLED) {
		d(printf("previously cancelled\n"));
		return TRUE;
	}

	msg = (CamelOperationMsg *)e_msgport_get(cc->cancel_port);
	if (msg) {
		d(printf("Got cancellation message\n"));
		CAMEL_OPERATION_LOCK(cc);
		cc->flags |= CAMEL_OPERATION_CANCELLED;
		CAMEL_OPERATION_UNLOCK(cc);
		return TRUE;
	}
	return FALSE;
}

/**
 * camel_operation_cancel_fd:
 * @cc: 
 * 
 * Retrieve a file descriptor that can be waited on (select, or poll)
 * for read, to asynchronously detect cancellation.
 * 
 * Return value: The fd, or -1 if cancellation is not available
 * (blocked, or has not been registered for this thread).
 **/
int camel_operation_cancel_fd(CamelOperation *cc)
{
	if (cc == NULL) {
		if (operation_active) {
			CAMEL_ACTIVE_LOCK();
			cc = g_hash_table_lookup(operation_active, (void *)pthread_self());
			CAMEL_ACTIVE_UNLOCK();
		}
		if (cc == NULL)
			return -1;
	}
	if (cc->blocked)
		return -1;

	return cc->cancel_fd;
}

/**
 * camel_operation_start:
 * @cc: 
 * @what: 
 * @: 
 * 
 * Report the start of an operation.  All start operations should have
 * similar end operations.
 **/
void camel_operation_start(CamelOperation *cc, char *what, ...)
{
	va_list ap;
	char *msg;

	if (operation_active == NULL)
		return;

	if (cc == NULL) {
		CAMEL_ACTIVE_LOCK();
		cc = g_hash_table_lookup(operation_active, (void *)pthread_self());
		CAMEL_ACTIVE_UNLOCK();
		if (cc == NULL)
			return;
	}

	if (cc->status == NULL)
		return;

	va_start(ap, what);
	msg = g_strdup_vprintf(what, ap);
	va_end(ap);
	cc->status(cc, msg, CAMEL_OPERATION_START, cc->status_data);
	cc->status_update = 0;
	cc->status_stack = g_slist_prepend(cc->status_stack, msg);
	d(printf("start '%s'\n", msg, pc));
}

/**
 * camel_operation_progress:
 * @cc: Operation to report to.
 * @pc: Percent complete, 0 to 100.
 * 
 * Report progress on the current operation.  If @cc is NULL, then the
 * currently registered operation is used.  @pc reports the current
 * percentage of completion, which should be in the range of 0 to 100.
 *
 * If the total percentage is not know, then use
 * camel_operation_progress_count().
 **/
void camel_operation_progress(CamelOperation *cc, int pc)
{
	char *msg;
	time_t now;

	if (operation_active == NULL)
		return;

	if (cc == NULL) {
		CAMEL_ACTIVE_LOCK();
		cc = g_hash_table_lookup(operation_active, (void *)pthread_self());
		CAMEL_ACTIVE_UNLOCK();
		if (cc == NULL)
			return;
	}

	if (cc->status == NULL)
		return;

	if (cc->status_stack == NULL)
		return;

	now = time(0);
	if (cc->status_update != now) {
		msg =cc->status_stack->data;
		cc->status(cc, msg, pc, cc->status_data);
		d(printf("progress '%s' %d %%\n", msg, pc));
		cc->status_update = now;
	}
}

void camel_operation_progress_count(CamelOperation *cc, int sofar)
{
	char *msg;
	time_t now;

	if (operation_active == NULL)
		return;

	if (cc == NULL) {
		CAMEL_ACTIVE_LOCK();
		cc = g_hash_table_lookup(operation_active, (void *)pthread_self());
		CAMEL_ACTIVE_UNLOCK();
		if (cc == NULL)
			return;
	}

	if (cc->status == NULL)
		return;

	if (cc->status_stack == NULL)
		return;

	/* FIXME: generate some meaningful pc value */
	now = time(0);
	if (cc->status_update != now) {
		msg =cc->status_stack->data;
		cc->status(cc, msg, sofar, cc->status_data);
		d(printf("progress '%s' %d done\n", msg, sofar));
		cc->status_update = now;
	}
}

/**
 * camel_operation_end:
 * @cc: 
 * @what: Format string.
 * @: 
 * 
 * Report the end of an operation.  If @cc is NULL, then the currently
 * registered operation is notified.
 **/
void camel_operation_end(CamelOperation *cc)
{
	char *msg;

	if (operation_active == NULL)
		return;

	if (cc == NULL) {
		CAMEL_ACTIVE_LOCK();
		cc = g_hash_table_lookup(operation_active, (void *)pthread_self());
		CAMEL_ACTIVE_UNLOCK();
		if (cc == NULL)
			return;
	}

	if (cc->status == NULL)
		return;

	if (cc->status_stack == NULL)
		return;

	msg = cc->status_stack->data;
	cc->status(cc, msg, CAMEL_OPERATION_END, cc->status_data);
	g_free(msg);
	cc->status_stack = g_slist_remove_link(cc->status_stack, cc->status_stack);
}
