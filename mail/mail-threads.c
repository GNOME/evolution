/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * Author :
 *  Peter Williams (peterw@helixcode.com)
 *
 *  Copyright 2000, Helix Code, Inc. (http://www.helixcode.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#include <config.h>

#include <string.h>
#include <glib.h>
#include "camel/camel-object.h"
#include "mail.h"
#include "mail-threads.h"

#define DEBUG(p) g_print p

/* FIXME TODO: Do we need operations that don't get a progress window because
 * they're quick, but we still want camel to be locked? We need some kind
 * of flag to mail_operation_queue, but then we also need some kind of monitor
 * to open the window if it takes more than a second or something. That would
 * probably entail another thread....
 */

/**
 * A function and its userdata
 **/

typedef struct closure_s
{
	gpointer in_data;
	gboolean free_in_data;
	gpointer op_data;
	const mail_operation_spec *spec;
	CamelException *ex;
	gchar *infinitive;
	gchar *gerund;
}
closure_t;

/**
 * A command issued through the compipe
 **/

typedef struct com_msg_s
{
	enum com_msg_type_e { 
		STARTING, 
		PERCENTAGE, 
		HIDE_PBAR, 
		SHOW_PBAR, 
		MESSAGE, 
		PASSWORD,
		ERROR, 
		FORWARD_EVENT,
		FINISHED
	} type;
	gfloat percentage;
	gchar *message;

	closure_t *clur;

	/* Password stuff */
	gchar **reply;
	gboolean secret;
	gboolean *success;

	/* Event stuff */
	CamelObjectEventHookFunc event_hook;
	CamelObject *event_obj;
	gpointer event_event_data;
	gpointer event_user_data;
}
com_msg_t;

/**
 * @dispatch_thread_started: gboolean that tells us whether
 * the dispatch thread has been launched.
 **/

static gboolean dispatch_thread_started = FALSE;

/** 
 * @queue_len : the number of operations pending
 * and being executed.
 *
 * Because camel is not thread-safe we work
 * with the restriction that more than one mailbox
 * cannot be accessed at once. Thus we cannot
 * concurrently check mail and move messages, etc.
 **/

static gint queue_len = 0;

/**
 * @queue_window: The little window on the screen that
 * shows the progress of the current operation and the
 * operations that are queued to proceed after it.
 *
 * @queue_window_pending: The vbox that contains the
 * list of pending operations.
 *
 * @queue_window_message: The label that contains the
 * operation's message to the user
 **/

static GtkWidget *queue_window = NULL;
static GtkWidget *queue_window_pending = NULL;
static GtkWidget *queue_window_message = NULL;
static GtkWidget *queue_window_progress = NULL;

/**
 * @progress_timeout_handle: the handle to our timer
 * function so that we can remove it when the progress bar
 * mode changes.
 **/

static int progress_timeout_handle = -1;

/**
 * @main_compipe: The pipe through which the dispatcher communicates
 * with the main thread for GTK+ calls
 *
 * @chan_reader: the GIOChannel that reads our pipe
 *
 * @MAIN_READER: the fd in our main pipe that.... reads!
 * @MAIN_WRITER: the fd in our main pipe that.... writes!
 */

#define MAIN_READER main_compipe[0]
#define MAIN_WRITER main_compipe[1]
#define DISPATCH_READER dispatch_compipe[0]
#define DISPATCH_WRITER dispatch_compipe[1]

static int main_compipe[2] = { -1, -1 };
static int dispatch_compipe[2] = { -1, -1 };

GIOChannel *chan_reader = NULL;

/**
 * @modal_cond: a condition maintained so that the
 * calling thread (the dispatch thread) blocks correctly
 * until the user has responded to some kind of modal
 * dialog boxy thing.
 *
 * @modal_lock: a mutex for said condition
 *
 * @modal_may_proceed: a gboolean telling us whether
 * the dispatch thread may proceed its operations.
 */

G_LOCK_DEFINE_STATIC (modal_lock);
static GCond *modal_cond = NULL;
static gboolean modal_may_proceed = FALSE;

/**
 * @ready_for_op: A lock that the main thread only releases
 * when it is ready for the dispatch thread to do its thing
 *
 * @ready_cond: A condition for this ... condition
 *
 * @ready_may_proceed: a gboolean telling the dispatch thread
 * when it may proceed.
 **/

G_LOCK_DEFINE_STATIC (ready_for_op);
static GCond *ready_cond = NULL;
static gboolean ready_may_proceed = FALSE;

/**
 * Static prototypes
 **/

static void create_queue_window (void);
static void destroy_queue_window (void);
static void *dispatch (void * data);
static void check_dispatcher (void);
static void check_compipes (void);
static void check_cond (void);
static gboolean read_msg (GIOChannel * source, GIOCondition condition,
			  gpointer userdata);
static void remove_next_pending (void);
static void show_error (com_msg_t * msg);
static void show_error_clicked (GtkObject * obj);
static void get_password (com_msg_t * msg);
static void get_password_cb (gchar * string, gpointer data);
static void get_password_clicked (GnomeDialog * dialog, gint button,
				  gpointer user_data);
static void get_password_deleted (GtkWidget *widget, gpointer user_data);

static gboolean progress_timeout (gpointer data);
static void timeout_toggle (gboolean active);
static gboolean display_timeout (gpointer data);
static closure_t *new_closure (const mail_operation_spec * spec, gpointer input,
			       gboolean free_in_data);
static void free_closure (closure_t *clur);

/* Pthread code */
/* FIXME: support other thread types!!!! */

#ifdef G_THREADS_IMPL_POSIX

#include <pthread.h>

/**
 * @dispatch_thread: the pthread_t (when using pthreads, of
 * course) representing our dispatcher routine. Never used
 * except to make pthread_create happy
 **/

static pthread_t dispatch_thread;

/* FIXME: do we need to set any attributes for our thread? 
 * If so, we need to create a pthread_attr structure and
 * fill it in somewhere. But the defaults should be good
 * enough.
 */

#elif defined( G_THREADS_IMPL_SOLARIS )

#include <thread.h>

static thread_t dispatch_thread;

#else /* no supported thread impl */
void
f (void)
{
	Error_No_supported_thread_implementation_recognized ();
	choke on this;
}
#endif

/**
 * mail_operation_queue:
 * @spec: describes the operation to be performed
 * @input: input data for the operation.
 *
 * Runs a mail operation asynchronously. If no other operation is running,
 * we start another thread and call the callback in that thread. The function
 * can then use the mail_op_ functions to perform limited UI returns, while
 * the main UI is completely unlocked.
 *
 * If an async operation is going on when this function is called again, 
 * it waits for the currently executing operation to finish, then
 * executes the callback function in another thread.
 *
 * Returns TRUE on success, FALSE on some sort of queueing error.
 **/

gboolean
mail_operation_queue (const mail_operation_spec * spec, gpointer input,
		      gboolean free_in_data)
{
	closure_t *clur;

	g_assert (spec);

	clur = new_closure (spec, input, free_in_data);

	if (spec->setup)
		(spec->setup) (clur->in_data, clur->op_data, clur->ex);

	if (camel_exception_is_set (clur->ex)) {
		if (clur->ex->id != CAMEL_EXCEPTION_USER_CANCEL) {
			GtkWidget *err_dialog;
			gchar *msg;

			msg =
				g_strdup_printf
				("Error while preparing to %s:\n" "%s",
				 clur->infinitive,
				 camel_exception_get_description (clur->ex));
			err_dialog = gnome_error_dialog (msg);
			g_free (msg);
			gnome_dialog_set_close (GNOME_DIALOG (err_dialog),
						TRUE);
			/*gnome_dialog_run_and_close (GNOME_DIALOG (err_dialog));*/
			/*gtk_widget_destroy (err_dialog); */
			gtk_widget_show (GTK_WIDGET (err_dialog));

			g_warning ("Setup failed for `%s': %s",
				   clur->infinitive,
				   camel_exception_get_description (clur->
								    ex));
		}

		free_closure (clur);
		return FALSE;
	}

	if (queue_len == 0) {
		check_cond ();
		check_compipes ();
		check_dispatcher ();
		create_queue_window ();
		/*gtk_widget_show_all (queue_window); */
		gtk_timeout_add (1000, display_timeout, NULL);
	} else {
		GtkWidget *label;

		/* We already have an operation running. Well,
		 * queue ourselves up. (visually)
		 */

		/* Show us in the pending window. */
		label = gtk_label_new (clur->infinitive);
		gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);
		gtk_box_pack_start (GTK_BOX (queue_window_pending), label,
				    FALSE, TRUE, 2);
		gtk_widget_show (label);

		/* If we want the next op to be on the bottom, uncomment this */
		/* 1 = first on list always (0-based) */
		/* gtk_box_reorder_child( GTK_BOX( queue_window_pending ), label, 1 ); */
		gtk_widget_show (queue_window_pending);
	}

	write (DISPATCH_WRITER, clur, sizeof (closure_t));
	queue_len++;
	return TRUE;
}

