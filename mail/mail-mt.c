/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 */

#include <config.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

#include <glib.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <libedataserver/e-data-server-util.h>
#include <libedataserver/e-flag.h>

#include <camel/camel-url.h>
#include <camel/camel-operation.h>

#include "misc/e-gui-utils.h"
#include "e-util/e-error.h"
#include "e-util/e-icon-factory.h"

#include "e-activity-handler.h"

#include "mail-config.h"
#include "mail-component.h"
#include "mail-session.h"
#include "mail-mt.h"

/*#define MALLOC_CHECK*/
#define LOG_OPS
#define LOG_LOCKS
#define d(x)

static void set_stop(gint sensitive);
static void mail_operation_status(CamelOperation *op, const gchar *what, gint pc, gpointer data);

#ifdef LOG_LOCKS
#define MAIL_MT_LOCK(x) (log_locks?fprintf(log, "%" G_GINT64_MODIFIER "x: lock " # x "\n", e_util_pthread_id(pthread_self())):0, pthread_mutex_lock(&x))
#define MAIL_MT_UNLOCK(x) (log_locks?fprintf(log, "%" G_GINT64_MODIFIER "x: unlock " # x "\n", e_util_pthread_id(pthread_self())): 0, pthread_mutex_unlock(&x))
#else
#define MAIL_MT_LOCK(x) pthread_mutex_lock(&x)
#define MAIL_MT_UNLOCK(x) pthread_mutex_unlock(&x)
#endif

/* background operation status stuff */
struct _MailMsgPrivate {
	gint activity_state;	/* sigh sigh sigh, we need to keep track of the state external to the
				   pointer itself for locking/race conditions */
	gint activity_id;
	GtkWidget *error;
	gboolean cancelable;
};

/* mail_msg stuff */
#ifdef LOG_OPS
static FILE *log;
static gint log_ops, log_locks, log_init;
#endif

static guint mail_msg_seq; /* sequence number of each message */
static GHashTable *mail_msg_active_table; /* table of active messages, must hold mail_msg_lock to access */
static pthread_mutex_t mail_msg_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t mail_msg_cond = PTHREAD_COND_INITIALIZER;

MailAsyncEvent *mail_async_event;

gpointer
mail_msg_new (MailMsgInfo *info)
{
	MailMsg *msg;

	MAIL_MT_LOCK(mail_msg_lock);

#if defined(LOG_OPS) || defined(LOG_LOCKS)
	if (!log_init) {
		time_t now = time(NULL);

		log_init = TRUE;
		log_ops = getenv("EVOLUTION_MAIL_LOG_OPS") != NULL;
		log_locks = getenv("EVOLUTION_MAIL_LOG_LOCKS") != NULL;
		if (log_ops || log_locks) {
			log = fopen("evolution-mail-ops.log", "w+");
			if (log) {
				setvbuf(log, NULL, _IOLBF, 0);
				fprintf(log, "Started evolution-mail: %s\n", ctime(&now));
				g_warning("Logging mail operations to evolution-mail-ops.log");

				if (log_ops)
					fprintf(log, "Logging async operations\n");

				if (log_locks) {
					fprintf(log, "%" G_GINT64_MODIFIER "x: lock mail_msg_lock\n", e_util_pthread_id(pthread_self()));
				}
			} else {
				g_warning ("Could not open log file: %s", g_strerror(errno));
				log_ops = log_locks = FALSE;
			}
		}
	}
#endif
	msg = g_slice_alloc0 (info->size);
	msg->info = info;
	msg->ref_count = 1;
	msg->seq = mail_msg_seq++;
	msg->cancel = camel_operation_new(mail_operation_status, GINT_TO_POINTER(msg->seq));
	camel_exception_init(&msg->ex);
	msg->priv = g_slice_new0 (MailMsgPrivate);
	msg->priv->cancelable = TRUE;

	g_hash_table_insert(mail_msg_active_table, GINT_TO_POINTER(msg->seq), msg);

	d(printf("New message %p\n", msg));

#ifdef LOG_OPS
	if (log_ops)
		fprintf(log, "%p: New\n", (gpointer) msg);
#endif
	MAIL_MT_UNLOCK(mail_msg_lock);

	return msg;
}

