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

#include <gdk/gdkx.h>

#include <gtk/gtkmain.h>
#include <gtk/gtkplug.h>
#include <gtk/gtkversion.h>

#ifdef USE_GTKFILECHOOSER
#include <gtk/gtkfilechooser.h>
#include <gtk/gtkfilechooserdialog.h>
#include <gtk/gtkstock.h>
#else
#include <gtk/gtkfilesel.h>
#endif

#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>


/**
 * e_notice:
 * @parent: the dialog's parent window, or %NULL
 * @type: the type of dialog (%GTK_MESSAGE_INFO, %GTK_MESSAGE_WARNING,
 * or %GTK_MESSAGE_ERROR)
 * @format: printf-style format string, followed by arguments
 *
 * Convenience function to show a dialog with a message and an "OK"
 * button.
 **/
void
e_notice (gpointer parent, GtkMessageType type, const char *format, ...)
{
	GtkWidget *dialog;
	va_list args;
	char *str;

	va_start (args, format);
	str = g_strdup_vprintf (format, args);
	dialog = gtk_message_dialog_new (NULL,
					 GTK_DIALOG_DESTROY_WITH_PARENT,
					 type,
					 GTK_BUTTONS_OK,
					 "%s",
					 str);
#if !GTK_CHECK_VERSION (2,4,0)
	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
#endif
	va_end (args);
	g_free (str);
	
	if (parent)
		e_dialog_set_transient_for (GTK_WINDOW (dialog), parent);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

/**
 * e_notice_with_xid:
 * @parent: the dialog's parent window, or %NULL
 * @type: the type of dialog (%GTK_MESSAGE_INFO, %GTK_MESSAGE_WARNING,
 * or %GTK_MESSAGE_ERROR)
 * @format: printf-style format string, followed by arguments
 *
 * Like e_notice(), but takes a GdkNativeWindow for the parent
 * window argument.
 **/
void
e_notice_with_xid (GdkNativeWindow parent, GtkMessageType type, const char *format, ...)
{
	GtkWidget *dialog;
	va_list args;
	char *str;

	va_start (args, format);
	str = g_strdup_vprintf (format, args);
	dialog = gtk_message_dialog_new (NULL,
					 GTK_DIALOG_DESTROY_WITH_PARENT,
					 type,
					 GTK_BUTTONS_OK,
					 "%s",
					 str);
	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
	va_end (args);
	g_free (str);
	
	if (parent)
		e_dialog_set_transient_for_xid (GTK_WINDOW (dialog), parent);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}


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
#ifdef GDK_MULTIHEAD_SAFE
	GdkDisplay *display;
#endif
	GdkWindow *parent;

	g_return_if_fail (GTK_IS_WINDOW (dialog));

	if (!GTK_WIDGET_REALIZED (dialog)) {
		g_signal_connect (dialog, "realize",
				  G_CALLBACK (dialog_realized),
				  (gpointer) xid);
		return;
	}

#ifdef GDK_MULTIHEAD_SAFE
	display = gdk_drawable_get_display (GDK_DRAWABLE (GTK_WIDGET (dialog)->window));
	parent = gdk_window_lookup_for_display (display, xid);
	if (!parent)
		parent = gdk_window_foreign_new_for_display (display, xid);
#else
	parent = gdk_window_lookup (xid);
	if (!parent)
		parent = gdk_window_foreign_new (xid);
#endif
	g_return_if_fail (parent != NULL);

	gdk_window_set_transient_for (GTK_WIDGET (dialog)->window, parent);
}



static void
save_ok (GtkWidget *widget, gpointer data)
{
	GtkWidget *fs;
	char **filename = data;
	const char *path;
	int btn = GTK_RESPONSE_YES;
	
	fs = gtk_widget_get_toplevel (widget);
#ifdef USE_GTKFILECHOOSER
	path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (fs));
#else
	path = gtk_file_selection_get_filename (GTK_FILE_SELECTION (fs));
#endif
	
	if (g_file_test (path, G_FILE_TEST_IS_REGULAR)) {
		GtkWidget *dlg;
		
		dlg = gtk_message_dialog_new (GTK_WINDOW (fs), 0,
					      GTK_MESSAGE_QUESTION,
					      GTK_BUTTONS_YES_NO,
					      _("A file by that name already exists.\n"
						"Overwrite it?"));
		gtk_window_set_title (GTK_WINDOW (dlg), _("Overwrite file?"));
		gtk_dialog_set_has_separator (GTK_DIALOG (dlg), FALSE);

		btn = gtk_dialog_run (GTK_DIALOG (dlg));
		gtk_widget_destroy (dlg);
	}
	
	if (btn == GTK_RESPONSE_YES)
		*filename = g_strdup (path);
	
	gtk_main_quit ();
}

#ifdef USE_GTKFILECHOOSER
static void
filechooser_response (GtkWidget *fc, gint response_id, gpointer data)
{
	if (response_id == GTK_RESPONSE_ACCEPT)
		save_ok (fc, data);
	else
		gtk_widget_destroy (fc);
}
#endif

char *
e_file_dialog_save (const char *title)
{
	GtkWidget *selection;
	char *path, *filename = NULL;

#ifdef USE_GTKFILECHOOSER
	selection = gtk_file_chooser_dialog_new (title,
						 NULL,
						 GTK_FILE_CHOOSER_ACTION_SAVE,
						 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
						 GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
						 NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (selection), GTK_RESPONSE_ACCEPT);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (selection), g_get_home_dir ());

	g_signal_connect (G_OBJECT (selection), "response", G_CALLBACK (filechooser_response), &filename);
#else
	selection = gtk_file_selection_new (title);
	path = g_strdup_printf ("%s/", g_get_home_dir ());
	gtk_file_selection_set_filename (GTK_FILE_SELECTION (selection), path);
	g_free (path);

	g_signal_connect (G_OBJECT (GTK_FILE_SELECTION (selection)->ok_button), "clicked", G_CALLBACK (save_ok), &filename);
	g_signal_connect (G_OBJECT (GTK_FILE_SELECTION (selection)->cancel_button), "clicked", G_CALLBACK (gtk_main_quit), NULL);
#endif
	
	gtk_widget_show (GTK_WIDGET (selection));
	gtk_grab_add (GTK_WIDGET (selection));
	gtk_main ();
	
	gtk_widget_destroy (GTK_WIDGET (selection));
	
	return filename;
}