/**
 * mail_op_set_percentage:
 * @percentage: the percentage that will be displayed in the progress bar
 *
 * Set the percentage of the progress bar for the currently executing operation.
 * Threadsafe for, nay, intended to be called by, the dispatching thread.
 **/

void
mail_op_set_percentage (gfloat percentage)
{
	com_msg_t msg;

	msg.type = PERCENTAGE;
	msg.percentage = percentage;
	write (MAIN_WRITER, &msg, sizeof (msg));
}

/**
 * mail_op_hide_progressbar:
 *
 * Hide the progress bar in the status box
 * Threadsafe for, nay, intended to be called by, the dispatching thread.
 **/

/* FIXME: I'd much rather have one of those Netscape-style progress
 * bars that just zips back and forth, but gtkprogressbar can't do
 * that, right? 
 */

void
mail_op_hide_progressbar (void)
{
	com_msg_t msg;

	msg.type = HIDE_PBAR;
	write (MAIN_WRITER, &msg, sizeof (msg));
}

/**
 * mail_op_show_progressbar:
 *
 * Show the progress bar in the status box
 * Threadsafe for, nay, intended to be called by, the dispatching thread.
 **/

void
mail_op_show_progressbar (void)
{
	com_msg_t msg;

	msg.type = SHOW_PBAR;
	write (MAIN_WRITER, &msg, sizeof (msg));
}

