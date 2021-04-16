/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Mike Kestner  <mkestner@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <libebook/libebook.h>
#include <shell/e-shell.h>

#include "e-select-names-editable.h"

G_DEFINE_TYPE_WITH_CODE (
	ESelectNamesEditable,
	e_select_names_editable,
	E_TYPE_NAME_SELECTOR_ENTRY,
	G_IMPLEMENT_INTERFACE (
		GTK_TYPE_CELL_EDITABLE, NULL))

static void
e_select_names_editable_class_init (ESelectNamesEditableClass *class)
{
}

static void
e_select_names_editable_init (ESelectNamesEditable *esne)
{
}

GtkWidget *
e_select_names_editable_new (EClientCache *client_cache)
{
	g_return_val_if_fail (E_IS_CLIENT_CACHE (client_cache), NULL);

	return g_object_new (
		E_TYPE_SELECT_NAMES_EDITABLE,
		"client-cache", client_cache, NULL);
}

EDestination *
e_select_names_editable_get_destination (ESelectNamesEditable *esne)
{
	EDestinationStore *destination_store;
	EDestination *destination = NULL;
	GList *list;

	g_return_val_if_fail (E_SELECT_NAMES_EDITABLE (esne), NULL);

	destination_store = e_name_selector_entry_peek_destination_store (E_NAME_SELECTOR_ENTRY (esne));
	list = e_destination_store_list_destinations (destination_store);
	if (list == NULL)
		return NULL;

	if (list && !list->next) {
		destination = E_DESTINATION (list->data);
	}

	g_list_free (list);

	return destination;
}

gchar *
e_select_names_editable_get_email (ESelectNamesEditable *esne)
{
	EDestinationStore *destination_store;
	GList *list;
	gchar *result = NULL;

	g_return_val_if_fail (E_SELECT_NAMES_EDITABLE (esne), NULL);

	destination_store = e_name_selector_entry_peek_destination_store (
		E_NAME_SELECTOR_ENTRY (esne));
	list = e_destination_store_list_destinations (destination_store);
	if (list == NULL)
		return NULL;

	if (list != NULL) {
		EDestination *destination;

		destination = E_DESTINATION (list->data);
		result = g_strdup (e_destination_get_email (destination));
		g_list_free (list);
	}

	return result;
}

GList *
e_select_names_editable_get_emails (ESelectNamesEditable *esne)
{
	EDestinationStore *destination_store;
	GQueue queue = G_QUEUE_INIT;
	GList *list, *link;

	g_return_val_if_fail (E_SELECT_NAMES_EDITABLE (esne), NULL);

	destination_store = e_name_selector_entry_peek_destination_store (
		E_NAME_SELECTOR_ENTRY (esne));
	list = e_destination_store_list_destinations (destination_store);

	for (link = list; link != NULL; link = g_list_next (link)) {
		EDestination *destination;

		destination = E_DESTINATION (link->data);

		if (e_destination_is_evolution_list (destination)) {
			const GList *list_dests;

			list_dests = e_destination_list_get_dests (destination);
			while (list_dests != NULL) {
				const gchar *email;

				destination = E_DESTINATION (list_dests->data);
				email = e_destination_get_email (destination);
				g_queue_push_tail (&queue, g_strdup (email));

				list_dests = g_list_next (list_dests);
			}

		} else {
			EContact *contact;
			const gchar *name;
			const gchar *email;
			gboolean contact_is_list;

			contact = e_destination_get_contact (destination);
			name = e_destination_get_name (destination);
			email = e_destination_get_email (destination);

			contact_is_list =
				(contact != NULL) &&
				e_contact_get (contact, E_CONTACT_IS_LIST);

			if (contact_is_list) {
				/* If its a contact_list which is not expanded,
				 * it won't have an email id, so we can use the
				 * name as the email id. */
				g_queue_push_tail (&queue, g_strdup (name));
			} else {
				g_queue_push_tail (&queue, g_strdup (email));
			}
		}
	}

	g_list_free (list);

	return queue.head;
}

gchar *
e_select_names_editable_get_name (ESelectNamesEditable *esne)
{
	EDestinationStore *destination_store;
	GList *list;
	gchar *result = NULL;

	g_return_val_if_fail (E_SELECT_NAMES_EDITABLE (esne), NULL);

	destination_store = e_name_selector_entry_peek_destination_store (
		E_NAME_SELECTOR_ENTRY (esne));
	list = e_destination_store_list_destinations (destination_store);

	if (list != NULL) {
		EDestination *destination;

		destination = E_DESTINATION (list->data);
		result = g_strdup (e_destination_get_name (destination));
		g_list_free (list);
	}

	return result;
}

GList *
e_select_names_editable_get_names (ESelectNamesEditable *esne)
{
	EDestinationStore *destination_store;
	GQueue queue = G_QUEUE_INIT;
	GList *list, *link;

	g_return_val_if_fail (E_SELECT_NAMES_EDITABLE (esne), NULL);

	destination_store = e_name_selector_entry_peek_destination_store (
		E_NAME_SELECTOR_ENTRY (esne));
	list = e_destination_store_list_destinations (destination_store);

	for (link = list; link != NULL; link = g_list_next (link)) {
		EDestination *destination;

		destination = E_DESTINATION (link->data);

		if (e_destination_is_evolution_list (destination)) {
			const GList *list_dests;

			list_dests = e_destination_list_get_dests (destination);
			while (list_dests != NULL) {
				const gchar *name;

				destination = E_DESTINATION (list_dests->data);
				name = e_destination_get_name (destination);
				g_queue_push_tail (&queue, g_strdup (name));

				list_dests = g_list_next (list_dests);
			}
		} else {
			const gchar *name;

			name = e_destination_get_name (destination);
			g_queue_push_tail (&queue, g_strdup (name));
		}
	}

	g_list_free (list);

	return queue.head;
}

void
e_select_names_editable_set_address (ESelectNamesEditable *esne,
                                     const gchar *name,
                                     const gchar *email)
{
	EDestinationStore *destination_store;
	GList *list;
	EDestination *destination;

	g_return_if_fail (E_IS_SELECT_NAMES_EDITABLE (esne));

	destination_store = e_name_selector_entry_peek_destination_store (
		E_NAME_SELECTOR_ENTRY (esne));
	list = e_destination_store_list_destinations (destination_store);

	if (list == NULL)
		destination = e_destination_new ();
	else
		destination = g_object_ref (list->data);

	e_destination_set_name (destination, name);
	e_destination_set_email (destination, email);

	if (list == NULL)
		e_destination_store_append_destination (
			destination_store, destination);

	g_object_unref (destination);
	g_list_free (list);
}