static void
end_event_callback (CamelObject *o, gpointer event_data, gpointer error)
{
	MailComponent *component;
	EActivityHandler *activity_handler;
	guint activity_id = GPOINTER_TO_INT (event_data);

	component = mail_component_peek ();
	activity_handler = mail_component_peek_activity_handler (component);
	if (!error) {
		e_activity_handler_operation_finished (activity_handler, activity_id);
	} else {
		d(printf("Yahooooo, we got it nonintrusively\n"));
		e_activity_handler_operation_set_error (activity_handler, activity_id, error);
	}
}

#ifdef MALLOC_CHECK
#include <mcheck.h>

static void
checkmem(gpointer p)
{
	if (p) {
		gint status = mprobe(p);

		switch (status) {
		case MCHECK_HEAD:
			printf("Memory underrun at %p\n", p);
			abort();
		case MCHECK_TAIL:
			printf("Memory overrun at %p\n", p);
			abort();
		case MCHECK_FREE:
			printf("Double free %p\n", p);
			abort();
		}
	}
}
#endif

static void
mail_msg_free (MailMsg *mail_msg)
{
	if (mail_msg->cancel != NULL) {
		camel_operation_mute (mail_msg->cancel);
		camel_operation_unref (mail_msg->cancel);
	}

	camel_exception_clear (&mail_msg->ex);
	g_slice_free (MailMsgPrivate, mail_msg->priv);
	g_slice_free1 (mail_msg->info->size, mail_msg);
}

gpointer
mail_msg_ref (gpointer msg)
{
	MailMsg *mail_msg = msg;

	g_return_val_if_fail (mail_msg != NULL, msg);
	g_return_val_if_fail (mail_msg->ref_count > 0, msg);

	g_atomic_int_add (&mail_msg->ref_count, 1);
	return msg;
}

void
mail_msg_unref (gpointer msg)
{
	MailMsg *mail_msg = msg;
	gint activity_id;
	GtkWidget *error = NULL;

	g_return_if_fail (mail_msg != NULL);
	g_return_if_fail (mail_msg->ref_count > 0);

	if (g_atomic_int_exchange_and_add (&mail_msg->ref_count, -1) > 1)
		return;

#ifdef MALLOC_CHECK
	checkmem(mail_msg);
	checkmem(mail_msg->cancel);
	checkmem(mail_msg->priv);
#endif
	d(printf("Free message %p\n", msg));

	if (mail_msg->info->free)
		mail_msg->info->free(mail_msg);

	MAIL_MT_LOCK(mail_msg_lock);

#ifdef LOG_OPS
	if (log_ops) {
		const gchar *description;

		description = camel_exception_get_description (&mail_msg->ex);
		if (description == NULL)
			description = "None";
		fprintf(log, "%p: Free  (exception `%s')\n", msg, description);
	}
#endif
	g_hash_table_remove (
		mail_msg_active_table, GINT_TO_POINTER (mail_msg->seq));
	pthread_cond_broadcast (&mail_msg_cond);

	/* We need to make sure we dont lose a reference here YUCK YUCK */
	/* This is tightly integrated with the code in do_op_status,
	   as it closely relates to the CamelOperation setup in msg_new() above */
	if (mail_msg->priv->activity_state == 1) {
		/* tell the other to free it itself */
		mail_msg->priv->activity_state = 3;
		MAIL_MT_UNLOCK(mail_msg_lock);
		return;
	} else {
		activity_id = mail_msg->priv->activity_id;
		error = mail_msg->priv->error;
		if (error && !activity_id) {
			e_activity_handler_make_error (mail_component_peek_activity_handler (mail_component_peek ()), "mail", E_LOG_ERROR, error);
			printf("Making error\n");
		}

	}

	MAIL_MT_UNLOCK(mail_msg_lock);

	mail_msg_free (mail_msg);

	if (activity_id != 0)
		mail_async_event_emit (
			mail_async_event, MAIL_ASYNC_GUI,
			(MailAsyncFunc) end_event_callback,
			NULL, GINT_TO_POINTER (activity_id), error);
}