/**
 * mail_op_set_message:
 * @fmt: printf-style format string for the message
 * @...: arguments to the format string
 *
 * Set the message displayed above the progress bar for the currently
 * executing operation.
 * Threadsafe for, nay, intended to be called by, the dispatching thread.
 **/

void
mail_op_set_message (gchar * fmt, ...)
{
	com_msg_t msg;
	va_list val;

	va_start (val, fmt);
	msg.type = MESSAGE;
	msg.message = g_strdup_vprintf (fmt, val);
	va_end (val);

	write (MAIN_WRITER, &msg, sizeof (msg));
}

/**
 * mail_op_get_password:
 * @prompt: the question put to the user
 * @secret: whether the dialog box shold print stars when the user types
 * @dest: where to store the reply
 *
 * Asks the user for a password (or string entry in general). Waits for
 * the user's response. On success, returns TRUE and @dest contains the
 * response. On failure, returns FALSE and @dest contains the error
 * message.
 **/

gboolean
mail_op_get_password (gchar * prompt, gboolean secret, gchar ** dest)
{
	com_msg_t msg;
	gboolean result;

	msg.type = PASSWORD;
	msg.secret = secret;
	msg.message = prompt;
	msg.reply = dest;
	msg.success = &result;

	(*dest) = NULL;

	G_LOCK (modal_lock);

	write (MAIN_WRITER, &msg, sizeof (msg));
	modal_may_proceed = FALSE;

	while (modal_may_proceed == FALSE)
		g_cond_wait (modal_cond,
			     g_static_mutex_get_mutex (&G_LOCK_NAME
						       (modal_lock)));

	G_UNLOCK (modal_lock);

	return result;
}

/**
 * mail_op_error:
 * @fmt: printf-style format string for the error
 * @...: arguments to the format string
 *
 * Opens an error dialog for the currently executing operation.
 * Threadsafe for, nay, intended to be called by, the dispatching thread.
 **/

void
mail_op_error (gchar * fmt, ...)
{
	com_msg_t msg;
	va_list val;

	va_start (val, fmt);
	msg.type = ERROR;
	msg.message = g_strdup_vprintf (fmt, val);
	va_end (val);

	G_LOCK (modal_lock);

	modal_may_proceed = FALSE;
	write (MAIN_WRITER, &msg, sizeof (msg));

	while (modal_may_proceed == FALSE)
		g_cond_wait (modal_cond,
			     g_static_mutex_get_mutex (&G_LOCK_NAME
						       (modal_lock)));

	G_UNLOCK (modal_lock);
}

/**
 * mail_op_forward_event:
 *
 * Communicate a camel event over to the main thread.
 **/

void
mail_op_forward_event (CamelObjectEventHookFunc func, CamelObject *o, 
		       gpointer event_data, gpointer user_data)
{
	com_msg_t msg;

	msg.type = FORWARD_EVENT;
	msg.event_hook = func;
	msg.event_obj = o;
	msg.event_event_data = event_data;
	msg.event_user_data = user_data;
	write (MAIN_WRITER, &msg, sizeof (msg));
}
/**
 * mail_operation_wait_for_finish:
 *
 * Waits for the currently executing async operations
 * to finish executing
 */

void
mail_operation_wait_for_finish (void)
{
	while (queue_len)
		gtk_main_iteration ();
	/* Sigh. Otherwise we deadlock upon exit. */
	GDK_THREADS_LEAVE ();
}

