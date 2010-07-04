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
#include <errno.h>

#include <glib.h>
#include <gtk/gtk.h>

#include <libedataserver/e-flag.h>

#include "shell/e-shell.h"
#include "e-util/e-alert-activity.h"
#include "e-util/e-alert-dialog.h"

#include "mail-mt.h"

/*#define MALLOC_CHECK*/
#define d(x)

/* XXX This is a dirty hack on a dirty hack.  We really need
 *     to rework or get rid of the functions that use this. */
const gchar *shell_builtin_backend = "mail";

static void mail_operation_status		(CamelOperation *op,
						 const gchar *what,
						 gint pc,
						 gpointer data);

/* background operation status stuff */
struct _MailMsgPrivate {
	/* XXX We need to keep track of the state external to the
	 *     pointer itself for locking/race conditions. */
	gint activity_state;
	EActivity *activity;
	GtkWidget *error;
	gboolean cancelable;
};

static guint mail_msg_seq; /* sequence number of each message */

/* Table of active messages.  Must hold mail_msg_lock to access. */
static GHashTable *mail_msg_active_table;
static GMutex *mail_msg_lock;
static GCond *mail_msg_cond;

MailAsyncEvent *mail_async_event;

gpointer
mail_msg_new (MailMsgInfo *info)
{
	MailMsg *msg;

	g_mutex_lock (mail_msg_lock);

	msg = g_slice_alloc0 (info->size);
	msg->info = info;
	msg->ref_count = 1;
	msg->seq = mail_msg_seq++;
	msg->cancel = camel_operation_new(mail_operation_status, GINT_TO_POINTER(msg->seq));
	msg->priv = g_slice_new0 (MailMsgPrivate);
	msg->priv->cancelable = TRUE;

	g_hash_table_insert(mail_msg_active_table, GINT_TO_POINTER(msg->seq), msg);

	d(printf("New message %p\n", msg));

	g_mutex_unlock (mail_msg_lock);

	return msg;
}

