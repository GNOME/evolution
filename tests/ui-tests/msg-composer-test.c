/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

#include <gnome.h>

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

	gtk_object_unref (GTK_OBJECT (message));

#if 0
	gtk_widget_destroy (GTK_WIDGET (composer));
	gtk_main_quit ();
#endif
}

int
main (int argc, char **argv)
{
	GtkWidget *composer;

	gnome_init ("test", "0.0", argc, argv);
	glade_gnome_init ();

	composer = e_msg_composer_new ();
	gtk_widget_show (composer);

	gtk_signal_connect (GTK_OBJECT (composer), "send",
			    GTK_SIGNAL_FUNC (send_cb), NULL);

	gtk_main ();

	return 0;
}
