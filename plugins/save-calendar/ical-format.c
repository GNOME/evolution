/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Authors: Rodrigo Moya <rodrigo@novell.com> 
 *           Philip Van Hoof <pvanhoof@gnome.org>
 *           
 *  Copyright 2004 Novell, Inc. (www.novell.com)
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU General Public
 *  License as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <glib.h>
#include <glib/gi18n.h>
#ifdef USE_GTKFILECHOOSER
#  include <gtk/gtkfilechooser.h>
#  include <gtk/gtkfilechooserdialog.h>
#else
#  include <gtk/gtkfilesel.h>
#endif
#include <libgnomevfs/gnome-vfs-ops.h>
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtkstock.h>
#include <gtk/gtk.h>
#include <libedataserver/e-source.h>
#include <libedataserverui/e-source-selector.h>
#include <libecal/e-cal.h>
#include <libecal/e-cal-util.h>
#include <calendar/gui/e-cal-popup.h>
#include <libgnomevfs/gnome-vfs.h>
#include <string.h>

#include "format-handler.h"

static void
display_error_message (GtkWidget *parent, const char *message)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new (GTK_WINDOW (parent), 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, message);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

static void
do_save_calendar_ical (FormatHandler *handler, EPlugin *ep, ECalPopupTargetSource *target, ECalSourceType type, char *dest_uri)
{
	ESource *primary_source;
	ECal *source_client;
	GError *error = NULL;
	GList *objects;
	icalcomponent *top_level = NULL;

	primary_source = e_source_selector_peek_primary_selection (target->selector);

	if (!dest_uri)
		return;

	/* open source client */
	source_client = auth_new_cal_from_source (primary_source, type);
	if (!e_cal_open (source_client, TRUE, &error)) {
		display_error_message (gtk_widget_get_toplevel (GTK_WIDGET (target->selector)), error->message);
		g_object_unref (source_client);
		g_error_free (error);
		return;
	}

	/* create destination file */
	top_level = e_cal_util_new_top_level ();

	error = NULL;
	if (e_cal_get_object_list (source_client, "#t", &objects, &error)) {
		GnomeVFSResult result;
		GnomeVFSHandle *handle;

		while (objects != NULL) {
			icalcomponent *icalcomp = objects->data;
			
			icalcomponent_add_component (top_level, icalcomp);

			/* remove item from the list */
			objects = g_list_remove (objects, icalcomp);
		}

		/* save the file */
		result = gnome_vfs_open (&handle, dest_uri, GNOME_VFS_OPEN_WRITE);
		if (result != GNOME_VFS_OK) {
			if ((result = gnome_vfs_create (&handle, dest_uri, GNOME_VFS_OPEN_WRITE,
							TRUE, GNOME_VFS_PERM_USER_ALL)) != GNOME_VFS_OK) {
				display_error_message (gtk_widget_get_toplevel (GTK_WIDGET (target->selector)),
					       gnome_vfs_result_to_string (result));
			}
		}

		if (result == GNOME_VFS_OK) {
			char *ical_str;
			GnomeVFSFileSize bytes_written;

			ical_str = icalcomponent_as_ical_string (top_level);
			if ((result = gnome_vfs_write (handle, (gconstpointer) ical_str, strlen (ical_str), &bytes_written))
			    != GNOME_VFS_OK) {
				display_error_message (gtk_widget_get_toplevel (GTK_WIDGET (target->selector)),
						       gnome_vfs_result_to_string (result));
			}

			gnome_vfs_close (handle);
		}
	} else {
		display_error_message (gtk_widget_get_toplevel (GTK_WIDGET (target->selector)), error->message);
		g_error_free (error);
	}

	/* terminate */
	g_object_unref (source_client);
	icalcomponent_free (top_level);
}

FormatHandler *ical_format_handler_new (void)
{
	FormatHandler *handler = g_new (FormatHandler, 1);

	handler->isdefault = TRUE;
	handler->combo_label = _("iCalendar format (.ics)");
	handler->filename_ext = ".ics";
	handler->options_widget = NULL;
	handler->save = do_save_calendar_ical;
	handler->data = NULL;

	return handler;
}
