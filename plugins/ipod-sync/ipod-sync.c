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
 *		Srinivasa Ragavan <sragavan@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/* This is written from the save-calendar plugin and James Bowes evo-iPod
 * sync code.
 *
 * This provides eplugin support to sync calendar/task/addressbook with iPod
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <gio/gio.h>
#include <glib/gi18n.h>
#include <unistd.h>

#include <libebook/e-book.h>
#include <libebook/e-contact.h>

#include <libedataserver/e-source.h>
#include <libedataserverui/e-source-selector.h>
#include <libecal/e-cal.h>
#include <calendar/gui/e-cal-popup.h>
#include <addressbook/gui/widgets/eab-popup.h>
#include <string.h>

#include "e-util/e-error.h"
#include "format-handler.h"
#include "evolution-ipod-sync.h"

void org_gnome_sync_calendar (EPlugin *ep, ECalPopupTargetSource *target);
void org_gnome_sync_tasks (EPlugin *ep, ECalPopupTargetSource *target);
void org_gnome_sync_memos (EPlugin *ep, ECalPopupTargetSource *target);
void org_gnome_sync_addressbook (EPlugin *ep, EABPopupTargetSource *target);

static void
display_error_message (GtkWidget *parent, const gchar *message)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new (GTK_WINDOW (parent), 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "%s", message);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

/* Returns output stream for the uri, or NULL on any error.
   When done with the stream, just g_output_stream_close and g_object_unref it.
   It will ask for overwrite if file already exists.
*/
GOutputStream *
open_for_writing (GtkWindow *parent, const gchar *uri, GError **error)
{
	GFile *file;
	GFileOutputStream *fostream;
	GError *err = NULL;

	g_return_val_if_fail (uri != NULL, NULL);

	file = g_file_new_for_uri (uri);

	g_return_val_if_fail (file != NULL, NULL);

	fostream = g_file_create (file, G_FILE_CREATE_NONE, NULL, &err);

	if (err && err->code == G_IO_ERROR_EXISTS) {
		g_clear_error (&err);

		if (e_error_run (parent, E_ERROR_ASK_FILE_EXISTS_OVERWRITE, uri, NULL) == GTK_RESPONSE_OK) {
			fostream = g_file_replace (file, NULL, FALSE, G_FILE_CREATE_NONE, NULL, &err);

			if (err && fostream) {
				g_object_unref (fostream);
				fostream = NULL;
			}
		} else if (fostream) {
			g_object_unref (fostream);
			fostream = NULL;
		}
	}

	g_object_unref (file);

	if (error && err)
		*error = err;
	else if (err)
		g_error_free (err);

	if (fostream)
		return G_OUTPUT_STREAM (fostream);

	return NULL;
}

static void
destination_save_addressbook  (EPlugin *ep, EABPopupTargetSource *target)
{
	EBook *book;
	EBookQuery *query;
	GList *contacts, *tmp;
	ESource *primary_source;
	gchar *uri;
	GOutputStream *stream;
	GError *error = NULL;
	gchar *dest_uri = NULL;
	gchar *mount = ipod_get_mount();

	primary_source = e_source_selector_peek_primary_selection (target->selector);

	/* use g_file api here to build path*/
	dest_uri = g_strdup_printf("%s/%s/Evolution-Addressbook-%s.vcf", mount, "Contacts", e_source_peek_name (primary_source));
	g_free (mount);

	uri = e_source_get_uri (primary_source);

	book = e_book_new_from_uri (uri, NULL);

	if (!book
	    || !e_book_open (book, TRUE, NULL)) {
		g_warning ("Couldn't load addressbook %s", uri);
		return;
	}

	/* Let us export some meaning full contacts */
	query = e_book_query_any_field_contains ("");
	e_book_get_contacts (book, query, &contacts, NULL);
	e_book_query_unref (query);

	stream = open_for_writing (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (target->selector))), dest_uri, &error);

	if (stream && !error) {
		for (tmp = contacts; tmp; tmp=tmp->next) {
			EContact *contact = tmp->data;
			gchar *temp = e_vcard_to_string (E_VCARD (contact), EVC_FORMAT_VCARD_30);
			gchar *vcard;
			gchar *converted_vcard;
			gsize vcard_latin1_length;

			vcard = g_strconcat (temp, "\r\n", NULL);
			converted_vcard = g_convert (vcard, -1, "ISO-8859-1", "UTF-8", NULL, &vcard_latin1_length, NULL);
			g_output_stream_write_all (stream, converted_vcard, vcard_latin1_length, NULL, NULL, &error);

			if (error) {
				display_error_message (gtk_widget_get_toplevel (GTK_WIDGET (target->selector)), error->message);
				g_clear_error (&error);
			}

			g_object_unref (contact);
			g_free (temp);
			g_free (vcard);
			g_free (converted_vcard);
		}

		g_output_stream_close (stream, NULL, NULL);
	}

	if (stream)
		g_object_unref (stream);

	sync();

	if (contacts != NULL)
		g_list_free (contacts);

	g_object_unref (book);
	g_free (dest_uri);
	g_free (uri);

	if (error) {
		display_error_message (gtk_widget_get_toplevel (GTK_WIDGET (target->selector)), error->message);
		g_error_free (error);
	}
}

static void
destination_save_cal (EPlugin *ep, ECalPopupTargetSource *target, ECalSourceType type)
{
	FormatHandler *handler = NULL;
	gchar *mount = ipod_get_mount();
	gchar *dest_uri = NULL, *path;
	ESource *primary_source = e_source_selector_peek_primary_selection (target->selector);

	/* The available formathandlers */
	handler= ical_format_handler_new ();

	switch (type) {
	case E_CAL_SOURCE_TYPE_EVENT:
		path = g_strdup_printf ("Evolution-Calendar-%s", e_source_peek_name (primary_source));
		break;
	case E_CAL_SOURCE_TYPE_TODO:
		path = g_strdup_printf ("Evolution-Tasks-%s", e_source_peek_name (primary_source));
		break;
	case E_CAL_SOURCE_TYPE_JOURNAL:
		path = g_strdup_printf ("Evolution-Memos-%s", e_source_peek_name (primary_source));
		break;
	default:
		path = NULL;
		g_assert_not_reached ();
	}

	dest_uri = g_strdup_printf("%s/%s/%s.ics", mount, "Calendars", path);
	g_free (path);

	handler->save (handler, ep, target, type, dest_uri);

	sync();

	g_free (dest_uri);
	g_free (mount);
	g_free (handler);
}

void
org_gnome_sync_calendar (EPlugin *ep, ECalPopupTargetSource *target)
{
	if (!ipod_check_status(FALSE))
		return;

	destination_save_cal (ep, target, E_CAL_SOURCE_TYPE_EVENT);
}

void
org_gnome_sync_tasks (EPlugin *ep, ECalPopupTargetSource *target)
{
	if (!ipod_check_status(FALSE))
		return;

	destination_save_cal (ep, target, E_CAL_SOURCE_TYPE_TODO);
}

void
org_gnome_sync_memos (EPlugin *ep, ECalPopupTargetSource *target)
{
	if (!ipod_check_status(FALSE))
		return;

	destination_save_cal (ep, target, E_CAL_SOURCE_TYPE_JOURNAL);
}

void
org_gnome_sync_addressbook (EPlugin *ep, EABPopupTargetSource *target)
{
	if (!ipod_check_status(FALSE))
		return;

	destination_save_addressbook (ep, target);
}
