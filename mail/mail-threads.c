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

#include "folder-browser-factory.h"

#include "camel/camel-object.h"
#include "mail.h"
#include "mail-threads.h"

#define DEBUG(p) g_print p

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

#if 0
		PERCENTAGE, 
		HIDE_PBAR, 
		SHOW_PBAR, 
#endif

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
 * Stuff needed for blocking
 **/

typedef struct block_info_s {
	GMutex *mutex;
	GCond *cond;
	gboolean val;
} block_info_t;

#define BLOCK_INFO_INIT { NULL, NULL, FALSE }

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
 * @modal_block: a condition maintained so that the
 * calling thread (the dispatch thread) blocks correctly
 * until the user has responded to some kind of modal
 * dialog boxy thing.
 */

static block_info_t modal_block = BLOCK_INFO_INIT;

/**
 * @finish_block: A condition so that the dispatch thread
 * blocks until the main thread has finished the cleanup.
 **/

static block_info_t finish_block = BLOCK_INFO_INIT;

/**
 * @current_message: The current message for the status bar.
 * @busy_status: Whether we are currently busy doing some async operation,
 * for status bar purposes.
 */

static char *current_message = NULL;
static gboolean busy = FALSE;

/**
 * Static prototypes
 **/

static void ui_set_busy (void);
static void ui_unset_busy (void);
static void ui_set_message (const char *message);
static void ui_unset_message (void);

static void block_prepare (block_info_t *info);
static void block_wait (block_info_t *info);
static void block_hold (block_info_t *info);
static void block_release (block_info_t *info);

static void *dispatch (void * data);
static void check_dispatcher (void);
static void check_compipes (void);
static gboolean read_msg (GIOChannel * source, GIOCondition condition,
			  gpointer userdata);

static void show_error (com_msg_t * msg);

static void get_password (com_msg_t * msg);
static void get_password_cb (gchar * string, gpointer data);

static void cleanup_op (com_msg_t * msg);

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
			GDK_THREADS_ENTER ();
			gnome_dialog_run_and_close (GNOME_DIALOG (err_dialog));
			GDK_THREADS_LEAVE ();
			/*gtk_widget_destroy (err_dialog); */
			/*gtk_widget_show (GTK_WIDGET (err_dialog));*/

			g_warning ("Setup failed for `%s': %s",
				   clur->infinitive,
				   camel_exception_get_description (clur->
								    ex));
		}

		free_closure (clur);
		return FALSE;
	}

	if (queue_len == 0) {
		check_compipes ();
		check_dispatcher ();
	} /* else add self to queue */

	write (DISPATCH_WRITER, clur, sizeof (closure_t));
	queue_len++;
	return TRUE;
}

#if 0
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

#endif

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

	block_prepare (&modal_block);
	write (MAIN_WRITER, &msg, sizeof (msg));
	block_wait (&modal_block);

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

	block_prepare (&modal_block);
	write (MAIN_WRITER, &msg, sizeof (msg));
	block_wait (&modal_block);
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

void
mail_operations_get_status (int *busy_return,
			    const char **message_return)
{
	*busy_return = busy;
	*message_return = current_message;
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

		/* Wait for the cleanup to finish before starting our next op */
		block_prepare (&finish_block);
		write (MAIN_WRITER, &msg, sizeof (msg));
		block_wait (&finish_block);
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
		ui_set_message (msg->message);
		ui_set_busy ();
		g_free (msg->message);
		break;
#if 0
	case PERCENTAGE:
		DEBUG (("*** Message -- PERCENTAGE\n"));
		g_warning ("PERCENTAGE operation unsupported");
		break;
	case HIDE_PBAR:
		DEBUG (("*** Message -- HIDE_PBAR\n"));
		g_warning ("HIDE_PBAR operation unsupported");
		break;
	case SHOW_PBAR:
		DEBUG (("*** Message -- SHOW_PBAR\n"));
		g_warning ("HIDE_PBAR operation unsupported");
		break;
#endif

	case MESSAGE:
		DEBUG (("*** Message -- MESSAGE\n"));
		ui_set_message (msg->message);
		g_free (msg->message);
		break;

	case PASSWORD:
		DEBUG (("*** Message -- PASSWORD\n"));
		g_assert (msg->reply);
		g_assert (msg->success);
		get_password (msg);
		break;

	case ERROR:
		DEBUG (("*** Message -- ERROR\n"));
		show_error (msg);
		break;

		/* Don't fall through; dispatch_func does the FINISHED
		 * call for us 
		 */

	case FORWARD_EVENT:
		DEBUG (("*** Message -- FORWARD_EVENT %p\n", msg->event_hook));
		g_assert (msg->event_hook);
		(msg->event_hook) (msg->event_obj, msg->event_event_data, msg->event_user_data);
		break;

	case FINISHED:
		DEBUG (("*** Message -- FINISH %s\n", msg->clur->gerund));
		cleanup_op (msg);
		break;

	default:
		g_warning (_("Corrupted message from dispatching thread?"));
		break;
	}

	GDK_THREADS_LEAVE ();
	g_free (msg);
	return TRUE;
}

/**
 * cleanup_op:
 *
 * Cleanup after a finished operation
 **/

