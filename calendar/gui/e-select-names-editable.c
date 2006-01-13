/*
 * e-select-names-editable.c
 *
 * Author: Mike Kestner  <mkestner@ximian.com>
 *
 * Copyright (C) 2003 Ximian Inc.
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
 */

#include <config.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkcelleditable.h>
#include <libebook/e-destination.h>
#include <libedataserverui/e-name-selector-entry.h>

#include "e-select-names-editable.h"

struct _ESelectNamesEditablePriv {
	gint dummy;
};

static ENameSelectorEntryClass *parent_class;

static void
esne_cell_editable_init (GtkCellEditableIface *iface)
{
}

static void
esne_finalize (GObject *obj)
{
	ESelectNamesEditable *esne = (ESelectNamesEditable *) obj;

	g_free (esne->priv);

	if (G_OBJECT_CLASS (parent_class)->finalize)
		G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
esne_init (ESelectNamesEditable *esne)
{
	esne->priv = g_new0 (ESelectNamesEditablePriv, 1);
}

static void
esne_class_init (GObjectClass *klass)
{
	klass->finalize = esne_finalize;
	
	parent_class = E_NAME_SELECTOR_ENTRY_CLASS (g_type_class_peek_parent (klass));
}

GType
e_select_names_editable_get_type (void)
{
	static GType esne_type = 0;
	
	if (!esne_type) {
		static const GTypeInfo esne_info = {
			sizeof (ESelectNamesEditableClass),
			NULL,		/* base_init */
			NULL,		/* base_finalize */
			(GClassInitFunc) esne_class_init,
			NULL,		/* class_finalize */
			NULL,		/* class_data */
			sizeof (ESelectNamesEditable),
			0,              /* n_preallocs */
			(GInstanceInitFunc) esne_init,
		};

		static const GInterfaceInfo cell_editable_info = {
			(GInterfaceInitFunc) esne_cell_editable_init,
			NULL, 
			NULL 
		};
      
		esne_type = g_type_register_static (E_TYPE_NAME_SELECTOR_ENTRY, "ESelectNamesEditable", &esne_info, 0);
		
		g_type_add_interface_static (esne_type, GTK_TYPE_CELL_EDITABLE, &cell_editable_info);
	}
	
	return esne_type;
}

ESelectNamesEditable *
e_select_names_editable_new ()
{
	ESelectNamesEditable *esne = g_object_new (E_TYPE_SELECT_NAMES_EDITABLE, NULL);

	return esne;
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
			if (e_contact_get (e_destination_get_contact (destination), E_CONTACT_IS_LIST)) {
				/* If its a contact_list which is not expanded, it wont have a email id,
				   so we can use the name as the email id */

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
	GList *destinations;
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
e_select_names_editable_set_address (ESelectNamesEditable *esne, const gchar *name, const gchar *email)
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
