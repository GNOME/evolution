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

#ifdef USE_BROKEN_THREADS

#include <string.h>
#include <glib.h>
#include "mail.h"
#include "mail-threads.h"

#define DEBUG(p) g_print p

/* FIXME TODO: Do we need operations that don't get a progress window because
 * they're quick, but we still want camel to be locked? We need some kind
 * of flag to mail_operation_try, but then we also need some kind of monitor
 * to open the window if it takes more than a second or something. That would
 * probably entail another thread....
 */

/**
 * A function and its userdata
 **/

typedef struct closure_s {
	void (*callback)( gpointer );
	void (*cleanup)( gpointer );
	gpointer data;
	
	gchar *prettyname;
	/* gboolean gets_window; */
} closure_t;

/**
 * A command issued through the compipe
 **/

typedef struct com_msg_s {
	enum com_msg_type_e { STARTING, PERCENTAGE, HIDE_PBAR, SHOW_PBAR, MESSAGE, PASSWORD, ERROR, FINISHED } type;
	gfloat percentage;
	gchar *message;

	void (*func)( gpointer );
	gpointer userdata;

	/* Password stuff */
	gchar **reply;
	gboolean secret;
	gboolean *success;
} com_msg_t;

/** 
 * @mail_operation_in_progress: When true, there's
 * another thread executing a major ev-mail operation:
 * fetch_mail, etc.
 *
 * Because camel is not thread-safe we work
 * with the restriction that more than one mailbox
 * cannot be accessed at once. Thus we cannot
 * concurrently check mail and move messages, etc.
 **/

static gboolean mail_operation_in_progress;

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
 * @op_queue: The list of operations the are scheduled
 * to proceed after the currently executing one. When
 * only one operation is going, this is NULL.
 **/

static GSList *op_queue = NULL;

/**
 * @compipe: The pipe through which the dispatcher communicates
 * with the main thread for GTK+ calls
 *
 * @chan_reader: the GIOChannel that reads our pipe
 *
 * @READER: the fd in our pipe that.... reads!
 * @WRITER: the fd in our pipe that.... writes!
 */

#define READER compipe[0]
#define WRITER compipe[1]

static int compipe[2] = { -1, -1 };

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

G_LOCK_DEFINE_STATIC( modal_lock );
static GCond *modal_cond = NULL;
static gboolean modal_may_proceed = FALSE;

/**
 * Static prototypes
 **/

static void create_queue_window( void );
static void dispatch( closure_t *clur );
static void *dispatch_func( void *data );
static void check_compipe( void );
static void check_cond( void );
static gboolean read_msg( GIOChannel *source, GIOCondition condition, gpointer userdata );
static void remove_next_pending( void );
static void show_error( com_msg_t *msg );
static void show_error_clicked( void );
static void get_password( com_msg_t *msg );
static void get_password_cb( gchar *string, gpointer data );
static void get_password_clicked( GnomeDialog *dialog, gint button, gpointer user_data );
static gboolean progress_timeout( gpointer data );
static void timeout_toggle( gboolean active );

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

#else /* defined USE_PTHREADS */
choke on this: no thread type defined
#endif

