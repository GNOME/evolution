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
	
};

static ENameSelectorEntryClass *parent_class;

static void
esne_start_editing (GtkCellEditable *cell_editable, GdkEvent *event)
{
	ESelectNamesEditable *esne = E_SELECT_NAMES_EDITABLE (cell_editable);

	/* Grab the focus */

	/* TODO */
}

static void
esne_cell_editable_init (GtkCellEditableIface *iface)
{
	iface->start_editing = esne_start_editing;
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

static void
entry_activate (ESelectNamesEditable *esne)
{
	gtk_cell_editable_editing_done (GTK_CELL_EDITABLE (esne));
	gtk_cell_editable_remove_widget (GTK_CELL_EDITABLE (esne));
}

ESelectNamesEditable *
e_select_names_editable_construct (ESelectNamesEditable *esne)
{
	g_signal_connect (esne, "activate", G_CALLBACK (entry_activate), esne);

	return esne;

#if 0
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	esne->priv->select_names = bonobo_activation_activate_from_id (SELECT_NAMES_OAFIID, 0, NULL, &ev);
	if (BONOBO_EX (&ev)) {
		CORBA_exception_free (&ev);
		return NULL;
	}

	GNOME_Evolution_Addressbook_SelectNames_addSection (esne->priv->select_names, "A", "A", &ev);
	if (BONOBO_EX (&ev)) {
		CORBA_exception_free (&ev);
		return NULL;
	}

	esne->priv->control = GNOME_Evolution_Addressbook_SelectNames_getEntryBySection (
			esne->priv->select_names, "A", &ev);
	if (BONOBO_EX (&ev)) {
		CORBA_exception_free (&ev);
		return NULL;
	}

	bonobo_widget_construct_control_from_objref (BONOBO_WIDGET (esne), esne->priv->control, CORBA_OBJECT_NIL, &ev);

	CORBA_exception_free (&ev);

	esne->priv->bag = bonobo_control_frame_get_control_property_bag (
			bonobo_widget_get_control_frame (BONOBO_WIDGET (esne)), NULL);
	bonobo_event_source_client_add_listener (esne->priv->bag, entry_activate,
						 "GNOME/Evolution/Addressbook/SelectNames:activate:entry",
						 NULL, esne);

	return esne;
#endif
}

ESelectNamesEditable *
e_select_names_editable_new ()
{
	ESelectNamesEditable *esne = g_object_new (E_TYPE_SELECT_NAMES_EDITABLE, NULL);

	if (!esne)
		return NULL;

	if (!e_select_names_editable_construct (esne)) {
		g_object_unref (esne);
		return NULL;
	}

	return esne;
}

gchar *
e_select_names_editable_get_address (ESelectNamesEditable *esne)
{
	EDestinationStore *destination_store;
	GList *destinations;
	EDestination *destination;
	gchar *dest_str;
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

gchar *
e_select_names_editable_get_name (ESelectNamesEditable *esne)
{
	EDestinationStore *destination_store;
	GList *destinations;
	EDestination *destination;
	gchar *dest_str;
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

void
e_select_names_editable_set_address (ESelectNamesEditable *esne, const gchar *text)
{
	EDestinationStore *destination_store;
	GList *destinations;
	EDestination *destination;
	gchar *dest_str;
	gchar *result = NULL;

	g_return_if_fail (E_IS_SELECT_NAMES_EDITABLE (esne));

	destination_store = e_name_selector_entry_peek_destination_store (E_NAME_SELECTOR_ENTRY (esne));
	destinations = e_destination_store_list_destinations (destination_store);
	if (!destinations)
		return;

	destination = destinations->data;
	e_destination_set_address (destination, text);
	g_list_free (destinations);
}