/**
 * mail_operations_are_executing:
 *
 * Returns TRUE if operations are being executed asynchronously
 * when called, FALSE if not.
 **/

gboolean
mail_operations_are_executing (void)
{
	return (queue_len > 0);
}

/**
 * mail_operations_terminate:
 *
 * Let the operations finish then terminate the dispatch thread
 **/

void
mail_operations_terminate (void)
{
	closure_t clur;

	mail_operation_wait_for_finish();

	memset (&clur, 0, sizeof (closure_t));
	clur.spec = NULL;

	write (DISPATCH_WRITER, &clur, sizeof (closure_t));
}

/* ** Static functions **************************************************** */

static void check_dispatcher (void)
{
	int res;

	if (dispatch_thread_started)
		return;

#if defined( G_THREADS_IMPL_POSIX )
	res = pthread_create (&dispatch_thread, NULL,
			      (void *) &dispatch, NULL);
#elif defined( G_THREADS_IMPL_SOLARIS )
	res = thr_create (NULL, 0, (void *) &dispatch, NULL, 0, &dispatch_thread);
#else /* no known impl */
	Error_No_thread_create_implementation ();
	choke on this;
#endif
	if (res != 0) {
		g_warning ("Error launching dispatch thread!");
		/* FIXME: more error handling */
	} else
		dispatch_thread_started = TRUE;
}

static void
print_hide (GtkWidget * wid)
{
	g_message ("$$$ hide signal emitted");
}

static void
print_unmap (GtkWidget * wid)
{
	g_message ("$$$ unmap signal emitted");
}

static void
print_map (GtkWidget * wid)
{
	g_message ("$$$ map signal emitted");
}

static void
print_show (GtkWidget * wid)
{
	g_message ("$$$ show signal emitted");
}

/**
 * create_queue_window:
 *
 * Creates the queue_window widget that displays the progress of the
 * current operation.
 */

static void
queue_window_delete_event_cb (GtkWindow *window,
			      void *data)
{
	/* Do nothing.  Just prevent GTK+ from destroying the window.  */
}

static void
create_queue_window (void)
{
	GtkWidget *vbox;
	GtkWidget *pending_vb, *pending_lb;
	GtkWidget *progress_lb, *progress_bar;

	/* Check to see if we've only hidden it */
	if (queue_window != NULL)
		return;

	queue_window = gtk_window_new (GTK_WINDOW_DIALOG);
	gtk_container_set_border_width (GTK_CONTAINER (queue_window), 8);

	gtk_signal_connect (GTK_OBJECT (queue_window), "delete_event",
			    GTK_SIGNAL_FUNC (queue_window_delete_event_cb), NULL);

	vbox = gtk_vbox_new (FALSE, 4);

	pending_vb = gtk_vbox_new (FALSE, 2);
	queue_window_pending = pending_vb;

	pending_lb = gtk_label_new (_("Currently pending operations:"));
	gtk_misc_set_alignment (GTK_MISC (pending_lb), 0.0, 0.0);
	gtk_box_pack_start (GTK_BOX (pending_vb), pending_lb, FALSE, TRUE, 0);

	gtk_box_pack_start (GTK_BOX (vbox), pending_vb, TRUE, TRUE, 4);

	/* FIXME: 'operation' is not the warmest cuddliest word. */
	progress_lb = gtk_label_new ("");
	queue_window_message = progress_lb;
	gtk_box_pack_start (GTK_BOX (vbox), progress_lb, FALSE, TRUE, 4);

	progress_bar = gtk_progress_bar_new ();
	queue_window_progress = progress_bar;
	/* FIXME: is this fit for l10n? */
	gtk_progress_bar_set_orientation (GTK_PROGRESS_BAR (progress_bar),
					  GTK_PROGRESS_LEFT_TO_RIGHT);
	gtk_progress_bar_set_bar_style (GTK_PROGRESS_BAR (progress_bar),
					GTK_PROGRESS_CONTINUOUS);
	gtk_box_pack_start (GTK_BOX (vbox), progress_bar, FALSE, TRUE, 4);

	gtk_container_add (GTK_CONTAINER (queue_window), vbox);

	gtk_widget_show (GTK_WIDGET (progress_bar));
	gtk_widget_show (GTK_WIDGET (progress_lb));
	gtk_widget_show (GTK_WIDGET (pending_lb));
	gtk_widget_show (GTK_WIDGET (pending_vb));
	gtk_widget_show (GTK_WIDGET (vbox));

	gtk_signal_connect (GTK_OBJECT (queue_window), "hide", print_hide,
			    NULL);
	gtk_signal_connect (GTK_OBJECT (queue_window), "unmap", print_unmap,
			    NULL);
	gtk_signal_connect (GTK_OBJECT (queue_window), "show", print_show,
			    NULL);
	gtk_signal_connect (GTK_OBJECT (queue_window), "map", print_map,
			    NULL);
}