/**
 * mail_operation_try:
 * @description: A user-friendly string describing the operation.
 * @callback: the function to call in another thread to start the operation
 * @cleanup: the function to call in the main thread when the callback is finished.
 *    NULL is allowed.
 * @user_data: extra data passed to the callback
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
mail_operation_try( const gchar *description, void (*callback)( gpointer ), 
		    void (*cleanup)( gpointer ), gpointer user_data )
{
	closure_t *clur;
	g_assert( callback );

	clur = g_new( closure_t, 1 );
	clur->callback = callback;
	clur->cleanup = cleanup;
	clur->data = user_data;
	clur->prettyname = g_strdup( description );

	if( mail_operation_in_progress == FALSE ) {
		/* No operations are going on, none are pending. So
		 * we check to see if we're initialized (create the
		 * window and the pipes), and send off the operation
		 * on its merry way.
		 */

		mail_operation_in_progress = TRUE;

		check_compipe();
		create_queue_window();
		gtk_widget_show_all( queue_window );
		gnome_win_hints_set_layer( queue_window, 
					   WIN_LAYER_ONTOP );
		gnome_win_hints_set_state( queue_window, 
					   WIN_STATE_ARRANGE_IGNORE );
		gnome_win_hints_set_hints( queue_window, 
					   WIN_HINTS_SKIP_FOCUS |
					   WIN_HINTS_SKIP_WINLIST |
					   WIN_HINTS_SKIP_TASKBAR );
		gtk_widget_hide( queue_window_pending );

		dispatch( clur );
	} else {
		GtkWidget *label;

		/* Zut. We already have an operation running. Well,
		 * queue ourselves up.
		 *
		 * Yes, g_slist_prepend is faster down here.. But we pop
		 * operations off the beginning of the list later and
		 * that's a lot faster.
		 */

		op_queue = g_slist_append( op_queue, clur );

		/* Show us in the pending window. */
		label = gtk_label_new( description );
		gtk_misc_set_alignment( GTK_MISC( label ), 1.0, 0.5 );
		gtk_box_pack_start( GTK_BOX( queue_window_pending ), label,
				    FALSE, TRUE, 2 );

		/* If we want the next op to be on the bottom, uncomment this */
		/* 1 = first on list always (0-based) */
		/* gtk_box_reorder_child( GTK_BOX( queue_window_pending ), label, 1 ); */
		gtk_widget_show_all( queue_window_pending );
	}

	return TRUE;
}

/**
 * mail_op_set_percentage:
 * @percentage: the percentage that will be displayed in the progress bar
 *
 * Set the percentage of the progress bar for the currently executing operation.
 * Threadsafe for, nay, intended to be called by, the dispatching thread.
 **/

