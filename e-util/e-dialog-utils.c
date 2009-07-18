/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Michael Meeks <michael@ximian.com>
 *	Ettore Perazzoli <ettore@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-dialog-utils.h"

#include <errno.h>
#include <unistd.h>
#include <glib/gstdio.h>

#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif

#include <gconf/gconf-client.h>
#include <glib/gi18n.h>

#include "e-util/e-util.h"
#include "e-util/e-error.h"


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
e_notice (gpointer parent, GtkMessageType type, const gchar *format, ...)
{
	GtkWidget *dialog;
	va_list args;
	gchar *str;

	va_start (args, format);
	str = g_strdup_vprintf (format, args);
	dialog = gtk_message_dialog_new (NULL,
					 GTK_DIALOG_DESTROY_WITH_PARENT,
					 type,
					 GTK_BUTTONS_OK,
					 "%s",
					 str);
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
e_notice_with_xid (GdkNativeWindow parent, GtkMessageType type, const gchar *format, ...)
{
	GtkWidget *dialog;
	va_list args;
	gchar *str;

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


#ifdef GDK_WINDOWING_X11
/* Tests whether or not an X Window is being managed by the
 * window manager.
 */
static gboolean
window_is_wm_toplevel (Display *display, Window window)
{
	static Atom WM_STATE = None;
	unsigned long nitems, after;
	guchar *data = NULL;
	Atom type = None;
	gint format;

	if (!WM_STATE)
		WM_STATE = XInternAtom (display, "WM_STATE", False);

	if (XGetWindowProperty (display, window, WM_STATE, 0, 0, False,
				AnyPropertyType, &type, &format,
				&nitems, &after, &data) == Success) {
		if (data)
			XFree((gchar *)data);
		if (type)
			return TRUE;
	}
	return FALSE;
}

#endif

/**
 * e_dialog_set_transient_for:
 * @dialog: a dialog window
 * @parent_widget: the parent for @dialog
 *
 * This sets the parent for @dialog to be @parent_widget. Unlike
 * gtk_window_set_transient_for (), this doesn't need @parent_widget to
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
#ifdef GDK_WINDOWING_X11
	Window parent, root_ret, *children;
	guint numchildren;
	Display *display;
	Status status;
#endif
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
#ifdef GDK_WINDOWING_X11
	/* Find the top-level windowmanager-managed X Window */
	display = GDK_WINDOW_XDISPLAY (gtk_widget_get_window (parent_widget));
	parent = GDK_WINDOW_XID (gtk_widget_get_window (parent_widget));

	while (parent && !window_is_wm_toplevel (display, parent)) {
		status = XQueryTree (display, parent, &root_ret,
				     &parent, &children, &numchildren);
		if (status != 0)
			XFree (children);
	}

	e_dialog_set_transient_for_xid (dialog, parent);
#endif
#ifdef GDK_WINDOWING_WIN32
	g_warning ("Not implemented on Win32: e_dialog_set_transient_for () for plug windows");
#endif
}

static void
dialog_realized (GtkWindow *dialog, gpointer xid)
{
	e_dialog_set_transient_for_xid (dialog, (GdkNativeWindow)GPOINTER_TO_INT(xid));
}

/**
 * e_dialog_set_transient_for_xid:
 * @dialog: a dialog window
 * @xid: the X Window parent
 *
 * Like e_dialog_set_transient_for (), but use an XID to specify the
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
				  GINT_TO_POINTER(xid));
		return;
	}

#ifdef GDK_MULTIHEAD_SAFE
	display = gdk_drawable_get_display (GDK_DRAWABLE (gtk_widget_get_window (GTK_WIDGET (dialog))));
	parent = gdk_window_lookup_for_display (display, xid);
	if (!parent)
		parent = gdk_window_foreign_new_for_display (display, xid);
#else
	parent = gdk_window_lookup (xid);
	if (!parent)
		parent = gdk_window_foreign_new (xid);
#endif
	g_return_if_fail (parent != NULL);

	gdk_window_set_transient_for (gtk_widget_get_window (GTK_WIDGET (dialog)), parent);
}


static void
save_ok (GtkWidget *widget, gpointer data)
{
	GtkWidget *fs;
	gchar **filename = data;
	gchar *uri;

	fs = gtk_widget_get_toplevel (widget);
	uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (fs));

	if (e_file_can_save((GtkWindow *)widget, uri)) {
		e_file_update_save_path(gtk_file_chooser_get_current_folder_uri(GTK_FILE_CHOOSER(fs)), TRUE);
		*filename = uri;
	}

	gtk_main_quit ();
}

static void
filechooser_response (GtkWidget *fc, gint response_id, gpointer data)
{
	if (response_id == GTK_RESPONSE_OK)
		save_ok (fc, data);
	else
		gtk_widget_destroy (fc);
}

gchar *
e_file_dialog_save (const gchar *title, const gchar *fname)
{
	GtkWidget *selection;
	gchar *filename = NULL;

	selection = e_file_get_save_filesel(NULL, title, fname, GTK_FILE_CHOOSER_ACTION_SAVE);

	g_signal_connect (G_OBJECT (selection), "response", G_CALLBACK (filechooser_response), &filename);

	gtk_widget_show (GTK_WIDGET (selection));
	gtk_grab_add (GTK_WIDGET (selection));
	gtk_main ();

	gtk_widget_destroy (GTK_WIDGET (selection));

	return filename;
}

static void
save_folder_ok (GtkWidget *widget, gpointer data)
{
	GtkWidget *fs;
	gchar **filename = data;
	gchar *uri;

	fs = gtk_widget_get_toplevel (widget);
	uri = gtk_file_chooser_get_current_folder_uri (GTK_FILE_CHOOSER (fs));

	e_file_update_save_path(uri, FALSE);
	*filename = uri;

	gtk_main_quit ();
}

static void
folderchooser_response (GtkWidget *fc, gint response_id, gpointer data)
{
	if (response_id == GTK_RESPONSE_OK)
		save_folder_ok (fc, data);
	else
		gtk_widget_destroy (fc);
}

gchar *
e_file_dialog_save_folder (const gchar *title)
{
	GtkWidget *selection;
	gchar *filename = NULL;

	selection = e_file_get_save_filesel(NULL, title, NULL, GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
	g_signal_connect (G_OBJECT (selection), "response", G_CALLBACK (folderchooser_response), &filename);

	gtk_widget_show (GTK_WIDGET (selection));
	gtk_grab_add (GTK_WIDGET (selection));
	gtk_main ();

	gtk_widget_destroy (GTK_WIDGET (selection));

	return filename;
}

/**
 * e_file_get_save_filesel:
 * @parent: parent window
 * @title: dialog title
 * @name: filename; already in a proper form (suitable for file system)
 * @action: action for dialog
 *
 * Creates a save dialog, using the saved directory from gconf.   The dialog has
 * no signals connected and is not shown.
 **/
GtkWidget *
e_file_get_save_filesel (GtkWidget *parent, const gchar *title, const gchar *name, GtkFileChooserAction action)
{
	GtkWidget *filesel;
	gchar *uri;

	filesel = gtk_file_chooser_dialog_new (title,
					       NULL,
					       action,
					       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					       (action == GTK_FILE_CHOOSER_ACTION_OPEN) ? GTK_STOCK_OPEN:GTK_STOCK_SAVE, GTK_RESPONSE_OK,
					       NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (filesel), GTK_RESPONSE_OK);
	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (filesel), FALSE);

	if (parent)
		e_dialog_set_transient_for ((GtkWindow *)filesel, parent);

	uri = e_file_get_save_path();

	gtk_file_chooser_set_current_folder_uri (GTK_FILE_CHOOSER (filesel), uri);

	if (name && name[0])
		gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (filesel), name);

	g_free (uri);

	return filesel;
}