static void destroy_queue_window (void)
{
	g_return_if_fail (queue_window);

	timeout_toggle (FALSE);
	gtk_widget_destroy (queue_window);

	queue_window = NULL;
	queue_window_progress = NULL;
	queue_window_pending = NULL;
	queue_window_message = NULL;
}

/**
 * check_compipes:
 *
 * Check and see if our pipe has been opened and open
 * it if necessary.
 **/

static void
check_compipes (void)
{
	if (MAIN_READER < 0) {
		if (pipe (main_compipe) < 0) {
			g_warning ("Call to pipe(2) failed!");

			/* FIXME: better error handling. How do we react? */
			return;
		}
		
		chan_reader = g_io_channel_unix_new (MAIN_READER);
		g_io_add_watch (chan_reader, G_IO_IN, read_msg, NULL);
	}

	if (DISPATCH_READER < 0) {
		if (pipe (dispatch_compipe) < 0) {
			g_warning ("Call to pipe(2) failed!");

			/* FIXME: better error handling. How do we react? */
			return;
		}
	}
}

/**
 * check_cond:
 *
 * See if our condition is initialized and do so if necessary
 **/

static void
check_cond (void)
{
	if (modal_cond == NULL)
		modal_cond = g_cond_new ();

	if (ready_cond == NULL)
		ready_cond = g_cond_new ();
}

/**
 * dispatch:
 * @clur: The operation to execute and its parameters
 *
 * Start a thread that executes the closure and exit
 * it when done.
 */

static void *
dispatch (void *unused)
{
	size_t len;
	closure_t *clur;
	com_msg_t msg;

	/* Let the compipes be created */
	sleep (1);

	while (1) {
		clur = g_new (closure_t, 1);
		len = read (DISPATCH_READER, clur, sizeof (closure_t));

		if (len <= 0)
			break;

		if (len != sizeof (closure_t)) {
			g_warning ("dispatcher: Didn't read full message!");
			continue;
		}

		if (clur->spec == NULL)
			break;

		msg.type = STARTING;
		msg.message = g_strdup (clur->gerund);
		write (MAIN_WRITER, &msg, sizeof (msg));

		mail_op_hide_progressbar ();
		
		(clur->spec->callback) (clur->in_data, clur->op_data, clur->ex);

		if (camel_exception_is_set (clur->ex)) {
			if (clur->ex->id != CAMEL_EXCEPTION_USER_CANCEL) {
				g_warning ("Callback failed for `%s': %s",
					   clur->infinitive,
					   camel_exception_get_description (clur->
									    ex));
				mail_op_error ("Error while `%s':\n" "%s",
					       clur->gerund,
					       camel_exception_get_description (clur->
										ex));
			}
		}

		msg.type = FINISHED;
		msg.clur = clur;

		G_LOCK (ready_for_op);
		write (MAIN_WRITER, &msg, sizeof (msg));

		ready_may_proceed = FALSE;
		while (ready_may_proceed == FALSE)
			g_cond_wait (ready_cond,
				     g_static_mutex_get_mutex (&G_LOCK_NAME
							       (ready_for_op)));
		G_UNLOCK (ready_for_op);
	}

#ifdef G_THREADS_IMPL_POSIX
	pthread_exit (0);
#elif defined( G_THREADS_IMPL_SOLARIS )
	thr_exit (NULL);
#else /* no known impl */
	Error_No_thread_exit_implemented ();
	choke on this;
#endif
	return NULL;
	/*NOTREACHED*/
}

/**
 * read_msg:
 * @source: the channel that has data to read
 * @condition: the reason we were called
 * @userdata: unused
 *
 * A message has been recieved on our pipe; perform the appropriate 
 * action.
 **/

