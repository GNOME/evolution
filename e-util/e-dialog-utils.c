/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-dialog-utils.h
 *
 * Copyright (C) 2001  Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors:
 *   Michael Meeks <michael@ximian.com>
 *   Ettore Perazzoli <ettore@ximian.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-dialog-utils.h"

#include "widgets/misc/e-bonobo-widget.h"

#include <gdk/gdkx.h>
#include <gdk/gdkprivate.h>
#include <gdk/gdk.h>

#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkfilesel.h>

#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <libgnomeui/gnome-dialog-util.h>
#include <libgnomeui/gnome-uidefs.h>

#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-property-bag.h>


#define TRANSIENT_DATA_ID "e-dialog:transient"


static void
transient_realize_callback (GtkWidget *widget)
{
	GdkWindow *window;

	window = gtk_object_get_data (GTK_OBJECT (widget), TRANSIENT_DATA_ID);
	g_assert (window != NULL);

	gdk_window_set_transient_for (GTK_WIDGET (widget)->window, window);
}

static void
transient_unrealize_callback (GtkWidget *widget)
{
	GdkWindow *window;

	window = gtk_object_get_data (GTK_OBJECT (widget), TRANSIENT_DATA_ID);
	g_assert (window != NULL);

	gdk_property_delete (window, gdk_atom_intern ("WM_TRANSIENT_FOR", FALSE));
}

static void
transient_destroy_callback (GtkWidget *widget)
{
	GdkWindow *window;
	
	window = gtk_object_get_data (GTK_OBJECT (widget), "transient");
	if (window != NULL)
		gdk_window_unref (window);
}

static void       
set_transient_for_gdk (GtkWindow *window, 
		       GdkWindow *parent)
{
	g_return_if_fail (window != NULL);
	g_return_if_fail (gtk_object_get_data (GTK_OBJECT (window), TRANSIENT_DATA_ID) == NULL);

	/* if the parent window doesn't exist anymore,
	 * something is probably about to go very wrong,
	 * but at least let's not segfault here. */

	if (parent == NULL) {
		g_warning ("set_transient_for_gdk: uhoh, parent of window %p is NULL", window);
		return;
	}

	gdk_window_ref (parent); /* FIXME? */

	gtk_object_set_data (GTK_OBJECT (window), TRANSIENT_DATA_ID, parent);

	if (GTK_WIDGET_REALIZED (window))
		gdk_window_set_transient_for (GTK_WIDGET (window)->window, parent);

	gtk_signal_connect (GTK_OBJECT (window), "realize",
			    GTK_SIGNAL_FUNC (transient_realize_callback), NULL);

	gtk_signal_connect (GTK_OBJECT (window), "unrealize",
			    GTK_SIGNAL_FUNC (transient_unrealize_callback), NULL);
	
	gtk_signal_connect (GTK_OBJECT (window), "destroy",
			    GTK_SIGNAL_FUNC (transient_destroy_callback), NULL);
}


/**
 * e_set_dialog_parent:
 * @dialog: 
 * @parent_widget: 
 * 
 * This sets the parent for @dialog to be @parent_widget.  Unlike
 * gtk_window_set_parent(), this doesn't need @parent_widget to be the actual
 * toplevel, and also works if @parent_widget is been embedded as a Bonobo
 * control by an out-of-process container.
 **/
void
e_set_dialog_parent (GtkWindow *dialog,
		     GtkWidget *parent_widget)
{
	Bonobo_PropertyBag property_bag;
	GtkWidget *toplevel;
	GdkWindow *gdk_window;
	CORBA_char *id;
	guint32 xid;

	g_return_if_fail (dialog != NULL);
	g_return_if_fail (GTK_IS_WINDOW (dialog));
	g_return_if_fail (parent_widget != NULL);
	g_return_if_fail (GTK_IS_WIDGET (parent_widget));

	toplevel = gtk_widget_get_toplevel (parent_widget);
	if (toplevel == NULL)
		return;

	if (! BONOBO_IS_CONTROL (toplevel)) {
		if (GTK_IS_WINDOW (toplevel))
			gtk_window_set_transient_for (dialog, GTK_WINDOW (toplevel));
		return;
	}

	property_bag = bonobo_control_get_ambient_properties (BONOBO_CONTROL (toplevel), NULL);
	if (property_bag == CORBA_OBJECT_NIL)
		return;

	id = bonobo_property_bag_client_get_value_string (property_bag, E_BONOBO_WIDGET_TOPLEVEL_PROPERTY_ID, NULL);
	if (id == NULL)
		return;

	xid = strtol (id, NULL, 10);

	gdk_window = gdk_window_foreign_new (xid);
	set_transient_for_gdk (dialog, gdk_window);
}