static void
end_event_callback (CamelObject *o, EActivity *activity, gpointer error)
{
	EShell *shell;
	EShellBackend *shell_backend;

	shell = e_shell_get_default ();
	shell_backend = e_shell_get_backend_by_name (
		shell, shell_builtin_backend);

	if (activity != NULL) {
		e_activity_complete (activity);
		g_object_unref (activity);
	}

	if (error != NULL) {
		activity = e_alert_activity_new_warning (error);
		e_shell_backend_add_activity (shell_backend, activity);
		g_object_unref (activity);
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
	if (mail_msg->priv->activity != NULL)
		g_object_unref (mail_msg->priv->activity);

	if (mail_msg->cancel != NULL) {
		camel_operation_mute (mail_msg->cancel);
		camel_operation_unref (mail_msg->cancel);
	}

	if (mail_msg->error != NULL)
		g_error_free (mail_msg->error);

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
	EActivity *activity = NULL;
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

	g_mutex_lock (mail_msg_lock);

	g_hash_table_remove (
		mail_msg_active_table, GINT_TO_POINTER (mail_msg->seq));
	g_cond_broadcast (mail_msg_cond);

	/* We need to make sure we dont lose a reference here YUCK YUCK */
	/* This is tightly integrated with the code in do_op_status,
	   as it closely relates to the CamelOperation setup in msg_new() above */
	if (mail_msg->priv->activity_state == 1) {
		/* tell the other to free it itself */
		mail_msg->priv->activity_state = 3;
		g_mutex_unlock (mail_msg_lock);
		return;
	} else {
		activity = mail_msg->priv->activity;
		if (activity != NULL)
			g_object_ref (activity);
		error = mail_msg->priv->error;
	}

	g_mutex_unlock (mail_msg_lock);

	mail_msg_free (mail_msg);

	if (activity != NULL)
		mail_async_event_emit (
			mail_async_event, MAIL_ASYNC_GUI,
			(MailAsyncFunc) end_event_callback,
			NULL, activity, error);
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
	GtkWindow *parent;
	MailMsg *m = msg;
	gchar *what;
	GtkDialog *gd;

#ifdef MALLOC_CHECK
	checkmem(m);
	checkmem(m->cancel);
	checkmem(m->priv);
#endif

	if (m->error == NULL
	    || g_error_matches (m->error, G_IO_ERROR, G_IO_ERROR_CANCELLED)
	    || g_error_matches (m->error, CAMEL_FOLDER_ERROR, CAMEL_FOLDER_ERROR_INVALID_UID)
	    || (m->cancel && camel_operation_cancel_check (m->cancel)))
		return;

	if (active_errors == NULL)
		active_errors = g_hash_table_new(NULL, NULL);

	/* check to see if we have dialogue already running for this operation */
	/* we key on the operation pointer, which is at least accurate enough
	   for the operation type, although it could be on a different object. */
	if (g_hash_table_lookup(active_errors, m->info)) {
		g_message (
			"Error occurred while existing dialogue active:\n%s",
			m->error->message);
		return;
	}

	parent = e_shell_get_active_window (NULL);

	if (m->info->desc
	    && (what = m->info->desc (m))) {
		gd = (GtkDialog *) e_alert_dialog_new_for_args (
			parent, "mail:async-error", what,
			m->error->message, NULL);
		g_free(what);
	} else
		gd = (GtkDialog *) e_alert_dialog_new_for_args (
			parent, "mail:async-error-nodescribe",
			m->error->message, NULL);

	g_hash_table_insert(active_errors, m->info, gd);
	g_signal_connect(gd, "response", G_CALLBACK(error_response), m->info);
	g_signal_connect(gd, "destroy", G_CALLBACK(error_destroy), m->info);
	if (m->priv->cancelable)
		m->priv->error = (GtkWidget *) gd;
	else
		gtk_widget_show((GtkWidget *)gd);

}

void mail_msg_cancel(guint msgid)
{
	MailMsg *m;

	g_mutex_lock (mail_msg_lock);

	m = g_hash_table_lookup(mail_msg_active_table, GINT_TO_POINTER(msgid));

	if (m && m->cancel)
		camel_operation_cancel(m->cancel);

	g_mutex_unlock (mail_msg_lock);
}

/* waits for a message to be finished processing (freed)
   the messageid is from MailMsg->seq */
void mail_msg_wait(guint msgid)
{
	MailMsg *m;

	if (mail_in_main_thread ()) {
		g_mutex_lock (mail_msg_lock);
		m = g_hash_table_lookup(mail_msg_active_table, GINT_TO_POINTER(msgid));
		while (m) {
			g_mutex_unlock (mail_msg_lock);
			gtk_main_iteration();
			g_mutex_lock (mail_msg_lock);
			m = g_hash_table_lookup(mail_msg_active_table, GINT_TO_POINTER(msgid));
		}
		g_mutex_unlock (mail_msg_lock);
	} else {
		g_mutex_lock (mail_msg_lock);
		m = g_hash_table_lookup(mail_msg_active_table, GINT_TO_POINTER(msgid));
		while (m) {
			g_cond_wait (mail_msg_cond, mail_msg_lock);
			m = g_hash_table_lookup(mail_msg_active_table, GINT_TO_POINTER(msgid));
		}
		g_mutex_unlock (mail_msg_lock);
	}
}

gint mail_msg_active(guint msgid)
{
	gint active;

	g_mutex_lock (mail_msg_lock);
	if (msgid == (guint)-1)
		active = g_hash_table_size(mail_msg_active_table) > 0;
	else
		active = g_hash_table_lookup(mail_msg_active_table, GINT_TO_POINTER(msgid)) != NULL;
	g_mutex_unlock (mail_msg_lock);

	return active;
}

void mail_msg_wait_all(void)
{
	if (mail_in_main_thread ()) {
		g_mutex_lock (mail_msg_lock);
		while (g_hash_table_size(mail_msg_active_table) > 0) {
			g_mutex_unlock (mail_msg_lock);
			gtk_main_iteration();
			g_mutex_lock (mail_msg_lock);
		}
		g_mutex_unlock (mail_msg_lock);
	} else {
		g_mutex_lock (mail_msg_lock);
		while (g_hash_table_size(mail_msg_active_table) > 0) {
			g_cond_wait (mail_msg_cond, mail_msg_lock);
		}
		g_mutex_unlock (mail_msg_lock);
	}
}

/* **************************************** */

static GHookList cancel_hook_list;

GHook *
mail_cancel_hook_add (GHookFunc func, gpointer data)
{
	GHook *hook;

	g_mutex_lock (mail_msg_lock);

	if (!cancel_hook_list.is_setup)
		g_hook_list_init (&cancel_hook_list, sizeof (GHook));

	hook = g_hook_alloc (&cancel_hook_list);
	hook->func = func;
	hook->data = data;

	g_hook_append (&cancel_hook_list, hook);

	g_mutex_unlock (mail_msg_lock);

	return hook;
}

void
mail_cancel_hook_remove (GHook *hook)
{
	g_mutex_lock (mail_msg_lock);

	g_return_if_fail (cancel_hook_list.is_setup);
	g_hook_destroy_link (&cancel_hook_list, hook);

	g_mutex_unlock (mail_msg_lock);
}

void
mail_cancel_all (void)
{
	camel_operation_cancel (NULL);

	g_mutex_lock (mail_msg_lock);

	if (cancel_hook_list.is_setup)
		g_hook_list_invoke (&cancel_hook_list, FALSE);

	g_mutex_unlock (mail_msg_lock);
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

	if (msg->info->exec != NULL)
		msg->info->exec (msg);

	if (msg->info->desc != NULL && msg->cancel) {
		camel_operation_end (msg->cancel);
		camel_operation_unregister (msg->cancel);
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
	mail_msg_lock = g_mutex_new ();
	mail_msg_cond = g_cond_new ();

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
}

void
mail_msg_fast_ordered_push (gpointer msg)
{
	static GOnce once = G_ONCE_INIT;

	g_once (&once, (GThreadFunc) create_thread_pool, GINT_TO_POINTER (1));

	g_thread_pool_push ((GThreadPool *) once.retval, msg, NULL);
}

void
mail_msg_slow_ordered_push (gpointer msg)
{
	static GOnce once = G_ONCE_INIT;

	g_once (&once, (GThreadFunc) create_thread_pool, GINT_TO_POINTER (1));

	g_thread_pool_push ((GThreadPool *) once.retval, msg, NULL);
}

gboolean
mail_in_main_thread (void)
{
	return (g_thread_self () == main_thread);
}

/* ********************************************************************** */

struct _proxy_msg {
	MailMsg base;

	MailAsyncEvent *ea;
	mail_async_event_t type;

	GThread *thread;
	guint idle_id;

	MailAsyncFunc func;
	gpointer o;
	gpointer event_data;
	gpointer data;
};

static void
do_async_event(struct _proxy_msg *m)
{
	m->thread = g_thread_self ();
	m->func(m->o, m->event_data, m->data);
	m->thread = NULL;

	g_mutex_lock(m->ea->lock);
	m->ea->tasks = g_slist_remove(m->ea->tasks, m);
	g_mutex_unlock(m->ea->lock);
}

static gint
idle_async_event (struct _proxy_msg *m)
{
	m->idle_id = 0;
	do_async_event (m);
	mail_msg_unref (m);

	return FALSE;
}

static MailMsgInfo async_event_info = {
	sizeof (struct _proxy_msg),
	(MailMsgDescFunc) NULL,
	(MailMsgExecFunc) do_async_event,
	(MailMsgDoneFunc) NULL,
	(MailMsgFreeFunc) NULL
};

MailAsyncEvent *
mail_async_event_new (void)
{
	MailAsyncEvent *ea;

	ea = g_malloc0(sizeof(*ea));
	ea->lock = g_mutex_new();

	return ea;
}

guint
mail_async_event_emit (MailAsyncEvent *ea,
                       mail_async_event_t type,
                       MailAsyncFunc func,
                       gpointer o,
                       gpointer event_data,
                       gpointer data)
{
	struct _proxy_msg *m;
	guint id;

	/* We dont have a reply port for this, we dont
	 * care when/if it gets executed, just queue it. */
	m = mail_msg_new(&async_event_info);
	m->func = func;
	m->o = o;
	m->event_data = event_data;
	m->data = data;
	m->ea = ea;
	m->type = type;
	m->thread = NULL;

	id = m->base.seq;
	g_mutex_lock(ea->lock);
	ea->tasks = g_slist_prepend(ea->tasks, m);
	g_mutex_unlock(ea->lock);

	/* We use an idle function instead of our own message port only
	 * because the gui message ports's notification buffer might
	 * overflow and deadlock us. */
	if (type == MAIL_ASYNC_GUI) {
		if (mail_in_main_thread ())
			m->idle_id = g_idle_add (
				(GSourceFunc) idle_async_event, m);
		else
			mail_msg_main_loop_push(m);
	} else
		mail_msg_fast_ordered_push (m);

	return id;
}

gint
mail_async_event_destroy (MailAsyncEvent *ea)
{
	gint id;
	struct _proxy_msg *m;

	g_mutex_lock(ea->lock);
	while (ea->tasks) {
		m = ea->tasks->data;
		id = m->base.seq;
		if (m->thread == g_thread_self ()) {
			g_warning("Destroying async event from inside an event, returning EDEADLK");
			g_mutex_unlock(ea->lock);
			errno = EDEADLK;
			return -1;
		}
		if (m->idle_id > 0) {
			g_source_remove (m->idle_id);
			m->idle_id = 0;
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

	if (mail_in_main_thread ())
		do_call (m);
	else {
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

/* ******************************************************************************** */

struct _op_status_msg {
	MailMsg base;

	CamelOperation *op;
	gchar *what;
	gint pc;
	gpointer data;
};

static void
op_cancelled_cb (EActivity *activity,
                 gpointer user_data)
{
	mail_msg_cancel (GPOINTER_TO_UINT (user_data));
}

static void
op_status_exec (struct _op_status_msg *m)
{
	EShell *shell;
	EShellBackend *shell_backend;
	MailMsg *msg;
	MailMsgPrivate *data;
	gchar *out, *p, *o, c;
	gint pc;

	g_return_if_fail (mail_in_main_thread ());

	shell = e_shell_get_default ();
	shell_backend = e_shell_get_backend_by_name (
		shell, shell_builtin_backend);

	g_mutex_lock (mail_msg_lock);

	msg = g_hash_table_lookup (mail_msg_active_table, m->data);

	if (msg == NULL) {
		g_mutex_unlock (mail_msg_lock);
		return;
	}

	data = msg->priv;

	out = g_alloca (strlen (m->what) * 2 + 1);
	o = out;
	p = m->what;
	while ((c = *p++)) {
		if (c == '%')
			*o++ = '%';
		*o++ = c;
	}
	*o = 0;

	pc = m->pc;

	if (data->activity == NULL) {
		gchar *what;

		/* its being created/removed?  well leave it be */
		if (data->activity_state == 1 || data->activity_state == 3) {
			g_mutex_unlock (mail_msg_lock);
			return;
		} else {
			data->activity_state = 1;

			g_mutex_unlock (mail_msg_lock);
			if (msg->info->desc)
				what = msg->info->desc (msg);
			else if (m->what)
				what = g_strdup (m->what);
			/* uncommenting because message is not very useful for a user, see bug 271734*/
			else {
				what = g_strdup("");
			}

			data->activity = e_activity_new (what);
			e_activity_set_allow_cancel (data->activity, TRUE);
			e_activity_set_percent (data->activity, 0.0);
			e_shell_backend_add_activity (shell_backend, data->activity);

			g_signal_connect (
				data->activity, "cancelled",
				G_CALLBACK (op_cancelled_cb),
				GUINT_TO_POINTER (msg->seq));

			g_free (what);
			g_mutex_lock (mail_msg_lock);
			if (data->activity_state == 3) {
				EActivity *activity;

				activity = g_object_ref (data->activity);

				g_mutex_unlock (mail_msg_lock);
				mail_msg_free (msg);

				if (activity != 0)
					mail_async_event_emit (
						mail_async_event, MAIL_ASYNC_GUI, (MailAsyncFunc) end_event_callback,
								NULL, activity, NULL);
			} else {
				data->activity_state = 2;
				g_mutex_unlock (mail_msg_lock);
			}
			return;
		}
	} else if (data->activity != NULL) {
		e_activity_set_primary_text (data->activity, out);
		e_activity_set_percent (data->activity, pc);
		g_mutex_unlock (mail_msg_lock);
	} else {
		g_mutex_unlock (mail_msg_lock);
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
mail_operation_status (CamelOperation *op,
                       const gchar *what,
                       gint pc,
                       gpointer data)
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

void
mail_mt_set_backend (gchar *backend)
{
	shell_builtin_backend = backend;
}