static gboolean
read_msg (GIOChannel * source, GIOCondition condition, gpointer userdata)
{
	com_msg_t *msg;
	guint size;

	msg = g_new0 (com_msg_t, 1);

	g_io_channel_read (source, (gchar *) msg,
			   sizeof (com_msg_t) / sizeof (gchar), &size);

	if (size != sizeof (com_msg_t)) {
		g_warning (_("Incomplete message written on pipe!"));
		msg->type = ERROR;
		msg->message =
			g_strdup (_
				  ("Error reading commands from dispatching thread."));
	}

	/* This is very important, though I'm not quite sure why
	 * it is as we are in the main thread right now.
	 */

	GDK_THREADS_ENTER ();

	switch (msg->type) {
	case STARTING:
		DEBUG (("*** Message -- STARTING %s\n", msg->message));
		gtk_label_set_text (GTK_LABEL (queue_window_message),
				    msg->message);
		gtk_progress_bar_update (GTK_PROGRESS_BAR
					 (queue_window_progress), 0.0);
		g_free (msg->message);
		g_free (msg);
		break;
	case PERCENTAGE:
		DEBUG (("*** Message -- PERCENTAGE\n"));
		gtk_progress_bar_update (GTK_PROGRESS_BAR
					 (queue_window_progress),
					 msg->percentage);
		g_free (msg);
		break;
	case HIDE_PBAR:
		DEBUG (("*** Message -- HIDE_PBAR\n"));
		gtk_progress_set_activity_mode (GTK_PROGRESS
						(queue_window_progress),
						TRUE);
		timeout_toggle (TRUE);
		g_free (msg);
		break;
	case SHOW_PBAR:
		DEBUG (("*** Message -- SHOW_PBAR\n"));
		timeout_toggle (FALSE);
		gtk_progress_set_activity_mode (GTK_PROGRESS
						(queue_window_progress),
						FALSE);
		g_free (msg);
		break;
	case MESSAGE:
		DEBUG (("*** Message -- MESSAGE\n"));
		gtk_label_set_text (GTK_LABEL (queue_window_message),
				    msg->message);
		g_free (msg->message);
		g_free (msg);
		break;
	case PASSWORD:
		DEBUG (("*** Message -- PASSWORD\n"));
		g_assert (msg->reply);
		g_assert (msg->success);
		get_password (msg);
		/* don't free msg! done later */
		break;
	case ERROR:
		DEBUG (("*** Message -- ERROR\n"));
		show_error (msg);
		g_free (msg);
		break;

		/* Don't fall through; dispatch_func does the FINISHED
		 * call for us 
		 */

	case FORWARD_EVENT:
		DEBUG (("*** Message -- FORWARD_EVENT %p\n", msg->event_hook));

		g_assert (msg->event_hook);
		(msg->event_hook) (msg->event_obj, msg->event_event_data, msg->event_user_data);
		g_free (msg);
		break;

	case FINISHED:
		DEBUG (
		       ("*** Message -- FINISH %s\n",
			msg->clur->gerund));

		if (msg->clur->spec->cleanup)
			(msg->clur->spec->cleanup) (msg->clur->in_data,
						    msg->clur->op_data,
						    msg->clur->ex);

		G_LOCK (ready_for_op);
		ready_may_proceed = TRUE;
		g_cond_signal (ready_cond);
		G_UNLOCK (ready_for_op);

		if (camel_exception_is_set (msg->clur->ex) &&
		    msg->clur->ex->id != CAMEL_EXCEPTION_USER_CANCEL) {
			g_warning ("Error on cleanup of `%s': %s",
				   msg->clur->infinitive,
				   camel_exception_get_description (msg->
								    clur->
								    ex));
		}

		free_closure (msg->clur);
		queue_len--;

		if (queue_len == 0) {
			g_print ("\tNo more ops -- hide %p.\n", queue_window);
			/* All done! */
			/* gtk_widget_hide seems to have problems sometimes 
			 * here... perhaps because we're in a gsource handler,
			 * not a GTK event handler? Anyway, we defer the hiding
			 * til an idle. */
			/*gtk_idle_add (hide_queue_window, NULL);*/
			/*gtk_widget_hide (queue_window); */
			destroy_queue_window ();
		} else {
			g_print ("\tOperation(s) left.\n");

			/* There's another operation left :
			 * Clear it out of the 'pending' vbox 
			 */
			remove_next_pending ();
		}
		g_free (msg);
		break;
	default:
		g_warning (_("Corrupted message from dispatching thread?"));
		break;
	}

	GDK_THREADS_LEAVE ();
	return TRUE;
}

/**
 * remove_next_pending:
 *
 * Remove an item from the list of pending items. If
 * that's the last one, additionally hide the little
 * 'pending' message.
 **/

static void
remove_next_pending (void)
{
	GList *children;

	children =
		gtk_container_children (GTK_CONTAINER (queue_window_pending));

	/* Skip past the header label */
	children = g_list_first (children);
	children = g_list_next (children);

	if (!children) {
		g_warning ("Mistake in queue window!");
		return;
	}

	/* Nuke the one on top */
	gtk_container_remove (GTK_CONTAINER (queue_window_pending),
			      GTK_WIDGET (children->data));

	/* Hide it? */
	if (g_list_next (children) == NULL)
		gtk_widget_hide (queue_window_pending);
}

/**
 * show_error:
 *
 * Show the error dialog and wait for user OK
 **/