/**
 * e_file_can_save:
 *
 * Return TRUE if the URI can be saved to, FALSE otherwise.  It checks local
 * files to see if they're regular and can be accessed.  If the file exists and
 * is writable, it pops up a dialog asking the user if they want to overwrite
 * it.  Returns the users choice.
 **/
gboolean
e_file_can_save(GtkWindow *parent, const gchar *uri)
{
	struct stat st;
	gchar *path;
	gboolean res;

	if (!uri || uri[0] == 0)
		return FALSE;

	/* Assume remote files are writable; too costly to check */
	if (!e_file_check_local(uri))
		return TRUE;

	path = e_util_uri_to_filename (uri);
	if (!path)
		return FALSE;

	/* make sure we can actually save to it... */
	if (g_stat (path, &st) != -1 && !S_ISREG (st.st_mode)) {
		g_free(path);
		return FALSE;
	}

	res = TRUE;
	if (g_access (path, F_OK) == 0) {
		if (g_access (path, W_OK) != 0) { e_error_run(parent, "mail:no-save-path", path, g_strerror(errno), NULL);
			g_free(path);
			return FALSE;
		}

		res = e_error_run(parent, E_ERROR_ASK_FILE_EXISTS_OVERWRITE, path, NULL) == GTK_RESPONSE_OK;

	}

	g_free(path);
	return res;
}

gboolean
e_file_check_local (const gchar *name)
{
	gchar *uri;

	uri = e_util_uri_to_filename (name);
	if (uri) {
		g_free(uri);
		return TRUE;
	}

	return FALSE;
}