/* hash table of ops->dialogue of active errors */
static GHashTable *active_errors = NULL;

static void error_destroy(GtkObject *o, gpointer data)
{
	g_hash_table_remove(active_errors, data);
}

static void error_response(GtkObject *o, gint button, gpointer data)
{
	gtk_widget_destroy((GtkWidget *)o);
}

void
mail_msg_check_error (gpointer msg)
{
	MailMsg *m = msg;
	gchar *what;
	GtkDialog *gd;

#ifdef MALLOC_CHECK
	checkmem(m);
	checkmem(m->cancel);
	checkmem(m->priv);
#endif

	/* don't report any errors if we are not in interactive mode */
	if (!mail_session_get_interactive ())
		return;

	if (!camel_exception_is_set(&m->ex)
	    || m->ex.id == CAMEL_EXCEPTION_USER_CANCEL
	    || m->ex.id == CAMEL_EXCEPTION_FOLDER_INVALID_UID) {
		mail_component_show_status_bar (FALSE);
		return;
	}

	if (active_errors == NULL)
		active_errors = g_hash_table_new(NULL, NULL);

	/* check to see if we have dialogue already running for this operation */
	/* we key on the operation pointer, which is at least accurate enough
	   for the operation type, although it could be on a different object. */
	if (g_hash_table_lookup(active_errors, m->info)) {
		g_message("Error occurred while existing dialogue active:\n%s", camel_exception_get_description(&m->ex));
		return;
	}

	if (m->info->desc
	    && (what = m->info->desc (m))) {
		gd = (GtkDialog *)e_error_new(NULL, "mail:async-error", what, camel_exception_get_description(&m->ex), NULL);
		g_free(what);
	} else
		gd = (GtkDialog *)e_error_new(NULL, "mail:async-error-nodescribe", camel_exception_get_description(&m->ex), NULL);

	g_hash_table_insert(active_errors, m->info, gd);
	g_signal_connect(gd, "response", G_CALLBACK(error_response), m->info);
	g_signal_connect(gd, "destroy", G_CALLBACK(error_destroy), m->info);
	if (m->priv->cancelable) {
		m->priv->error = (GtkWidget *) gd;
		mail_component_show_status_bar (TRUE);
	} else
		gtk_widget_show((GtkWidget *)gd);

}

void mail_msg_cancel(guint msgid)
{
	MailMsg *m;

	MAIL_MT_LOCK(mail_msg_lock);
	m = g_hash_table_lookup(mail_msg_active_table, GINT_TO_POINTER(msgid));

	if (m && m->cancel)
		camel_operation_cancel(m->cancel);

	MAIL_MT_UNLOCK(mail_msg_lock);
}

/* waits for a message to be finished processing (freed)
   the messageid is from MailMsg->seq */
void mail_msg_wait(guint msgid)
{
	MailMsg *m;

	if (mail_in_main_thread ()) {
		MAIL_MT_LOCK(mail_msg_lock);
		m = g_hash_table_lookup(mail_msg_active_table, GINT_TO_POINTER(msgid));
		while (m) {
			MAIL_MT_UNLOCK(mail_msg_lock);
			gtk_main_iteration();
			MAIL_MT_LOCK(mail_msg_lock);
			m = g_hash_table_lookup(mail_msg_active_table, GINT_TO_POINTER(msgid));
		}
		MAIL_MT_UNLOCK(mail_msg_lock);
	} else {
		MAIL_MT_LOCK(mail_msg_lock);
		m = g_hash_table_lookup(mail_msg_active_table, GINT_TO_POINTER(msgid));
		while (m) {
			pthread_cond_wait(&mail_msg_cond, &mail_msg_lock);
			m = g_hash_table_lookup(mail_msg_active_table, GINT_TO_POINTER(msgid));
		}
		MAIL_MT_UNLOCK(mail_msg_lock);
	}
}

