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
#include "mail.h"
#include "mail-threads.h"

/* FIXME: support other thread types!!!! */
#define USE_PTHREADS

/**
 * A function and its userdata
 **/

typedef struct closure_s {
	void (*func)( gpointer );
	gpointer data;
} closure_t;

/**
 * A command issued through the compipe
 **/

typedef struct com_msg_s {
	enum com_msg_type_e { STARTING, PERCENTAGE, HIDE_PBAR, SHOW_PBAR, MESSAGE, ERROR, FINISHED } type;
	gfloat percentage;
	gchar *message;
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
 * @op_queue: The list of operations the are scheduled
 * to proceed after the currently executing one.
 **/

static GSList *op_queue = NULL;

/**
 * @compipe: The pipe through which the dispatcher communicates
 * with the main thread for GTK+ calls
 *
 * @chan_reader: the GIOChannel that reads our pipe
 */

#define READER compipe[0]
#define WRITER compipe[1]

static int compipe[2] = { -1, -1 };

GIOChannel *chan_reader = NULL;

/**
 * Static prototypes
 **/

static void create_queue_window( void );
static void dispatch( closure_t *clur );
static void *dispatch_func( void *data );
static void check_compipe( void );
static gboolean read_msg( GIOChannel *source, GIOCondition condition, gpointer userdata );
static void remove_next_pending( void );

/* Pthread code */
#ifdef USE_PTHREADS

#include <pthread.h>

/**
 * @dispatch_thread: the pthread_t (when using pthreads, of
 * course) representing our dispatcher routine.
 **/
static pthread_t dispatch_thread;

/* FIXME: do we need to set any attributes for our thread? */

#else /* defined USE_PTHREADS */
choke on this: no thread type defined
#endif

/**
 * mail_operation_try:
 * @description: A user-friendly string describing the operation.
 * @callback: the function to call in another thread to start the operation
 * @user_data: extra data passed to the callback
 *
 * Waits for the currently executing operation to finished, then
 * executes the callback function in another thread. Returns TRUE
 * on success, FALSE on some sort of queueing error.
 **/

gboolean
mail_operation_try( const gchar *description, void (*callback)( gpointer ), gpointer user_data )
{
	closure_t *clur;
	g_assert( callback );

	clur = g_new( closure_t, 1 );
	clur->func = callback;
	clur->data = user_data;

	if( mail_operation_in_progress == FALSE ) {
		/* We got the lock. Yippeee! This means that no operations
		 * are pending, either, so we'll create the queue window and
		 * show only the message and progress bar.
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
		 * queue ourselves up. */
		
		/* Yes, prepend is faster. But we pop operations
		 * off the beginning later and that's a lot easier.
		 */

		op_queue = g_slist_append( op_queue, clur );

		/* Show us in the pending window. */
		label = gtk_label_new( description );
		gtk_misc_set_alignment( GTK_MISC( label ), 1.0, 0.5 );
		gtk_box_pack_start( GTK_BOX( queue_window_pending ), label,
				    TRUE, TRUE, 2 );

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

	va_start( val, fmt );
	msg.type = ERROR;
	msg.message = g_strdup_vprintf( fmt, val );
	va_end( val );

	write( WRITER, &msg, sizeof( msg ) );
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

	pending_vb = gtk_vbox_new( TRUE, 2 );
	queue_window_pending = pending_vb;

	pending_lb = gtk_label_new( _("Currently pending operations:") );
	gtk_misc_set_alignment( GTK_MISC( pending_lb ), 0.0, 0.0 );
	gtk_box_pack_start( GTK_BOX( pending_vb ), pending_lb,
			    TRUE, TRUE, 0 );

	gtk_box_pack_start( GTK_BOX( vbox ), pending_vb,
			    TRUE, TRUE, 4 );

	/* FIXME: 'operation' is not the warmest cuddliest word. */
	progress_lb = gtk_label_new( _("Starting operation...") );
	queue_window_message = progress_lb;
	gtk_box_pack_start( GTK_BOX( vbox ), progress_lb,
			    TRUE, TRUE, 4 );

	progress_bar = gtk_progress_bar_new();
	queue_window_progress = progress_bar;
	/* FIXME: is this fit for l10n? */
	gtk_progress_bar_set_orientation( GTK_PROGRESS_BAR( progress_bar ), 
					  GTK_PROGRESS_LEFT_TO_RIGHT );
	gtk_progress_bar_set_bar_style( GTK_PROGRESS_BAR( progress_bar ), 
					GTK_PROGRESS_CONTINUOUS );
	gtk_box_pack_start( GTK_BOX( vbox ), progress_bar,
			    TRUE, TRUE, 4 );

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
	write( WRITER, &msg, sizeof( msg ) );

	(clur->func)( clur->data );

	msg.type = FINISHED;
	write( WRITER, &msg, sizeof( msg ) );

	g_free( data );

	pthread_exit( 0 );
	return NULL; /*NOTREACHED*/
}

/**
 * read_msg:
 * @userdata: unused
 *
 * A message has been recieved on our pipe; perform the appropriate 
 * action.
 **/

static gboolean read_msg( GIOChannel *source, GIOCondition condition, gpointer userdata )
{
	com_msg_t msg;
	closure_t *clur;
	GSList *temp;
	guint size;
	#if 0
	GtkWidget *err_dialog;
	#else
	gchar *errmsg;
	#endif

	g_io_channel_read( source, (gchar *) &msg, 
			   sizeof( msg ) / sizeof( gchar ), 
			   &size );

	if( size != sizeof( msg ) ) {
		g_warning( _("Incomplete message written on pipe!") );
		msg.type = ERROR;
		msg.message = g_strdup( _("Error reading commands from dispatching thread.") );
	}

	switch( msg.type ) {
	case STARTING:
		gtk_label_set_text( GTK_LABEL( queue_window_message ),
				    _("Starting operation...") );
		gtk_progress_bar_update( GTK_PROGRESS_BAR( queue_window_progress ), 0.0 );
		break;
	case PERCENTAGE:
		gtk_progress_bar_update( GTK_PROGRESS_BAR( queue_window_progress ), msg.percentage );
		break;
	case HIDE_PBAR:
		gtk_widget_hide( GTK_WIDGET( queue_window_progress ) );
		break;
	case SHOW_PBAR:
		gtk_widget_show( GTK_WIDGET( queue_window_progress ) );
		break;
	case MESSAGE:
		gtk_label_set_text( GTK_LABEL( queue_window_message ),
				    msg.message );
		g_free( msg.message );
		break;
	case ERROR:
		#if 0
		/* FIXME FIXME: the gnome_dialog_ functions are causing coredumps
		 * on my machine! Every time, threads or not... "IMLIB ERROR: Cannot
		 * allocate XImage buffer". Until then, work around
		 */
		err_dialog = gnome_error_dialog( msg.message );
		gnome_dialog_run_and_close( GNOME_DIALOG( err_dialog ) );
		#else
		errmsg = g_strdup_printf( "ERROR: %s", msg.message );
		gtk_label_set_text( GTK_LABEL( queue_window_message ),
				    errmsg );
		g_free( errmsg );
		#endif
		g_free( msg.message );
		break;
		/* Don't fall through; dispatch_func does the FINISHED
		 * call for us 
		 */
	case FINISHED:
		if( op_queue == NULL ) {
			/* All done! */
			gtk_widget_hide( queue_window );
			mail_operation_in_progress = FALSE;
		} else {
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
		break;
	default:
		g_warning( _("Corrupted message from dispatching thread?") );
		break;
	}

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

	/* FIXME: The window gets really messed up here */
	gtk_container_resize_children( GTK_CONTAINER( queue_window_pending ) );
	gtk_container_resize_children( GTK_CONTAINER( queue_window ) );
}
