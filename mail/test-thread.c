/* Tests the multithreaded UI code */

#include "config.h"
#include <unistd.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <libgnomeui/libgnomeui.h>
#include <stdio.h>
#include "mail-threads.h"

#ifdef ENABLE_BROKEN_THREADS

static void op_1( gpointer userdata );
static void op_2( gpointer userdata );
static void op_3( gpointer userdata );
static void op_4( gpointer userdata );
static void op_5( gpointer userdata );
static void done( gpointer userdata );
static gboolean queue_ops( void );

static gboolean queue_ops( void )
{
	int i;
	gchar buf[32];

	g_message( "Top of queue_ops" );

	mail_operation_try( "The Crawling Progress Bar of Doom", op_1, done, "op1 finished" );
	mail_operation_try( "The Mysterious Message Setter", op_2, done, "op2 finished" );
	mail_operation_try( "The Error Dialog of No Return", op_3, done, "op3 finished" );

	for( i = 0; i < 3; i++ ) {
		sprintf( buf, "Queue Filler %d", i );
		mail_operation_try( buf, op_4, NULL, GINT_TO_POINTER( i ) );
	}

	g_message( "Waiting for finish..." );
	mail_operation_wait_for_finish();

	g_message( "Ops done -- queue some more!" );

	mail_operation_try( "Progress Bar Redux", op_1, NULL, NULL );

	g_message( "Waiting for finish again..." );
	mail_operation_wait_for_finish();

	g_message( "Ops done -- more, more!" );

	mail_operation_try( "Dastardly Password Stealer", op_5, NULL, NULL );

	for( i = 0; i < 3; i++ ) {
		sprintf( buf, "Queue Filler %d", i );
		mail_operation_try( buf, op_4, NULL, GINT_TO_POINTER( i ) );
	}

	g_message( "Waiting for finish AGAIN..." );
	mail_operation_wait_for_finish();
	g_message( "Ops done again. Exiting 0" );
	gtk_exit( 0 );
	return FALSE;
}

static void op_1( gpointer userdata )
{
	gfloat pct;

	mail_op_show_progressbar();
	mail_op_set_message( "Watch the progress bar!" );

	for( pct = 0.0; pct < 1.0; pct += 0.2 ) {
		sleep( 1 );
		mail_op_set_percentage( pct );
	}
}

static void op_2( gpointer userdata )
{
	int i;

	mail_op_hide_progressbar();
	for( i = 5; i > 0; i-- ) {
		mail_op_set_message( "%d", i );
		sleep( 1 );
	}

	mail_op_set_message( "BOOOM!" );
	sleep( 1 );
}

static void op_3( gpointer userdata )
{
	gfloat pct;

	mail_op_show_progressbar();
	mail_op_set_message( "Frobulating the foosamatic" );

	for( pct = 0.0; pct < 0.3; pct += 0.1 ) {
		mail_op_set_percentage( pct );
		sleep( 1 );
	}

	mail_op_error( "Oh no! The foosamatic was booby-trapped!" );
	sleep( 1 );
}

static void op_4( gpointer userdata )
{
	mail_op_hide_progressbar();
	mail_op_set_message( "Filler # %d", GPOINTER_TO_INT( userdata ) );
	sleep( 1 );
}

static void op_5( gpointer userdata )
{
	gchar *pass;
	gboolean ret;

	mail_op_show_progressbar();
	mail_op_set_percentage( 0.5 );

	ret = mail_op_get_password( "What is your super-secret password?", TRUE, &pass );

	if( ret == FALSE )
		mail_op_set_message( "Oh no, you cancelled! : %s", pass );
	else
		mail_op_set_message( "\"%s\", you said?", pass );

	sleep( 1 );
}

static void done( gpointer userdata )
{
	g_message( "Operation done: %s", (gchar *) userdata );
}

int main( int argc, char **argv )
{
	g_thread_init( NULL );
	gnome_init( "test-thread", "0.0", argc, argv );
	gtk_idle_add( (GtkFunction) queue_ops, NULL );
	gtk_main();
	return 0;
}

#else

int main( int argc, char **argv )
{
	g_message( "Threads aren't enabled, so they cannot be tested." );
	return 0;
}

#endif