void mail_op_set_percentage( gfloat percentage )
{
	com_msg_t msg;

	msg.type = PERCENTAGE;
	msg.percentage = percentage;
	write( WRITER, &msg, sizeof( msg ) );
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

void mail_op_hide_progressbar( void )
{
	com_msg_t msg;

	msg.type = HIDE_PBAR;
	write( WRITER, &msg, sizeof( msg ) );
}

/**
 * mail_op_show_progressbar:
 *
 * Show the progress bar in the status box
 * Threadsafe for, nay, intended to be called by, the dispatching thread.
 **/

void mail_op_show_progressbar( void )
{
	com_msg_t msg;

	msg.type = SHOW_PBAR;
	write( WRITER, &msg, sizeof( msg ) );
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

void mail_op_set_message( gchar *fmt, ... )
{
	com_msg_t msg;
	va_list val;

	va_start( val, fmt );
	msg.type = MESSAGE;
	msg.message = g_strdup_vprintf( fmt, val );
	va_end( val );

	write( WRITER, &msg, sizeof( msg ) );
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

gboolean mail_op_get_password( gchar *prompt, gboolean secret, gchar **dest )
{
	com_msg_t msg;
	gboolean result;

	check_cond();

	msg.type = PASSWORD;
	msg.secret = secret;
	msg.message = prompt;
	msg.reply = dest;
	msg.success = &result;
	
	(*dest) = NULL;

	G_LOCK( modal_lock );

	write( WRITER, &msg, sizeof( msg ) );
	modal_may_proceed = FALSE;

	while( modal_may_proceed == FALSE )
		g_cond_wait( modal_cond, g_static_mutex_get_mutex( &G_LOCK_NAME( modal_lock ) ) );

	G_UNLOCK( modal_lock );

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

void mail_op_error( gchar *fmt, ... )
{
	com_msg_t msg;
	va_list val;

	check_cond();

	va_start( val, fmt );
	msg.type = ERROR;
	msg.message = g_strdup_vprintf( fmt, val );
	va_end( val );

	G_LOCK( modal_lock );

	modal_may_proceed = FALSE;
	write( WRITER, &msg, sizeof( msg ) );

	while( modal_may_proceed == FALSE )
		g_cond_wait( modal_cond, g_static_mutex_get_mutex( &G_LOCK_NAME( modal_lock ) ) );

	G_UNLOCK( modal_lock );
}

/**
 * mail_operation_wait_for_finish:
 *
 * Waits for the currently executing async operations
 * to finish executing
 */

void mail_operation_wait_for_finish( void )
{
	while( mail_operation_in_progress ) {
		while( gtk_events_pending() )
			gtk_main_iteration();
	}
}

/**
 * mail_operations_are_executing:
 *
 * Returns TRUE if operations are being executed asynchronously
 * when called, FALSE if not.
 */

gboolean mail_operations_are_executing( void )
{
	return mail_operation_in_progress;
}

/* ** Static functions **************************************************** */

/**
 * create_queue_window:
 *
 * Creates the queue_window widget that displays the progress of the
 * current operation.
 */

static void
create_queue_window( void )
{
	GtkWidget *vbox;
	GtkWidget *pending_vb, *pending_lb;
	GtkWidget *progress_lb, *progress_bar;

	/* Check to see if we've only hidden it */
	if( queue_window != NULL )
		return;

	queue_window = gtk_window_new( GTK_WINDOW_DIALOG );
	gtk_container_set_border_width( GTK_CONTAINER( queue_window ), 8 );

	vbox = gtk_vbox_new( FALSE, 4 );

	pending_vb = gtk_vbox_new( FALSE, 2 );
	queue_window_pending = pending_vb;

	pending_lb = gtk_label_new( _("Currently pending operations:") );
	gtk_misc_set_alignment( GTK_MISC( pending_lb ), 0.0, 0.0 );
	gtk_box_pack_start( GTK_BOX( pending_vb ), pending_lb,
			    FALSE, TRUE, 0 );

	gtk_box_pack_start( GTK_BOX( vbox ), pending_vb,
			    TRUE, TRUE, 4 );

	/* FIXME: 'operation' is not the warmest cuddliest word. */
	progress_lb = gtk_label_new( "" );
	queue_window_message = progress_lb;
	gtk_box_pack_start( GTK_BOX( vbox ), progress_lb,
			    FALSE, TRUE, 4 );

	progress_bar = gtk_progress_bar_new();
	queue_window_progress = progress_bar;
	/* FIXME: is this fit for l10n? */
	gtk_progress_bar_set_orientation( GTK_PROGRESS_BAR( progress_bar ), 
					  GTK_PROGRESS_LEFT_TO_RIGHT );
	gtk_progress_bar_set_bar_style( GTK_PROGRESS_BAR( progress_bar ), 
					GTK_PROGRESS_CONTINUOUS );
	gtk_box_pack_start( GTK_BOX( vbox ), progress_bar,
			    FALSE, TRUE, 4 );

	gtk_container_add( GTK_CONTAINER( queue_window ), vbox );
}

/**
 * check_compipe:
 *
 * Check and see if our pipe has been opened and open
 * it if necessary.
 **/

static void check_compipe( void )
{
	if( READER > 0 )
		return;

	if( pipe( compipe ) < 0 ) { 
		g_warning( "Call to pipe(2) failed!" );
		
		/* FIXME: better error handling. How do we react? */
		return;
	}

	chan_reader = g_io_channel_unix_new( READER );
	g_io_add_watch( chan_reader, G_IO_IN, read_msg, NULL );
}

/**
 * check_cond:
 *
 * See if our condition is initialized and do so if necessary
 **/

static void check_cond( void )
{
	if( modal_cond == NULL )
		modal_cond = g_cond_new();
}

/**
 * dispatch:
 * @clur: The function to execute and its userdata
 *
 * Start a thread that executes the closure and exit
 * it when done.
 */

static void dispatch( closure_t *clur )
{
	int res;

	res = pthread_create( &dispatch_thread, NULL, (void *) &dispatch_func, clur );

	if( res != 0 ) {
		g_warning( "Error launching dispatch thread!" );
		/* FIXME: more error handling */
	}
}

/**
 * dispatch_func:
 * @data: the closure to run
 *
 * Runs the closure and exits the thread.
 */

static void *dispatch_func( void *data )
{
	com_msg_t msg;
	closure_t *clur = (closure_t *) data;

	msg.type = STARTING;
	msg.message = clur->prettyname;
	write( WRITER, &msg, sizeof( msg ) );

	/*GDK_THREADS_ENTER ();*/
	(clur->callback)( clur->data );
	/*GDK_THREADS_LEAVE ();*/

	msg.type = FINISHED;
	msg.func = clur->cleanup; /* NULL is ok */
	msg.userdata = clur->data;
	write( WRITER, &msg, sizeof( msg ) );

	g_free( clur->prettyname );
	g_free( data );

	pthread_exit( 0 );
	return NULL; /*NOTREACHED*/
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

static gboolean read_msg( GIOChannel *source, GIOCondition condition, gpointer userdata )
{
	com_msg_t *msg;
	closure_t *clur;
	GSList *temp;
	guint size;

	msg = g_new0( com_msg_t, 1 );

	g_io_channel_read( source, (gchar *) msg, 
			   sizeof( com_msg_t ) / sizeof( gchar ), 
			   &size );

	if( size != sizeof( com_msg_t ) ) {
		g_warning( _("Incomplete message written on pipe!") );
		msg->type = ERROR;
		msg->message = g_strdup( _("Error reading commands from dispatching thread.") );
	}

	/* This is very important, though I'm not quite sure why
	 * it is as we are in the main thread right now.
	 */

	GDK_THREADS_ENTER();

	switch( msg->type ) {
	case STARTING:
		DEBUG (("*** Message -- STARTING\n"));
		gtk_label_set_text( GTK_LABEL( queue_window_message ), msg->message );
		gtk_progress_bar_update( GTK_PROGRESS_BAR( queue_window_progress ), 0.0 );
		g_free( msg );
		break;
	case PERCENTAGE:
		DEBUG (("*** Message -- PERCENTAGE\n"));
		gtk_progress_bar_update( GTK_PROGRESS_BAR( queue_window_progress ), msg->percentage );
		g_free( msg );
		break;
	case HIDE_PBAR:
		DEBUG (("*** Message -- HIDE_PBAR\n"));
		gtk_progress_set_activity_mode( GTK_PROGRESS( queue_window_progress ), TRUE );
		timeout_toggle( TRUE );

		g_free( msg );
		break;
	case SHOW_PBAR:
		DEBUG (("*** Message -- SHOW_PBAR\n"));
		timeout_toggle( FALSE );
		gtk_progress_set_activity_mode( GTK_PROGRESS( queue_window_progress ), FALSE );

		g_free( msg );
		break;
	case MESSAGE:
		DEBUG (("*** Message -- MESSAGE\n"));
		gtk_label_set_text( GTK_LABEL( queue_window_message ),
				    msg->message );
		g_free( msg->message );
		g_free( msg );
		break;
	case PASSWORD:
		DEBUG (("*** Message -- PASSWORD\n"));
		g_assert( msg->reply );
		g_assert( msg->success );
		get_password( msg );
		/* don't free msg! done later */
		break;
	case ERROR:
		DEBUG (("*** Message -- ERROR\n"));
		show_error( msg );
		g_free( msg );
		break;

		/* Don't fall through; dispatch_func does the FINISHED
		 * call for us 
		 */

	case FINISHED:
		DEBUG (("*** Message -- FINISH\n"));
		if( msg->func )
			(msg->func)( msg->userdata );

		if( op_queue == NULL ) {
			g_print("\tNo more ops -- hide %p.\n", queue_window);
			/* All done! */
			gtk_widget_hide( queue_window );
			mail_operation_in_progress = FALSE;
		} else {
			g_print("\tOperation left.\n");

			/* There's another operation left */
			
			/* Pop it off the front */
			clur = op_queue->data;
			temp = g_slist_next( op_queue );
			g_slist_free_1( op_queue );
			op_queue = temp;

			/* Clear it out of the 'pending' vbox */
			remove_next_pending();

			/* Run run run little process */
			dispatch( clur );
		}
		g_free( msg );
		break;
	default:
		g_warning( _("Corrupted message from dispatching thread?") );
		break;
	}

	GDK_THREADS_LEAVE();
	return TRUE;
}

/**
 * remove_next_pending:
 *
 * Remove an item from the list of pending items. If
 * that's the last one, additionally hide the little
 * 'pending' message.
 **/

static void remove_next_pending( void )
{
	GList *children;

	children = gtk_container_children( GTK_CONTAINER( queue_window_pending ) );

	/* Skip past the header label */
	children = g_list_first( children );
	children = g_list_next( children );

	/* Nuke the one on top */
	gtk_container_remove( GTK_CONTAINER( queue_window_pending ),
			      GTK_WIDGET( children->data ) );

	/* Hide it? */
	if( g_list_next( children ) == NULL )
		gtk_widget_hide( queue_window_pending );
}

/**
 * show_error:
 *
 * Show the error dialog and wait for user OK
 **/

static void show_error( com_msg_t *msg )
{
	GtkWidget *err_dialog;

	err_dialog = gnome_error_dialog( msg->message );
	gnome_dialog_set_close( GNOME_DIALOG(err_dialog), TRUE );
	gtk_signal_connect( GTK_OBJECT( err_dialog ), "clicked", (GtkSignalFunc) show_error_clicked, NULL );
	g_free( msg->message );

	G_LOCK( modal_lock );

	timeout_toggle( FALSE );
	modal_may_proceed = FALSE;
	gtk_widget_show( GTK_WIDGET( err_dialog ) );
	gnome_win_hints_set_layer( err_dialog, 
				   WIN_LAYER_ONTOP );
	gnome_win_hints_set_state( err_dialog, 
				   WIN_STATE_ARRANGE_IGNORE );
	gnome_win_hints_set_hints( err_dialog, 
				   WIN_HINTS_SKIP_FOCUS |
				   WIN_HINTS_SKIP_WINLIST |
				   WIN_HINTS_SKIP_TASKBAR );
}

/**
 * show_error_clicked:
 *
 * Called when the user makes hits okay to the error dialog --
 * the dispatch thread is allowed to continue.
 **/

static void show_error_clicked( void )
{
	modal_may_proceed = TRUE;
	timeout_toggle( TRUE );
	g_cond_signal( modal_cond );
	G_UNLOCK( modal_lock );
}

/**
 * get_password:
 *
 * Ask for a password and put the answer in *(msg->reply)
 **/

static void get_password( com_msg_t *msg )
{
	GtkWidget *dialog;

	dialog = gnome_request_dialog( msg->secret, msg->message, NULL,
				       0, get_password_cb, msg,
				       NULL );
	gnome_dialog_set_close( GNOME_DIALOG(dialog), TRUE );
	gtk_signal_connect( GTK_OBJECT( dialog ), "clicked", get_password_clicked, msg );

	G_LOCK( modal_lock );

	modal_may_proceed = FALSE;

	if( dialog == NULL ) {
		*(msg->success) = FALSE;
		*(msg->reply) = g_strdup( _("Could not create dialog box.") );
		modal_may_proceed = TRUE;
		g_cond_signal( modal_cond );
		G_UNLOCK( modal_lock );
	} else {
		*(msg->reply) = NULL;
		timeout_toggle( FALSE );
		gtk_widget_show( GTK_WIDGET( dialog ) );
		gnome_win_hints_set_layer( dialog, 
					   WIN_LAYER_ONTOP );
		gnome_win_hints_set_state( dialog, 
					   WIN_STATE_ARRANGE_IGNORE );
		gnome_win_hints_set_hints( dialog, 
					   WIN_HINTS_SKIP_FOCUS |
					   WIN_HINTS_SKIP_WINLIST |
					   WIN_HINTS_SKIP_TASKBAR );
	}
}

static void get_password_cb( gchar *string, gpointer data )
{
	com_msg_t *msg = (com_msg_t *) data;

        if (string)
                *(msg->reply) = g_strdup( string );
        else
                *(msg->reply) = NULL;
}

static void get_password_clicked( GnomeDialog *dialog, gint button, gpointer user_data )
{
	com_msg_t *msg = (com_msg_t *) user_data;

	if( button == 1 || *(msg->reply) == NULL ) {
		*(msg->success) = FALSE;
		*(msg->reply) = g_strdup( _("User cancelled query.") );
	} else
		*(msg->success) = TRUE;

	g_free( msg );
	modal_may_proceed = TRUE;
	timeout_toggle( TRUE );
	g_cond_signal( modal_cond );
	G_UNLOCK( modal_lock );
}

/* NOT totally copied from gtk+/gtk/testgtk.c, really! */

static gboolean
progress_timeout (gpointer data)
{
	gfloat new_val;
	GtkAdjustment *adj;

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
timeout_toggle( gboolean active )
{
	if( (GTK_PROGRESS( queue_window_progress ))->activity_mode == 0 )
		return;

	if( active ) {
		if( progress_timeout_handle < 0 )
			progress_timeout_handle = gtk_timeout_add( 80, progress_timeout, queue_window_progress );
	} else {
		if( progress_timeout_handle >= 0 ) {
			gtk_timeout_remove( progress_timeout_handle );
			progress_timeout_handle = -1;
		}
	}
}

#endif