static void
show_error (com_msg_t * msg)
{
	GtkWidget *err_dialog;
	gchar *old_message;

	err_dialog = gnome_error_dialog (msg->message);
	gnome_dialog_set_close (GNOME_DIALOG (err_dialog), TRUE);
	gtk_signal_connect (GTK_OBJECT (err_dialog), "close",
			    (GtkSignalFunc) show_error_clicked, NULL);
	gtk_signal_connect (GTK_OBJECT (err_dialog), "clicked",
			    (GtkSignalFunc) show_error_clicked, NULL);
	g_free (msg->message);

	/* Save the old message, but display a new one right now */
	gtk_label_get (GTK_LABEL (queue_window_message), &old_message);
	gtk_object_set_data (GTK_OBJECT (err_dialog), "old_message",
			     g_strdup (old_message));
	gtk_label_set_text (GTK_LABEL (queue_window_message),
			    _("Waiting for user to close error dialog"));

	G_LOCK (modal_lock);

	timeout_toggle (FALSE);
	modal_may_proceed = FALSE;
	/*gtk_widget_show_all (GTK_WIDGET (err_dialog));*/
	gnome_dialog_run_and_close (GNOME_DIALOG (err_dialog));
	/*
	 *gnome_win_hints_set_layer (err_dialog, WIN_LAYER_ONTOP);
	 *gnome_win_hints_set_state (err_dialog, WIN_STATE_ARRANGE_IGNORE);
	 *gnome_win_hints_set_hints (err_dialog,
	 *			   WIN_HINTS_SKIP_FOCUS |
	 *			   WIN_HINTS_SKIP_WINLIST |
	 *			   WIN_HINTS_SKIP_TASKBAR);
	 */
}

/**
 * show_error_clicked:
 *
 * Called when the user makes hits okay to the error dialog --
 * the dispatch thread is allowed to continue.
 **/

static void
show_error_clicked (GtkObject * obj)
{
	gchar *old_message;

	gtk_signal_disconnect_by_func (GTK_OBJECT (obj), show_error_clicked, NULL);

	/* Restore the old message */
	old_message = gtk_object_get_data (obj, "old_message");
	gtk_label_set_text (GTK_LABEL (queue_window_message),
			    old_message);
	g_free (old_message);

	modal_may_proceed = TRUE;
	timeout_toggle (TRUE);
	g_cond_signal (modal_cond);
	G_UNLOCK (modal_lock);
}

/**
 * get_password:
 *
 * Ask for a password and put the answer in *(msg->reply)
 **/

static void
get_password (com_msg_t * msg)
{
	GtkWidget *dialog;
	gchar *old_message;

	dialog = gnome_request_dialog (msg->secret, msg->message, NULL,
				       0, get_password_cb, msg, NULL);
	gnome_dialog_set_close (GNOME_DIALOG (dialog), TRUE);
	gtk_signal_connect (GTK_OBJECT (dialog), "clicked",
			    get_password_clicked, msg);
	gtk_signal_connect (GTK_OBJECT (dialog), "close",
			    get_password_deleted, msg);

	/* Save the old message, but display a new one right now */
	gtk_label_get (GTK_LABEL (queue_window_message), &old_message);
	gtk_object_set_data (GTK_OBJECT (dialog), "old_message", g_strdup(old_message));
	gtk_label_set_text (GTK_LABEL (queue_window_message),
			    _("Waiting for user to enter data"));

	G_LOCK (modal_lock);

	modal_may_proceed = FALSE;

	if (dialog == NULL) {
		*(msg->success) = FALSE;
		*(msg->reply) = g_strdup (_("Could not create dialog box."));
		modal_may_proceed = TRUE;
		g_cond_signal (modal_cond);
		G_UNLOCK (modal_lock);
	} else {
		*(msg->reply) = NULL;
		timeout_toggle (FALSE);
		/*
		 *gtk_widget_show_all (GTK_WIDGET (dialog));
		 *gnome_win_hints_set_layer (dialog, WIN_LAYER_ONTOP);
		 *gnome_win_hints_set_state (dialog, WIN_STATE_ARRANGE_IGNORE);
		 *gnome_win_hints_set_hints (dialog,
		 *			   WIN_HINTS_SKIP_FOCUS |
		 *			   WIN_HINTS_SKIP_WINLIST |
		 *			   WIN_HINTS_SKIP_TASKBAR);
		 */
		gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
	}
}

static void
get_password_cb (gchar * string, gpointer data)
{
	com_msg_t *msg = (com_msg_t *) data;

	if (string)
		*(msg->reply) = g_strdup (string);
	else
		*(msg->reply) = NULL;
}

