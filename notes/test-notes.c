/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

#include <config.h>
#include <gnome.h>
#include <gdk/gdkx.h>

#include "e-util/e-canvas.h"
#include "widgets/e-text/e-text.h"

#include "e-note.h"

void
text_changed (GtkWidget *widget, gpointer data)
{
	g_print ("Text changed!\n");
}

gint
main (gint argc, gchar **argv)
{
	GtkWidget *note;
	
	gnome_init ("NotesTest", "0.0.1", argc, argv);

	note = e_note_new ();
	e_note_set_text (E_NOTE (note), "This is a text note widget");
	gtk_signal_connect (GTK_OBJECT (note), "changed",
			    GTK_SIGNAL_FUNC (text_changed), NULL);
	
	gtk_widget_show (note);
	gtk_main ();
	
	return 0;
}
