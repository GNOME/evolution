/* Tests the multithreaded UI code */

#include "config.h"
#include <unistd.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include "mail-threads.h"

static void op_1( gpointer userdata );
static void op_2( gpointer userdata );
static void op_3( gpointer userdata );
static void op_4( gpointer userdata );
static gboolean queue_ops( void );

static gboolean queue_ops( void )
{
	int i;
	gchar buf[32];

	g_message( "Top of queue_ops" );

	mail_operation_try( "The Crawling Progress Bar of Doom", op_1, NULL );
	mail_operation_try( "The Mysterious Message Setter", op_2, NULL );
	mail_operation_try( "The Error Dialog of No Return", op_3, NULL );

	for( i = 0; i < 7; i++ ) {
		sprintf( buf, "Queue Filler %d", i );
		mail_operation_try( buf, op_4, GINT_TO_POINTER( i ) );
	}

	g_message( "Waiting for finish..." );
	mail_operation_wait_for_finish();

	g_message( "Ops done -- queue some more!" );

	mail_operation_try( "Progress Bar Redux", op_1, NULL );

	g_message( "Waiting for finish again..." );
	mail_operation_wait_for_finish();

	g_message( "Ops done -- more, more!" );

	for( i = 0; i < 3; i++ ) {
		sprintf( buf, "Queue Filler %d", i );
		mail_operation_try( buf, op_4, GINT_TO_POINTER( i ) );
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

	for( pct = 0.0; pct < 1.0; pct += 0.1 ) {
		sleep( 1 );
		mail_op_set_percentage( pct );
	}
}

static void op_2( gpointer userdata )
{
	int i;

	mail_op_hide_progressbar();
	for( i = 10; i > 0; i-- ) {
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

int main( int argc, char **argv )
{
	g_thread_init( NULL );
	gtk_init( &argc, &argv );
	gtk_idle_add( (GtkFunction) queue_ops, NULL );
	gtk_main();
	return 0;
}