static void
get_password_deleted (GtkWidget *widget, gpointer user_data)
{
	get_password_clicked (GNOME_DIALOG (widget), 1, user_data);
}

static void
get_password_clicked (GnomeDialog * dialog, gint button, gpointer user_data)
{
	com_msg_t *msg = (com_msg_t *) user_data;
	gchar *old_message;

	gtk_signal_disconnect_by_func (GTK_OBJECT (dialog), get_password_deleted, user_data);

	/* Restore the old message */
	old_message = gtk_object_get_data (GTK_OBJECT (dialog), "old_message");
	gtk_label_set_text (GTK_LABEL (queue_window_message),
			    old_message);
	g_free (old_message);

	if (button == 1 || *(msg->reply) == NULL) {
		*(msg->success) = FALSE;
		*(msg->reply) = g_strdup (_("User cancelled query."));
	} else
		*(msg->success) = TRUE;

	g_free (msg);
	modal_may_proceed = TRUE;
	timeout_toggle (TRUE);
	g_cond_signal (modal_cond);
	G_UNLOCK (modal_lock);
}

/* NOT totally copied from gtk+/gtk/testgtk.c, really! */

static gboolean
progress_timeout (gpointer data)
{
	gfloat new_val;
	GtkAdjustment *adj;

	if (queue_window == NULL) {
		gtk_timeout_remove (progress_timeout_handle);
		progress_timeout_handle = -1;
		return FALSE;
	}
		
	adj = GTK_PROGRESS (data)->adjustment;

	new_val = adj->value + 1;
	if (new_val > adj->upper)
		new_val = adj->lower;

	gtk_progress_set_value (GTK_PROGRESS (data), new_val);

	return TRUE;
}

/**
 * timeout_toggle:
 *
 * Turn on and off our timeout to zip the progressbar along,
 * protecting against recursion (Ie, call with TRUE twice
 * in a row.
 **/

static void
timeout_toggle (gboolean active)
{
	if (!queue_window)
		return;

	if ((GTK_PROGRESS (queue_window_progress))->activity_mode == 0)
		return;

	if (active) {
		/* We do this in case queue_window_progress gets reset */
		if (progress_timeout_handle < 0) {
			progress_timeout_handle =
				gtk_timeout_add (80, progress_timeout,
						 queue_window_progress);
		} else {
			gtk_timeout_remove (progress_timeout_handle);
			progress_timeout_handle =
				gtk_timeout_add (80, progress_timeout,
						 queue_window_progress);
		}
	} else {
		if (progress_timeout_handle >= 0) {
			gtk_timeout_remove (progress_timeout_handle);
			progress_timeout_handle = -1;
		}
	}
}

/* This can theoretically run into problems where if a short operation starts
 * and finishes, then another short operation starts and finishes a second
 * later, we will see the window prematurely. My response: oh noooooo!
 *
 * Solution: keep the timeout's handle and remove the timeout upon reception
 * of FINISH, and zero out the handle in this function. Whatever.
 */
static gboolean
display_timeout (gpointer data)
{
	if (queue_len > 0 && queue_window) {
		gtk_widget_show (queue_window);
		gnome_win_hints_set_layer (queue_window, WIN_LAYER_ONTOP);
		gnome_win_hints_set_state (queue_window,
					   WIN_STATE_ARRANGE_IGNORE);
		gnome_win_hints_set_hints (queue_window,
					   WIN_HINTS_SKIP_FOCUS |
					   WIN_HINTS_SKIP_WINLIST |
					   WIN_HINTS_SKIP_TASKBAR);

		if (queue_len == 1)
			gtk_widget_hide (queue_window_pending);
	}

	return FALSE;
}

static closure_t *
new_closure (const mail_operation_spec * spec, gpointer input,
	     gboolean free_in_data)
{
	closure_t *clur;

	clur = g_new0 (closure_t, 1);
	clur->spec = spec;
	clur->in_data = input;
	clur->free_in_data = free_in_data;
	clur->ex = camel_exception_new ();

	clur->op_data = g_malloc (spec->datasize);

	camel_exception_init (clur->ex);

	clur->infinitive = (spec->describe) (input, FALSE);
	clur->gerund = (spec->describe) (input, TRUE);

	return clur;
}

static void
free_closure (closure_t *clur)
{
	clur->spec = NULL;

	if (clur->free_in_data)
		g_free (clur->in_data);
	clur->in_data = NULL;

	g_free (clur->op_data);
	clur->op_data = NULL;

	camel_exception_free (clur->ex);
	clur->ex = NULL;

	g_free (clur->infinitive);
	g_free (clur->gerund);

	g_free (clur);
}
