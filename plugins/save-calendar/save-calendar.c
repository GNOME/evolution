/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Authors: Rodrigo Moya <jpr@novell.com>
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

/* This is prototype code only, this may, or may not, use undocumented
 * unstable or private internal function calls. */

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
#include <gtk/gtkstock.h>
#include <libedataserver/e-source.h>
#include <libedataserverui/e-source-selector.h>
#include <libecal/e-cal.h>
#include <calendar/gui/e-cal-popup.h>

void org_gnome_save_calendar (EPlugin *ep, ECalPopupTargetSource *target);
void org_gnome_save_tasks (EPlugin *ep, ECalPopupTargetSource *target);

static void
do_save_calendar (EPlugin *ep, ECalPopupTargetSource *target, ECalSourceType type)
{
	ESource *primary_source;
	char *dest_uri;
	ECal *source_client, *dest_client;
	GtkWidget *dialog;
	
	primary_source = e_source_selector_peek_primary_selection (target->selector);

	/* ask the user for destination file */
#ifdef USE_GTKFILECHOOSER
	dialog = gtk_file_chooser_dialog_new (_("Select destination file"),
					      NULL,
					      GTK_FILE_CHOOSER_ACTION_SAVE,
					      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					      GTK_STOCK_SAVE, GTK_RESPONSE_OK,
					      NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
#else
	dialog = gtk_file_selection_new (_("Select destination file"));
#endif

	if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_OK) {
		gtk_widget_destroy (dialog);
		return;
	}

#ifdef USE_GTKFILECHOOSER
	dest_uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (dialog));
#else
	dest_uri = g_strdup (gtk_file_selection_get_filename (GTK_FILE_SELECTION (filesel)));
#endif
	gtk_widget_destroy (dialog);
	if (!dest_uri)
		return;

	/* open source client */
	source_client = e_cal_new (primary_source, type);
	if (!e_cal_open (source_client, TRUE, NULL)) {
		g_object_unref (source_client);
		g_free (dest_uri);
		return;
	}

	/* open destination client */
	dest_client = e_cal_new_from_uri (dest_uri, type);
	if (e_cal_open (dest_client, FALSE, NULL)) {
		GList *objects;

		if (e_cal_get_object_list (source_client, "#t", &objects, NULL)) {
			while (objects != NULL) {
				icalcomponent *icalcomp = objects->data;

				/* FIXME: deal with additions/modifications */
				e_cal_create_object (dest_client, icalcomp, NULL, NULL);

				/* remove item from the list */
				objects = g_list_remove (objects, icalcomp);
				icalcomponent_free (icalcomp);
			}
		}
	}

	/* terminate */
	g_object_unref (source_client);
	g_object_unref (dest_client);
	g_free (dest_uri);
}

void
org_gnome_save_calendar (EPlugin *ep, ECalPopupTargetSource *target)
{
	do_save_calendar (ep, target, E_CAL_SOURCE_TYPE_EVENT);
}

void
org_gnome_save_tasks (EPlugin *ep, ECalPopupTargetSource *target)
{
	do_save_calendar (ep, target, E_CAL_SOURCE_TYPE_TODO);
}