gint mail_msg_active(guint msgid)
{
	gint active;

	MAIL_MT_LOCK(mail_msg_lock);
	if (msgid == (guint)-1)
		active = g_hash_table_size(mail_msg_active_table) > 0;
	else
		active = g_hash_table_lookup(mail_msg_active_table, GINT_TO_POINTER(msgid)) != NULL;
	MAIL_MT_UNLOCK(mail_msg_lock);

	return active;
}

void mail_msg_wait_all(void)
{
	if (mail_in_main_thread ()) {
		MAIL_MT_LOCK(mail_msg_lock);
		while (g_hash_table_size(mail_msg_active_table) > 0) {
			MAIL_MT_UNLOCK(mail_msg_lock);
			gtk_main_iteration();
			MAIL_MT_LOCK(mail_msg_lock);
		}
		MAIL_MT_UNLOCK(mail_msg_lock);
	} else {
		MAIL_MT_LOCK(mail_msg_lock);
		while (g_hash_table_size(mail_msg_active_table) > 0) {
			pthread_cond_wait(&mail_msg_cond, &mail_msg_lock);
		}
		MAIL_MT_UNLOCK(mail_msg_lock);
	}
}

/* **************************************** */

static GHookList cancel_hook_list;

GHook *
mail_cancel_hook_add (GHookFunc func, gpointer data)
{
	GHook *hook;

	MAIL_MT_LOCK (mail_msg_lock);

	if (!cancel_hook_list.is_setup)
		g_hook_list_init (&cancel_hook_list, sizeof (GHook));

	hook = g_hook_alloc (&cancel_hook_list);
	hook->func = func;
	hook->data = data;

	g_hook_append (&cancel_hook_list, hook);

	MAIL_MT_UNLOCK (mail_msg_lock);

	return hook;
}

void
mail_cancel_hook_remove (GHook *hook)
{
	MAIL_MT_LOCK (mail_msg_lock);

	g_return_if_fail (cancel_hook_list.is_setup);
	g_hook_destroy_link (&cancel_hook_list, hook);

	MAIL_MT_UNLOCK (mail_msg_lock);
}

void
mail_cancel_all (void)
{
	camel_operation_cancel (NULL);

	MAIL_MT_LOCK (mail_msg_lock);

	if (cancel_hook_list.is_setup)
		g_hook_list_invoke (&cancel_hook_list, FALSE);

	MAIL_MT_UNLOCK (mail_msg_lock);
}

void
mail_msg_set_cancelable (gpointer msg, gboolean status)
{
	MailMsg *mail_msg = msg;

	mail_msg->priv->cancelable = status;
}

static guint idle_source_id = 0;
G_LOCK_DEFINE_STATIC (idle_source_id);
static GAsyncQueue *main_loop_queue = NULL;
static GAsyncQueue *msg_reply_queue = NULL;
static GThread *main_thread = NULL;

static gboolean
mail_msg_idle_cb (void)
{
	MailMsg *msg;

	g_return_val_if_fail (main_loop_queue != NULL, FALSE);
	g_return_val_if_fail (msg_reply_queue != NULL, FALSE);

	G_LOCK (idle_source_id);
	idle_source_id = 0;
	G_UNLOCK (idle_source_id);
	mail_component_show_status_bar (TRUE);
	/* check the main loop queue */
	while ((msg = g_async_queue_try_pop (main_loop_queue)) != NULL) {
		if (msg->info->exec != NULL)
			msg->info->exec (msg);
		if (msg->info->done != NULL)
			msg->info->done (msg);
		mail_msg_unref (msg);
	}

	/* check the reply queue */
	while ((msg = g_async_queue_try_pop (msg_reply_queue)) != NULL) {
		if (msg->info->done != NULL)
			msg->info->done (msg);
		mail_msg_check_error (msg);
		mail_msg_unref (msg);
	}
	mail_component_show_status_bar (FALSE);
	return FALSE;
}

