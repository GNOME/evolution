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
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtkstock.h>
#include <gtk/gtk.h>
#include <libedataserver/e-source.h>
#include <libedataserverui/e-source-selector.h>
#include <libecal/e-cal.h>
#include <calendar/gui/e-cal-popup.h>
#include <libgnomevfs/gnome-vfs.h>
#include <string.h>

#include "format-handler.h"

static void
display_error_message (GtkWidget *parent, GError *error)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new (GTK_WINDOW (parent), 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
					 error->message);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

static void
do_save_calendar_ical (FormatHandler *handler, EPlugin *ep, ECalPopupTargetSource *target, ECalSourceType type, char *dest_uri)
{
	ESource *primary_source;
	ECal *source_client, *dest_client;
	GError *error = NULL;

	primary_source = e_source_selector_peek_primary_selection (target->selector);

	if (!dest_uri)
		return;

	/* open source client */
	source_client = e_cal_new (primary_source, type);
	if (!e_cal_open (source_client, TRUE, &error)) {
		display_error_message (gtk_widget_get_toplevel (GTK_WIDGET (target->selector)), error);
		g_object_unref (source_client);
		g_error_free (error);
		return;
	}

	/* open destination client */
	error = NULL;
	dest_client = e_cal_new_from_uri (dest_uri, type);
	if (e_cal_open (dest_client, FALSE, &error)) {
		GList *objects;

		if (e_cal_get_object_list (source_client, "#t", &objects, NULL)) {
			while (objects != NULL) {
				icalcomponent *icalcomp = objects->data;

				/* FIXME: deal with additions/modifications */

				/* FIXME: This stores a directory with one file in it, the user expects only a file */

				/* FIXME: It would be nice if this ical-handler would use gnome-vfs rather than e_cal_* */

				error = NULL;
				if (!e_cal_create_object (dest_client, icalcomp, NULL, &error)) {
					display_error_message (gtk_widget_get_toplevel (GTK_WIDGET (target->selector)), error);
					g_error_free (error);
				}

				/* remove item from the list */
				objects = g_list_remove (objects, icalcomp);
				icalcomponent_free (icalcomp);
			}
		}
	} else {
		display_error_message (gtk_widget_get_toplevel (GTK_WIDGET (target->selector)), error);
		g_error_free (error);
	}

	/* terminate */
	g_object_unref (source_client);
	g_object_unref (dest_client);
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
