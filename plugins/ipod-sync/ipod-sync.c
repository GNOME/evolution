/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Authors: Srinivasa Ragavan <sragavan@novell.com>
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

/* This is written from the save-calendar plugin and James Bowes evo-iPod
 * sync code.
 *
 * This provides eplugin support to sync calendar/task/addressbook with iPod
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n.h>

#include <libebook/e-book.h>
#include <libebook/e-contact.h>

#include <libedataserver/e-source.h>
#include <libedataserverui/e-source-selector.h>
#include <libecal/e-cal.h>
#include <calendar/gui/e-cal-popup.h>
#include <addressbook/gui/widgets/eab-popup.h>
#include <libgnomevfs/gnome-vfs.h>
#include <string.h>

#include "format-handler.h"
#include "evolution-ipod-sync.h"

void org_gnome_sync_calendar (EPlugin *ep, ECalPopupTargetSource *target);
void org_gnome_sync_tasks (EPlugin *ep, ECalPopupTargetSource *target);
void org_gnome_sync_addressbook (EPlugin *ep, EABPopupTargetSource *target);


static void
display_error_message (GtkWidget *parent, const char *message)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new (GTK_WINDOW (parent), 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, message);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

static void
destination_save_addressbook  (EPlugin *ep, EABPopupTargetSource *target)
{
	EBook *book;
	EBookQuery *query;
	GList *contacts, *tmp;
	ESource *primary_source;
	gchar *uri;
	char *dest_uri = NULL;
	GnomeVFSResult result;
	GnomeVFSHandle *handle;
	char *mount = ipod_get_mount();

	/* use g_file api here to build path*/
	dest_uri = g_strdup_printf("%s/%s/%s", mount, "Contacts", "evolution.vcf");
	g_free (mount);
	
	primary_source = e_source_selector_peek_primary_selection (target->selector);
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

	result = gnome_vfs_open (&handle, dest_uri, GNOME_VFS_OPEN_WRITE);

	if (result != GNOME_VFS_OK) {
		if ((result = gnome_vfs_create (&handle, dest_uri, GNOME_VFS_OPEN_WRITE,
						TRUE, GNOME_VFS_PERM_USER_ALL)) != GNOME_VFS_OK) {
			display_error_message (gtk_widget_get_toplevel (GTK_WIDGET (target->selector)),
				       gnome_vfs_result_to_string (result));
		}
	}

	if (result == GNOME_VFS_OK) {
		GnomeVFSFileSize bytes_written;

		for (tmp = contacts; tmp; tmp=tmp->next) {
				EContact *contact = tmp->data;
				gchar *temp = e_vcard_to_string (E_VCARD (contact), EVC_FORMAT_VCARD_30);
				gchar *vcard;
				
				vcard = g_strconcat(temp, "\r\n", NULL);
				if ((result = gnome_vfs_write (handle, (gconstpointer) vcard, strlen (vcard), &bytes_written))
				    != GNOME_VFS_OK) {
					display_error_message (gtk_widget_get_toplevel (GTK_WIDGET (target->selector)),
							       gnome_vfs_result_to_string (result));
				}

				g_object_unref (contact);
				g_free (temp);
				g_free (vcard);
		}
	}

	sync();

	if (contacts != NULL)
		g_list_free (contacts);

	gnome_vfs_close (handle);
	g_object_unref (book);
	g_free (dest_uri);
	g_free (uri);
}

static void 
destination_save_cal (EPlugin *ep, ECalPopupTargetSource *target, ECalSourceType type)
{
	FormatHandler *handler = NULL;
	char *mount = ipod_get_mount();
	char *dest_uri = NULL;

	/* The available formathandlers */
	handler= ical_format_handler_new ();

	dest_uri = g_strdup_printf("%s/%s/%s", mount, "Calendars", (type==E_CAL_SOURCE_TYPE_EVENT)? "evolution-calendar.ics":"evolution-todo.ics");

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
org_gnome_sync_addressbook (EPlugin *ep, EABPopupTargetSource *target)
{
	if (!ipod_check_status(FALSE))
			return;

	destination_save_addressbook (ep, target);
}
