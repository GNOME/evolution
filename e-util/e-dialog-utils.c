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

#include "e-dialog-utils.h"

#include <glib.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#include <gtk/gtkfilesel.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkplug.h>

#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <libgnomeui/gnome-dialog-util.h>
#include <libgnomeui/gnome-uidefs.h>


/* Tests whether or not an X Window is being managed by the
 * window manager.
 */
static gboolean
window_is_wm_toplevel (Display *display, Window window)
{
	static Atom WM_STATE = None;
	unsigned long nitems, after;
	unsigned char *data = NULL;
	Atom type = None;
	int format;

	if (!WM_STATE)
		WM_STATE = XInternAtom (display, "WM_STATE", False);

	if (XGetWindowProperty (display, window, WM_STATE, 0, 0, False,
				AnyPropertyType, &type, &format,
				&nitems, &after, &data) == Success) {
		if (data)
			XFree((char*)data);
		if (type)
			return TRUE;
	}
	return FALSE;
}

/**
 * e_dialog_set_transient_for:
 * @dialog: a dialog window
 * @parent_widget: the parent for @dialog
 * 
 * This sets the parent for @dialog to be @parent_widget. Unlike
 * gtk_window_set_transient_for(), this doesn't need @parent_widget to
 * be the actual toplevel, and also works if @parent_widget is
 * embedded as a Bonobo control by an out-of-process container.
 * @parent_widget must already be realized before calling this
 * function, but @dialog does not need to be.
 **/
void
e_dialog_set_transient_for (GtkWindow *dialog,
			    GtkWidget *parent_widget)
{
	GtkWidget *toplevel;
	Window parent, root_ret, *children;
	unsigned int numchildren;
	Display *display;
	Status status;

	g_return_if_fail (GTK_IS_WINDOW (dialog));
	g_return_if_fail (GTK_IS_WIDGET (parent_widget));

	toplevel = gtk_widget_get_toplevel (parent_widget);
	if (toplevel == NULL)
		return;

	if (!GTK_IS_PLUG (toplevel)) {
		gtk_window_set_transient_for (GTK_WINDOW (dialog),
					      GTK_WINDOW (toplevel));
		return;
	}

	/* Find the top-level windowmanager-managed X Window */
	display = GDK_WINDOW_XDISPLAY (parent_widget->window);
	parent = GDK_WINDOW_XID (parent_widget->window);

	while (parent && !window_is_wm_toplevel (display, parent)) {
		status = XQueryTree (display, parent, &root_ret,
				     &parent, &children, &numchildren);
		if (status != 0)
			XFree (children);
	}

	e_dialog_set_transient_for_xid (dialog, parent);
}

static void
dialog_realized (GtkWindow *dialog, gpointer xid)
{
	e_dialog_set_transient_for_xid (dialog, (GdkNativeWindow)xid);
}

/**
 * e_dialog_set_transient_for_xid:
 * @dialog: a dialog window
 * @xid: the X Window parent
 * 
 * Like e_dialog_set_transient_for(), but use an XID to specify the
 * parent window.
 **/
void
e_dialog_set_transient_for_xid (GtkWindow *dialog,
				GdkNativeWindow xid)
{
	GdkDisplay *display;
	GdkWindow *parent;

	g_return_if_fail (GTK_IS_WINDOW (dialog));

	if (!GTK_WIDGET_REALIZED (dialog)) {
		g_signal_connect (dialog, "realize",
				  G_CALLBACK (dialog_realized),
				  (gpointer) xid);
		return;
	}

	display = gdk_drawable_get_display (GDK_DRAWABLE (GTK_WIDGET (dialog)->window));
	parent = gdk_window_lookup_for_display (display, xid);
	if (!parent) {
		parent = gdk_window_foreign_new_for_display (display, xid);
		g_return_if_fail (parent != NULL);
	}

	gdk_window_set_transient_for (GTK_WIDGET (dialog)->window, parent);
}



static void
e_gnome_dialog_parent_destroyed (GnomeDialog *dialog, GObject *deadbeef)
{
	gnome_dialog_close (GNOME_DIALOG (dialog));
}

void
e_gnome_dialog_set_parent (GnomeDialog *dialog, GtkWindow *parent)
{
	gnome_dialog_set_parent (dialog, parent);
	g_object_weak_ref ((GObject *) parent, (GWeakNotify) e_gnome_dialog_parent_destroyed, dialog);
}

static void
save_ok (GtkWidget *widget, gpointer data)
{
	GtkWidget *fs;
	char **filename = data;
	const char *path;
	int btn = GNOME_YES;
	
	fs = gtk_widget_get_toplevel (widget);
	path = gtk_file_selection_get_filename (GTK_FILE_SELECTION (fs));
	
	if (g_file_test (path, G_FILE_TEST_IS_REGULAR)) {
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
	
	g_signal_connect (fs->ok_button, "clicked", G_CALLBACK (save_ok), &filename);
	g_signal_connect (fs->cancel_button, "clicked", G_CALLBACK (gtk_main_quit), NULL);
	
	gtk_widget_show (GTK_WIDGET (fs));
	gtk_grab_add (GTK_WIDGET (fs));
	gtk_main ();
	
	gtk_widget_destroy (GTK_WIDGET (fs));
	
	return filename;
}