static void
cleanup_op (com_msg_t * msg)
{
	block_hold (&finish_block);

	/* Run the cleanup */

	if (msg->clur->spec->cleanup)
		(msg->clur->spec->cleanup) (msg->clur->in_data,
					    msg->clur->op_data,
					    msg->clur->ex);
	
	/* Tell the dispatch thread that it can start
	 * the next operation */

	block_release (&finish_block);

	/* Print an exception if the cleanup caused one */

	if (camel_exception_is_set (msg->clur->ex) &&
	    msg->clur->ex->id != CAMEL_EXCEPTION_USER_CANCEL) {
		g_warning ("Error on cleanup of `%s': %s",
			   msg->clur->infinitive,
			   camel_exception_get_description (msg->clur->ex));
	}

	free_closure (msg->clur);
	queue_len--;

	ui_unset_busy ();
	ui_unset_message ();
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

	/* Create the dialog */

	err_dialog = gnome_error_dialog (msg->message);
	g_free (msg->message);

	/* Stop the other thread until the user reacts */

	ui_unset_busy ();
	block_hold (&modal_block);

	/* Show the dialog. */

	GDK_THREADS_ENTER ();
	gnome_dialog_run_and_close (GNOME_DIALOG (err_dialog));
	GDK_THREADS_LEAVE ();

	/* Allow the other thread to proceed */

	block_release (&modal_block);
	ui_set_busy ();
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
	int button;

	/* Create the dialog */

	dialog = gnome_request_dialog (msg->secret, msg->message, NULL,
				       0, get_password_cb, msg, NULL);

	/* Stop the other thread */

	ui_unset_busy ();
	block_hold (&modal_block);

	/* Show the dialog (or report an error) */

	if (dialog == NULL) {
		*(msg->success) = FALSE;
		*(msg->reply) = g_strdup (_("Could not create dialog box."));
		button = -1;
	} else {
		*(msg->reply) = NULL;
		/*GDK_THREADS_ENTER ();*/
		button = gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
		/*GDK_THREADS_LEAVE ();*/
	}

	if (button == 1 || *(msg->reply) == NULL) {
		*(msg->success) = FALSE;
		*(msg->reply) = g_strdup (_("User cancelled query."));
	} else if (button > 0) {
		*(msg->success) = TRUE;
	}

	/* Allow the other thread to proceed */
	
	block_release (&modal_block);
	ui_set_busy ();
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

/* ******************** */

/**
 *
 * Thread A calls block_prepare
 * Thread A causes thread B to do something
 * Thread A calls block_wait
 * Thread A continues when thread B calls block_release
 *
 * Thread B gets thread A's message
 * Thread B calls block_hold
 * Thread B does something
 * Thread B calls block_release
 *
 **/

static void
block_prepare (block_info_t *info)
{
	if (info->cond == NULL) {
		info->cond = g_cond_new ();
		info->mutex = g_mutex_new ();
	}

	g_mutex_lock (info->mutex);
	info->val = FALSE;
}

static void
block_wait (block_info_t *info)
{
	g_assert (info->cond);

	while (info->val == FALSE)
		g_cond_wait (info->cond, info->mutex);

	g_mutex_unlock (info->mutex);
}
static void
block_hold (block_info_t *info)
{
	g_assert (info->cond);

	g_mutex_lock (info->mutex);
	info->val = FALSE;
}

static void
block_release (block_info_t *info)
{
	g_assert (info->cond);

	info->val = TRUE;
	g_cond_signal (info->cond);
	g_mutex_unlock (info->mutex);
}

/* ******************** */

/* FIXME FIXME FIXME This is a totally evil hack.  */

static Evolution_ShellView
retrieve_shell_view_interface_from_control (BonoboControl *control)
{
	Bonobo_ControlFrame control_frame;
	Evolution_ShellView shell_view_interface;
	CORBA_Environment ev;

	control_frame = bonobo_control_get_control_frame (control);

	CORBA_exception_init (&ev);
	shell_view_interface = Bonobo_Unknown_query_interface (control_frame,
							       "IDL:Evolution/ShellView:1.0",
							       &ev);
	CORBA_exception_free (&ev);

	if (shell_view_interface != CORBA_OBJECT_NIL)
		gtk_object_set_data (GTK_OBJECT (control),
				     "mail_threads_shell_view_interface",
				     shell_view_interface);
	else
		g_warning ("Control frame doesn't have Evolution/ShellView.");

	return shell_view_interface;
}

static void
update_active_views (void)
{
	GList *active_controls;
	GList *p;

	active_controls = folder_browser_factory_get_active_control_list ();
	for (p = active_controls; p != NULL; p = p->next) {
		BonoboControl *control;
		Evolution_ShellView shell_view_interface;
		CORBA_Environment ev;

		control = BONOBO_CONTROL (p->data);

		shell_view_interface = gtk_object_get_data (GTK_OBJECT (control), "mail_threads_shell_view_interface");

		if (shell_view_interface == CORBA_OBJECT_NIL)
			shell_view_interface = retrieve_shell_view_interface_from_control (control);

		CORBA_exception_init (&ev);

		if (shell_view_interface != CORBA_OBJECT_NIL) {
			if (current_message == NULL && ! busy) {
				Evolution_ShellView_unset_message (shell_view_interface, &ev);
			} else {
				if (current_message == NULL)
					Evolution_ShellView_set_message (shell_view_interface,
									 "",
									 busy,
									 &ev);
				else
					Evolution_ShellView_set_message (shell_view_interface,
									 current_message,
									 busy,
									 &ev);
			}
		}

		CORBA_exception_free (&ev);
	}
}

static void 
ui_set_busy (void)
{
	busy = TRUE;
	update_active_views ();
}

static void 
ui_unset_busy (void)
{
	busy = FALSE;
	update_active_views ();
}

static void 
ui_set_message (const char *message)
{
	g_free (current_message);
	current_message = g_strdup (message);
	update_active_views ();
}

static void 
ui_unset_message (void)
{
	g_free (current_message);
	current_message = NULL;
	update_active_views ();
}