static void
mail_msg_proxy (MailMsg *msg)
{
	if (msg->info->desc != NULL && msg->cancel) {
		gchar *text = msg->info->desc (msg);
		camel_operation_register (msg->cancel);
		camel_operation_start (msg->cancel, "%s", text);
		g_free (text);
	}

	if (msg->info->exec != NULL) {
		mail_enable_stop ();
		msg->info->exec (msg);
		mail_disable_stop ();
	}

	if (msg->info->desc != NULL && msg->cancel) {
		camel_operation_end (msg->cancel);
		camel_operation_unregister (msg->cancel);
		MAIL_MT_LOCK (mail_msg_lock);
		camel_operation_unref (msg->cancel);
		msg->cancel = NULL;
		MAIL_MT_UNLOCK (mail_msg_lock);
	}

	g_async_queue_push (msg_reply_queue, msg);

	G_LOCK (idle_source_id);
	if (idle_source_id == 0)
		idle_source_id = g_idle_add (
			(GSourceFunc) mail_msg_idle_cb, NULL);
	G_UNLOCK (idle_source_id);
}

void
mail_msg_cleanup (void)
{
	mail_msg_wait_all();

	G_LOCK (idle_source_id);
	if (idle_source_id != 0) {
		GSource *source;

		/* Cancel the idle source. */
		source = g_main_context_find_source_by_id (
			g_main_context_default (), idle_source_id);
		g_source_destroy (source);
		idle_source_id = 0;
	}
	G_UNLOCK (idle_source_id);

	g_async_queue_unref (main_loop_queue);
	main_loop_queue = NULL;

	g_async_queue_unref (msg_reply_queue);
	msg_reply_queue = NULL;
}

void
mail_msg_init (void)
{
	main_loop_queue = g_async_queue_new ();
	msg_reply_queue = g_async_queue_new ();

	mail_msg_active_table = g_hash_table_new (NULL, NULL);
	main_thread = g_thread_self ();

	mail_async_event = mail_async_event_new ();
}

static gint
mail_msg_compare (const MailMsg *msg1, const MailMsg *msg2)
{
	gint priority1 = msg1->priority;
	gint priority2 = msg2->priority;

	if (priority1 == priority2)
		return 0;

	return (priority1 < priority2) ? 1 : -1;
}

static gpointer
create_thread_pool (gpointer data)
{
	GThreadPool *thread_pool;
	gint max_threads = GPOINTER_TO_INT (data);

	/* once created, run forever */
	thread_pool = g_thread_pool_new (
		(GFunc) mail_msg_proxy, NULL, max_threads, FALSE, NULL);
	g_thread_pool_set_sort_function (
		thread_pool, (GCompareDataFunc) mail_msg_compare, NULL);

	return thread_pool;
}

void
mail_msg_main_loop_push (gpointer msg)
{
	g_async_queue_push_sorted (main_loop_queue, msg,
		(GCompareDataFunc) mail_msg_compare, NULL);

	G_LOCK (idle_source_id);
	if (idle_source_id == 0)
		idle_source_id = g_idle_add (
			(GSourceFunc) mail_msg_idle_cb, NULL);
	G_UNLOCK (idle_source_id);
}

void
mail_msg_unordered_push (gpointer msg)
{
	static GOnce once = G_ONCE_INIT;

	g_once (&once, (GThreadFunc) create_thread_pool, GINT_TO_POINTER (10));

	g_thread_pool_push ((GThreadPool *) once.retval, msg, NULL);
	mail_component_show_status_bar (TRUE);
}

void
mail_msg_fast_ordered_push (gpointer msg)
{
	static GOnce once = G_ONCE_INIT;

	g_once (&once, (GThreadFunc) create_thread_pool, GINT_TO_POINTER (1));

	g_thread_pool_push ((GThreadPool *) once.retval, msg, NULL);
	mail_component_show_status_bar (TRUE);
}