/**
 * e_set_dialog_parent_from_xid:
 * @dialog: 
 * @xid: 
 * 
 * Like %e_set_dialog_parent_from_xid, but use an XID to specify the parent
 * window.
 **/
void
e_set_dialog_parent_from_xid (GtkWindow *dialog,
			      Window xid)
{
	g_return_if_fail (dialog != NULL);
	g_return_if_fail (GTK_IS_WINDOW (dialog));

	set_transient_for_gdk (dialog, gdk_window_foreign_new (xid));
}

static void
e_gnome_dialog_parent_destroyed (GtkWidget *parent, GtkWidget *dialog)
{
	gnome_dialog_close (GNOME_DIALOG (dialog));
}

void
e_gnome_dialog_set_parent (GnomeDialog *dialog, GtkWindow *parent)
{
	gnome_dialog_set_parent (dialog, parent);
	gtk_signal_connect_while_alive (GTK_OBJECT (parent), "destroy",
					e_gnome_dialog_parent_destroyed,
					dialog, GTK_OBJECT (dialog));
}

GtkWidget *
e_gnome_warning_dialog_parented (const char *warning, GtkWindow *parent)
{
	GtkWidget *dialog;
	
	dialog = gnome_warning_dialog_parented (warning, parent);
	gtk_signal_connect_while_alive (GTK_OBJECT (parent), "destroy",
					e_gnome_dialog_parent_destroyed, dialog, GTK_OBJECT(dialog));
	
	return dialog;
}

GtkWidget *
e_gnome_ok_cancel_dialog_parented (const char *message, GnomeReplyCallback callback,
				   gpointer data, GtkWindow *parent)
{
	GtkWidget *dialog;
	
	dialog = gnome_ok_cancel_dialog_parented (message, callback, data, parent);
	gtk_signal_connect_while_alive (GTK_OBJECT (parent), "destroy",
					e_gnome_dialog_parent_destroyed, dialog, GTK_OBJECT(dialog));
	
	return dialog;
}

static void
save_ok (GtkWidget *widget, gpointer data)
{
	GtkWidget *fs;
	char **filename = data;
	char *path;
	int btn = GNOME_YES;

	fs = gtk_widget_get_toplevel (widget);
	path = gtk_file_selection_get_filename (GTK_FILE_SELECTION (fs));

	if (g_file_test (path, G_FILE_TEST_ISFILE)) {
		GtkWidget *dlg;

		dlg = gnome_question_dialog_modal (_("A file by that name already exists.\n"
						     "Overwrite it?"), NULL, NULL);
		btn = gnome_dialog_run_and_close (GNOME_DIALOG (dlg));
	}

	if (btn == GNOME_YES)
		*filename = g_strdup (path);

	gtk_main_quit ();
}

char *
e_file_dialog_save (const char *title)
{
	GtkFileSelection *fs;
	char *path, *filename = NULL;

	fs = GTK_FILE_SELECTION (gtk_file_selection_new (title));
	path = g_strdup_printf ("%s/", g_get_home_dir ());
	gtk_file_selection_set_filename (fs, path);
	g_free (path);
	
	gtk_signal_connect (GTK_OBJECT (fs->ok_button), "clicked",
			    GTK_SIGNAL_FUNC (save_ok), &filename);
	gtk_signal_connect (GTK_OBJECT (fs->cancel_button), "clicked",
			    GTK_SIGNAL_FUNC (gtk_main_quit), NULL);
	
	gtk_widget_show (GTK_WIDGET (fs));
	gtk_grab_add (GTK_WIDGET (fs));
	gtk_main ();
	
	gtk_widget_destroy (GTK_WIDGET (fs));

	return filename;
}


