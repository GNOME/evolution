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
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-widget.h>
#include <addressbook/util/e-destination.h>

#include "e-select-names-editable.h"
#include "Evolution-Addressbook-SelectNames.h"

#define SELECT_NAMES_OAFIID "OAFIID:GNOME_Evolution_Addressbook_SelectNames:" BASE_VERSION

struct _ESelectNamesEditablePriv {
	GNOME_Evolution_Addressbook_SelectNames select_names;
	Bonobo_Control control;
	Bonobo_PropertyBag bag;
};

static BonoboWidgetClass *parent_class;

static void
esne_start_editing (GtkCellEditable *cell_editable, GdkEvent *event)
{
	ESelectNamesEditable *esne = E_SELECT_NAMES_EDITABLE (cell_editable);
	BonoboControlFrame *cf;

	/* Grab the focus */
	cf = bonobo_widget_get_control_frame (BONOBO_WIDGET (cell_editable));
	bonobo_control_frame_control_activate (cf);
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

	if (esne->priv->select_names != CORBA_OBJECT_NIL)
		bonobo_object_release_unref (esne->priv->select_names, NULL);
	esne->priv->select_names = CORBA_OBJECT_NIL;

	if (esne->priv->bag != CORBA_OBJECT_NIL)
		bonobo_object_release_unref (esne->priv->bag, NULL);
	esne->priv->bag = CORBA_OBJECT_NIL;

	g_free (esne->priv);

	if (G_OBJECT_CLASS (parent_class)->finalize)
		G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
esne_init (ESelectNamesEditable *esne)
{
	esne->priv = g_new0 (ESelectNamesEditablePriv, 1);

	esne->priv->select_names = CORBA_OBJECT_NIL;
	esne->priv->control = CORBA_OBJECT_NIL;
	esne->priv->bag = CORBA_OBJECT_NIL;
}

static void
esne_class_init (GObjectClass *klass)
{
	klass->finalize = esne_finalize;
	
	parent_class = BONOBO_WIDGET_CLASS (g_type_class_peek_parent (klass));
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
      
		esne_type = g_type_register_static (BONOBO_TYPE_WIDGET, "ESelectNamesEditable", &esne_info, 0);
		
		g_type_add_interface_static (esne_type, GTK_TYPE_CELL_EDITABLE, &cell_editable_info);
	}
	
	return esne_type;
}

static void
entry_activate (BonoboListener *listener, const char *event_name, const CORBA_any *arg, CORBA_Environment *ev, gpointer esne)
{
	gtk_cell_editable_editing_done (GTK_CELL_EDITABLE (esne));
	gtk_cell_editable_remove_widget (GTK_CELL_EDITABLE (esne));
}

ESelectNamesEditable *
e_select_names_editable_construct (ESelectNamesEditable *esne)
{
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
	EDestination **dest;
	gchar *dest_str;
	gchar *result = NULL;

	g_return_val_if_fail (E_SELECT_NAMES_EDITABLE (esne), NULL);

	dest_str = bonobo_pbclient_get_string (esne->priv->bag, "destinations", NULL);
	dest = e_destination_importv (dest_str);
	if (dest)
		result = g_strdup (e_destination_get_email (*dest));
	e_destination_freev (dest);

	return result;
}

gchar *
e_select_names_editable_get_name (ESelectNamesEditable *esne)
{
	EDestination **dest;
	gchar *dest_str;
	gchar *result = NULL;

	g_return_val_if_fail (E_SELECT_NAMES_EDITABLE (esne), NULL);

	dest_str = bonobo_pbclient_get_string (esne->priv->bag, "destinations", NULL);
	dest = e_destination_importv (dest_str);
	if (dest)
		result = g_strdup (e_destination_get_name (*dest));
	e_destination_freev (dest);

	return result;
}

void
e_select_names_editable_set_address (ESelectNamesEditable *esne, const gchar *text)
{
	g_return_if_fail (E_IS_SELECT_NAMES_EDITABLE (esne));

	bonobo_pbclient_set_string (esne->priv->bag, "addresses", text, NULL);
}

