/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

#include <gnome.h>
#include <libgnorba/gnorba.h>
#include <bonobo.h>

#include <glade/glade.h>

#include <camel/camel-data-wrapper.h>
#include <camel/camel-stream-fs.h>
#include <camel/camel-stream.h>

#include "e-msg-composer.h"

static void
send_cb (EMsgComposer *composer,
	 gpointer data)
{
	CamelMimeMessage *message;
	CamelStream *stream;
	gint stdout_dup;

	message = e_msg_composer_get_message (composer);

	stdout_dup = dup (1);
	stream = camel_stream_fs_new_with_fd (stdout_dup);
	camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (message),
					    stream);
	camel_stream_close (stream);

	camel_object_unref (CAMEL_OBJECT (message));

#if 0
	gtk_widget_destroy (GTK_WIDGET (composer));
	gtk_main_quit ();
#endif
}

static guint
create_composer (void)
{
	GtkWidget *composer;

	composer = e_msg_composer_new ();
	gtk_widget_show (composer);

	gtk_signal_connect (GTK_OBJECT (composer), "send", GTK_SIGNAL_FUNC (send_cb), NULL);

	return FALSE;
}

int
main (int argc, char **argv)
{
	CORBA_Environment ev;
	CORBA_ORB orb;

	CORBA_exception_init (&ev);
	gnome_CORBA_init ("evolution-test-msg-composer", "0.0", &argc, argv, 0, &ev);
	CORBA_exception_free (&ev);

	orb = gnome_CORBA_ORB ();

	glade_gnome_init ();

	if (bonobo_init (orb, NULL, NULL) == FALSE)
		g_error ("Could not initialize Bonobo\n");

	/* We can't make any CORBA calls unless we're in the main loop.  So we
	   delay creating the container here. */
	gtk_idle_add ((GtkFunction) create_composer, NULL);

	bonobo_main ();

	return 0;
}