void
mail_msg_slow_ordered_push (gpointer msg)
{
	static GOnce once = G_ONCE_INIT;

	g_once (&once, (GThreadFunc) create_thread_pool, GINT_TO_POINTER (1));

	g_thread_pool_push ((GThreadPool *) once.retval, msg, NULL);
	mail_component_show_status_bar (TRUE);
}

gboolean
mail_in_main_thread (void)
{
	return (g_thread_self () == main_thread);
}

/* ********************************************************************** */

/* locks */
static pthread_mutex_t status_lock = PTHREAD_MUTEX_INITIALIZER;

/* ********************************************************************** */

struct _proxy_msg {
	MailMsg base;

	MailAsyncEvent *ea;
	mail_async_event_t type;

	pthread_t thread;
	gint have_thread;

	MailAsyncFunc func;
	gpointer o;
	gpointer event_data;
	gpointer data;
};

static void
do_async_event(struct _proxy_msg *m)
{
	m->thread = pthread_self();
	m->have_thread = TRUE;
	m->func(m->o, m->event_data, m->data);
	m->have_thread = FALSE;

	g_mutex_lock(m->ea->lock);
	m->ea->tasks = g_slist_remove(m->ea->tasks, m);
	g_mutex_unlock(m->ea->lock);
}

static gint
idle_async_event(gpointer mm)
{
	do_async_event(mm);
	mail_msg_unref(mm);

	return FALSE;
}

static MailMsgInfo async_event_info = {
	sizeof (struct _proxy_msg),
	(MailMsgDescFunc) NULL,
	(MailMsgExecFunc) do_async_event,
	(MailMsgDoneFunc) NULL,
	(MailMsgFreeFunc) NULL
};

MailAsyncEvent *mail_async_event_new(void)
{
	MailAsyncEvent *ea;

	ea = g_malloc0(sizeof(*ea));
	ea->lock = g_mutex_new();

	return ea;
}

gint mail_async_event_emit(MailAsyncEvent *ea, mail_async_event_t type, MailAsyncFunc func, gpointer o, gpointer event_data, gpointer data)
{
	struct _proxy_msg *m;
	gint id;

	/* we dont have a reply port for this, we dont care when/if it gets executed, just queue it */
	m = mail_msg_new(&async_event_info);
	m->func = func;
	m->o = o;
	m->event_data = event_data;
	m->data = data;
	m->ea = ea;
	m->type = type;
	m->have_thread = FALSE;

	id = m->base.seq;
	g_mutex_lock(ea->lock);
	ea->tasks = g_slist_prepend(ea->tasks, m);
	g_mutex_unlock(ea->lock);

	/* We use an idle function instead of our own message port only because the
	   gui message ports's notification buffer might overflow and deadlock us */
	if (type == MAIL_ASYNC_GUI) {
		if (mail_in_main_thread ())
			g_idle_add(idle_async_event, m);
		else
			mail_msg_main_loop_push(m);
	} else
		mail_msg_fast_ordered_push (m);

	return id;
}

gint mail_async_event_destroy(MailAsyncEvent *ea)
{
	gint id;
	pthread_t thread = pthread_self();
	struct _proxy_msg *m;

	g_mutex_lock(ea->lock);
	while (ea->tasks) {
		m = ea->tasks->data;
		id = m->base.seq;
		if (m->have_thread && pthread_equal(m->thread, thread)) {
			g_warning("Destroying async event from inside an event, returning EDEADLK");
			g_mutex_unlock(ea->lock);
			errno = EDEADLK;
			return -1;
		}
		g_mutex_unlock(ea->lock);
		mail_msg_wait(id);
		g_mutex_lock(ea->lock);
	}
	g_mutex_unlock(ea->lock);

	g_mutex_free(ea->lock);
	g_free(ea);

	return 0;
}

/* ********************************************************************** */

struct _call_msg {
	MailMsg base;

	mail_call_t type;
	MailMainFunc func;
	gpointer ret;
	va_list ap;
	EFlag *done;
};

