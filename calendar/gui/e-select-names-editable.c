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
 *		Mike Kestner  <mkestner@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

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

ESelectNamesEditable *
e_select_names_editable_new (void)
{
	EShell *shell;

	/* Might be cleaner to have 'registry' passed in, but the call chain
	 * of this widget doesn't have access that low in the functions, thus
	 * making the change without (private) API break. */
	shell = e_shell_get_default ();

	return g_object_new (
		E_TYPE_SELECT_NAMES_EDITABLE,
		"registry", e_shell_get_registry (shell),
		NULL);
}

gchar *
e_select_names_editable_get_email (ESelectNamesEditable *esne)
{
	EDestinationStore *destination_store;
	GList *destinations;
	EDestination *destination;
	gchar *result = NULL;

	g_return_val_if_fail (E_SELECT_NAMES_EDITABLE (esne), NULL);

	destination_store = e_name_selector_entry_peek_destination_store (E_NAME_SELECTOR_ENTRY (esne));
	destinations = e_destination_store_list_destinations (destination_store);
	if (!destinations)
		return NULL;

	destination = destinations->data;
	result = g_strdup (e_destination_get_email (destination));
	g_list_free (destinations);
	return result;
}

GList *
e_select_names_editable_get_emails (ESelectNamesEditable *esne)
{
	EDestinationStore *destination_store;
	GList *destinations, *l;
	EDestination *destination;
	GList *result = NULL;

	g_return_val_if_fail (E_SELECT_NAMES_EDITABLE (esne), NULL);

	destination_store = e_name_selector_entry_peek_destination_store (E_NAME_SELECTOR_ENTRY (esne));
	destinations = e_destination_store_list_destinations (destination_store);
	if (!destinations)
		return NULL;

	for (l = destinations; l != NULL; l = l->next) {
		destination = l->data;
		if (e_destination_is_evolution_list (destination)) {
			const GList *list_dests, *l;

			list_dests = e_destination_list_get_dests (destination);
			for (l = list_dests; l != NULL; l = g_list_next (l)) {
				result = g_list_append (result, g_strdup (e_destination_get_email (l->data)));
			}
		} else {
			/* check if the contact is contact list, it does not contain all the email ids  */
			/* we dont expand it currently, TODO do we need to expand it by getting it from addressbook*/
			if (e_destination_get_contact (destination) &&
			    e_contact_get (e_destination_get_contact (destination), E_CONTACT_IS_LIST)) {
				/* If its a contact_list which is not expanded, it wont have a email id,
				 * so we can use the name as the email id */

				result = g_list_append (result, g_strdup (e_destination_get_name (destination)));
			} else
				result = g_list_append (result, g_strdup (e_destination_get_email (destination)));
		}
	}

	g_list_free (destinations);

	return result;
}

gchar *
e_select_names_editable_get_name (ESelectNamesEditable *esne)
{
	EDestinationStore *destination_store;
	GList *destinations;
	EDestination *destination;
	gchar *result = NULL;

	g_return_val_if_fail (E_SELECT_NAMES_EDITABLE (esne), NULL);

	destination_store = e_name_selector_entry_peek_destination_store (E_NAME_SELECTOR_ENTRY (esne));
	destinations = e_destination_store_list_destinations (destination_store);
	if (!destinations)
		return NULL;

	destination = destinations->data;
	result = g_strdup (e_destination_get_name (destination));
	g_list_free (destinations);
	return result;
}

GList *
e_select_names_editable_get_names (ESelectNamesEditable *esne)
{
	EDestinationStore *destination_store;
	GList *destinations, *l;
	EDestination *destination;
	GList *result = NULL;

	g_return_val_if_fail (E_SELECT_NAMES_EDITABLE (esne), NULL);

	destination_store = e_name_selector_entry_peek_destination_store (E_NAME_SELECTOR_ENTRY (esne));
	destinations = e_destination_store_list_destinations (destination_store);
	if (!destinations)
		return NULL;

	for (l = destinations; l != NULL; l = l->next) {
		destination = l->data;
		if (e_destination_is_evolution_list (destination)) {
			const GList *list_dests, *l;

			list_dests = e_destination_list_get_dests (destination);
			for (l = list_dests; l != NULL; l = g_list_next (l)) {
				result = g_list_append (result, g_strdup (e_destination_get_name (l->data)));
			}
		} else {
			result = g_list_append (result, g_strdup (e_destination_get_name (destination)));
		}
	}

	g_list_free (destinations);

	return result;
}

void
e_select_names_editable_set_address (ESelectNamesEditable *esne,
                                     const gchar *name,
                                     const gchar *email)
{
	EDestinationStore *destination_store;
	GList *destinations;
	EDestination *destination;

	g_return_if_fail (E_IS_SELECT_NAMES_EDITABLE (esne));

	destination_store = e_name_selector_entry_peek_destination_store (E_NAME_SELECTOR_ENTRY (esne));
	destinations = e_destination_store_list_destinations (destination_store);

	if (!destinations)
		destination = e_destination_new ();
	else
		destination = g_object_ref (destinations->data);

	e_destination_set_name (destination, name);
	e_destination_set_email (destination, email);

	if (!destinations)
		e_destination_store_append_destination (destination_store, destination);
	g_object_unref (destination);
}
