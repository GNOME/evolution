/* Tests the multithreaded UI code */

#include "config.h"
#include <unistd.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <libgnomeui/libgnomeui.h>
#include <stdio.h>
#include "mail-threads.h"

static gchar *desc_1 (gpointer in, gboolean gerund);
static void op_1( gpointer in, gpointer op, CamelException *ex );
static gchar *desc_2 (gpointer in, gboolean gerund);
static void op_2( gpointer in, gpointer op, CamelException *ex );
static gchar *desc_3 (gpointer in, gboolean gerund);
static void op_3( gpointer in, gpointer op, CamelException *ex );
static gchar *desc_4 (gpointer in, gboolean gerund);
static void op_4( gpointer in, gpointer op, CamelException *ex );
static gchar *desc_5 (gpointer in, gboolean gerund);
static void op_5( gpointer in, gpointer op, CamelException *ex );
static gchar *desc_6 (gpointer in, gboolean gerund);
static gchar *desc_7 (gpointer in, gboolean gerund);
static gchar *desc_8 (gpointer in, gboolean gerund);
static void done( gpointer in, gpointer op, CamelException *ex );
static void exception( gpointer in, gpointer op, CamelException *ex );
static gboolean queue_ops( void );

const mail_operation_spec spec1 = { desc_1, 0, NULL, op_1, done };
const mail_operation_spec spec2 = { desc_2, 0, NULL, op_2, done };
const mail_operation_spec spec3 = { desc_3, 0, NULL, op_3, done };
const mail_operation_spec spec4 = { desc_4, 0, NULL, op_4, NULL };
const mail_operation_spec spec5 = { desc_5, 0, NULL, op_5, done };
const mail_operation_spec spec6 = { desc_6, 0, exception, op_4, NULL };
const mail_operation_spec spec7 = { desc_7, 0, NULL, exception, NULL };
const mail_operation_spec spec8 = { desc_8, 0, NULL, op_4, exception };

static gboolean queue_ops( void )
{
	int i;

	g_message( "Top of queue_ops" );

	mail_operation_queue( &spec1, "op1 finished", FALSE );
	mail_operation_queue( &spec2, "op2 finished", FALSE );
	mail_operation_queue( &spec3, "op3 finished", FALSE );

	for( i = 0; i < 3; i++ ) {
		mail_operation_queue( &spec4, GINT_TO_POINTER( i ), FALSE );
	}

	g_message( "Waiting for finish..." );
	mail_operation_wait_for_finish();

	g_message( "Ops done -- queue some more!" );

	mail_operation_queue( &spec1, "done a second time", FALSE );

	g_message( "Waiting for finish again..." );
	mail_operation_wait_for_finish();

	g_message( "Ops done -- more, more!" );

	mail_operation_queue( &spec5, "passwords stolen", FALSE );

	for( i = 0; i < 3; i++ ) {
		mail_operation_queue( &spec4, GINT_TO_POINTER( i ), FALSE );
	}

	mail_operation_queue( &spec6, NULL, FALSE );
	mail_operation_queue( &spec7, NULL, FALSE );
	mail_operation_queue( &spec8, NULL, FALSE );

	g_message( "Waiting for finish for the last time..." );
	mail_operations_terminate();
	g_message( "Ops done again. Exiting 0" );
	gtk_exit( 0 );
	return FALSE;
}

static void exception( gpointer in, gpointer op, CamelException *ex )
{
	camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM, "I don't feel like it.");
}

static void op_1( gpointer in, gpointer op, CamelException *ex )
{
	gfloat pct;

	mail_op_show_progressbar();
	mail_op_set_message( "Watch the progress bar!" );

	for( pct = 0.0; pct < 1.0; pct += 0.2 ) {
		sleep( 1 );
		mail_op_set_percentage( pct );
	}
}

static void op_2( gpointer in, gpointer op, CamelException *ex )
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

static void op_3( gpointer in, gpointer op, CamelException *ex )
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

static void op_4( gpointer in, gpointer op, CamelException *ex )
{
	mail_op_hide_progressbar();
	mail_op_set_message( "Filler # %d", GPOINTER_TO_INT( in ) );
	sleep( 1 );
}

static void op_5( gpointer in, gpointer op, CamelException *ex )
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

static void done( gpointer in, gpointer op, CamelException *ex )
{
	g_message( "Operation done: %s", (gchar *) in );
}

static gchar *desc_1 (gpointer in, gboolean gerund)
{
	if (gerund)
		return g_strdup ("Showing the Crawling Progress Bar of Doom");
	else
		return g_strdup ("Progress Bar");
}

static gchar *desc_2 (gpointer in, gboolean gerund)
{
	if (gerund)
		return g_strdup ("Exploring the Mysterious Message Setter");
	else
		return g_strdup ("Explore");
}

static gchar *desc_3 (gpointer in, gboolean gerund)
{
	if (gerund)
		return g_strdup ("Dare the Error Dialog of No Return");
	else
		return g_strdup ("Dare");
}

static gchar *desc_4 (gpointer in, gboolean gerund)
{
	if (gerund)
		return g_strdup_printf ("Filling Queue Space -- %d", GPOINTER_TO_INT (in));
	else
		return g_strdup_printf ("Filler -- %d", GPOINTER_TO_INT (in));
}

static gchar *desc_5 (gpointer in, gboolean gerund)
{
	if (gerund)
		return g_strdup ("Stealing your Password");
	else
		return g_strdup ("The Dastardly Password Stealer");
}

static gchar *desc_6 (gpointer in, gboolean gerund)
{
	if (gerund)
		return g_strdup ("Setting exception on setup");
	else
		return g_strdup ("Exception on setup");
}

static gchar *desc_7 (gpointer in, gboolean gerund)
{
	if (gerund)
		return g_strdup ("Setting exception in process");
	else
		return g_strdup ("Exception coming soon");
}

static gchar *desc_8 (gpointer in, gboolean gerund)
{
	if (gerund)
		return g_strdup ("Setting exception in cleanup");
	else
		return g_strdup ("Exception in cleanup");
}


int main( int argc, char **argv )
{
	g_thread_init( NULL );
	gnome_init( "test-thread", "0.0", argc, argv );
	gtk_idle_add( (GtkFunction) queue_ops, NULL );
	gtk_main();
	return 0;
}