static void
do_call(struct _call_msg *m)
{
	gpointer p1, *p2, *p3, *p4, *p5;
	gint i1;
	va_list ap;

	G_VA_COPY(ap, m->ap);

	switch (m->type) {
	case MAIL_CALL_p_p:
		p1 = va_arg(ap, gpointer );
		m->ret = m->func(p1);
		break;
	case MAIL_CALL_p_pp:
		p1 = va_arg(ap, gpointer );
		p2 = va_arg(ap, gpointer );
		m->ret = m->func(p1, p2);
		break;
	case MAIL_CALL_p_ppp:
		p1 = va_arg(ap, gpointer );
		p2 = va_arg(ap, gpointer );
		p3 = va_arg(ap, gpointer );
		m->ret = m->func(p1, p2, p3);
		break;
	case MAIL_CALL_p_pppp:
		p1 = va_arg(ap, gpointer );
		p2 = va_arg(ap, gpointer );
		p3 = va_arg(ap, gpointer );
		p4 = va_arg(ap, gpointer );
		m->ret = m->func(p1, p2, p3, p4);
		break;
	case MAIL_CALL_p_ppppp:
		p1 = va_arg(ap, gpointer );
		p2 = va_arg(ap, gpointer );
		p3 = va_arg(ap, gpointer );
		p4 = va_arg(ap, gpointer );
		p5 = va_arg(ap, gpointer );
		m->ret = m->func(p1, p2, p3, p4, p5);
		break;
	case MAIL_CALL_p_ppippp:
		p1 = va_arg(ap, gpointer );
		p2 = va_arg(ap, gpointer );
		i1 = va_arg(ap, gint);
		p3 = va_arg(ap, gpointer );
		p4 = va_arg(ap, gpointer );
		p5 = va_arg(ap, gpointer );
		m->ret = m->func(p1, p2, i1, p3, p4, p5);
		break;
	}

	if (m->done != NULL)
		e_flag_set (m->done);
}

static MailMsgInfo mail_call_info = {
	sizeof (struct _call_msg),
	(MailMsgDescFunc) NULL,
	(MailMsgExecFunc) do_call,
	(MailMsgDoneFunc) NULL,
	(MailMsgFreeFunc) NULL
};

gpointer
mail_call_main (mail_call_t type, MailMainFunc func, ...)
{
	struct _call_msg *m;
	gpointer ret;
	va_list ap;

	va_start(ap, func);

	m = mail_msg_new (&mail_call_info);
	m->type = type;
	m->func = func;
	G_VA_COPY(m->ap, ap);

	if (mail_in_main_thread ()) {
		mail_component_show_status_bar (TRUE);
		do_call (m);
		mail_component_show_status_bar (FALSE);
	} else {
		mail_msg_ref (m);
		m->done = e_flag_new ();
		mail_msg_main_loop_push (m);
		e_flag_wait (m->done);
		e_flag_free (m->done);
	}

	va_end(ap);

	ret = m->ret;
	mail_msg_unref (m);

	return ret;
}

/* ********************************************************************** */
/* locked via status_lock */
static gint busy_state;

static void
do_set_busy(MailMsg *mm)
{
	set_stop(busy_state > 0);
}

static MailMsgInfo set_busy_info = {
	sizeof (MailMsg),
	(MailMsgDescFunc) NULL,
	(MailMsgExecFunc) do_set_busy,
	(MailMsgDoneFunc) NULL,
	(MailMsgFreeFunc) NULL
};

void mail_enable_stop(void)
{
	MailMsg *m;

	MAIL_MT_LOCK(status_lock);
	busy_state++;
	if (busy_state == 1) {
		m = mail_msg_new(&set_busy_info);
		mail_msg_main_loop_push(m);
	}
	MAIL_MT_UNLOCK(status_lock);
}

void mail_disable_stop(void)
{
	MailMsg *m;

	MAIL_MT_LOCK(status_lock);
	busy_state--;
	if (busy_state == 0) {
		m = mail_msg_new(&set_busy_info);
		mail_msg_main_loop_push(m);
	}
	MAIL_MT_UNLOCK(status_lock);
}

/* ******************************************************************************** */

struct _op_status_msg {
	MailMsg base;

	CamelOperation *op;
	gchar *what;
	gint pc;
	gpointer data;
};

static void
op_status_exec (struct _op_status_msg *m)
{
	EActivityHandler *activity_handler = mail_component_peek_activity_handler (mail_component_peek ());
	MailMsg *msg;
	MailMsgPrivate *data;
	gchar *out, *p, *o, c;
	gint pc;

	g_return_if_fail (mail_in_main_thread ());

	MAIL_MT_LOCK (mail_msg_lock);

	msg = g_hash_table_lookup (mail_msg_active_table, m->data);

	if (msg == NULL) {
		MAIL_MT_UNLOCK (mail_msg_lock);
		return;
	}

	data = msg->priv;

	out = alloca (strlen (m->what) * 2 + 1);
	o = out;
	p = m->what;
	while ((c = *p++)) {
		if (c == '%')
			*o++ = '%';
		*o++ = c;
	}
	*o = 0;

	pc = m->pc;

	if (data->activity_id == 0) {
		gchar *what;

		/* its being created/removed?  well leave it be */
		if (data->activity_state == 1 || data->activity_state == 3) {
			MAIL_MT_UNLOCK (mail_msg_lock);
			return;
		} else {
			data->activity_state = 1;

			MAIL_MT_UNLOCK (mail_msg_lock);
			if (msg->info->desc)
				what = msg->info->desc (msg);
			else if (m->what)
				what = g_strdup (m->what);
			/* uncommenting because message is not very useful for a user, see bug 271734*/
			else {
				what = g_strdup("");
			}

			data->activity_id = e_activity_handler_cancelable_operation_started (activity_handler, "evolution-mail", what, TRUE, (void (*) (gpointer)) camel_operation_cancel, msg->cancel);

			g_free (what);
			MAIL_MT_LOCK (mail_msg_lock);
			if (data->activity_state == 3) {
				gint activity_id = data->activity_id;

				MAIL_MT_UNLOCK (mail_msg_lock);
				mail_msg_free (msg);

				if (activity_id != 0)
					mail_async_event_emit (mail_async_event, MAIL_ASYNC_GUI, (MailAsyncFunc) end_event_callback,
								NULL, GINT_TO_POINTER (activity_id), NULL);
			} else {
				data->activity_state = 2;
				MAIL_MT_UNLOCK (mail_msg_lock);
			}
			return;
		}
	} else if (data->activity_id != 0) {
		MAIL_MT_UNLOCK (mail_msg_lock);
		e_activity_handler_operation_progressing (activity_handler, data->activity_id, out, (double)(pc/100.0));
	} else {
		MAIL_MT_UNLOCK (mail_msg_lock);
	}
}

static void
op_status_free (struct _op_status_msg *m)
{
	g_free (m->what);
}

static MailMsgInfo op_status_info = {
	sizeof (struct _op_status_msg),
	(MailMsgDescFunc) NULL,
	(MailMsgExecFunc) op_status_exec,
	(MailMsgDoneFunc) NULL,
	(MailMsgFreeFunc) op_status_free
};

static void
mail_operation_status (CamelOperation *op, const gchar *what, gint pc, gpointer data)
{
	struct _op_status_msg *m;

	d(printf("got operation statys: %s %d%%\n", what, pc));

	m = mail_msg_new(&op_status_info);
	m->op = op;
	m->what = g_strdup(what);
	switch (pc) {
	case CAMEL_OPERATION_START:
		pc = 0;
		break;
	case CAMEL_OPERATION_END:
		pc = 100;
		break;
	}
	m->pc = pc;
	m->data = data;
	mail_msg_main_loop_push(m);
}

/* ******************** */

static void
set_stop (gint sensitive)
{
	static gint last = FALSE;

	if (last == sensitive)
		return;

	/*bonobo_ui_component_set_prop (uic, "/commands/MailStop", "sensitive", sensitive ? "1" : "0", NULL);*/
	last = sensitive;
}
