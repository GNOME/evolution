/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Evolution calendar - Delegate selector dialog
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Damon Chaplin <damon@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <liboaf/liboaf.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-widget.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkcombo.h>
#include <gtk/gtkentry.h>
#include <gtk/gtklabel.h>
#include <gnome.h>
#include <ical.h>
#include <glade/glade.h>
#include <widgets/misc/e-map.h>
#include <ebook/e-destination.h>
#include "Evolution-Addressbook-SelectNames.h"
#include "e-delegate-dialog.h"

struct _EDelegateDialogPrivate {
	char *name;
	char *address;

	/* Glade XML data */
	GladeXML *xml;

	/* Widgets from the Glade file */
	GtkWidget *app;
	GtkWidget *hbox;
	GtkWidget *addressbook;

        GNOME_Evolution_Addressbook_SelectNames corba_select_names;	
	GtkWidget *entry;	
};

#define SELECT_NAMES_OAFID "OAFIID:GNOME_Evolution_Addressbook_SelectNames"
static const char *section_name = "Delegate To";

static void e_delegate_dialog_class_init	(EDelegateDialogClass *class);
static void e_delegate_dialog_init		(EDelegateDialog      *edd);
static void e_delegate_dialog_destroy		(GtkObject	*object);

static gboolean get_widgets			(EDelegateDialog *edd);
static void addressbook_clicked_cb              (GtkWidget *widget, gpointer data);

static GtkObjectClass *parent_class;


GtkType
e_delegate_dialog_get_type (void)
{
	static GtkType e_delegate_dialog_type = 0;

	if (!e_delegate_dialog_type) {
		static const GtkTypeInfo e_delegate_dialog_info = {
			"EDelegateDialog",
			sizeof (EDelegateDialog),
			sizeof (EDelegateDialogClass),
			(GtkClassInitFunc) e_delegate_dialog_class_init,
			(GtkObjectInitFunc) e_delegate_dialog_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		e_delegate_dialog_type = gtk_type_unique (GTK_TYPE_OBJECT,
							  &e_delegate_dialog_info);
	}

	return e_delegate_dialog_type;
}

/* Class initialization function for the event editor */
static void
e_delegate_dialog_class_init (EDelegateDialogClass *class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) class;

	parent_class = gtk_type_class (GTK_TYPE_OBJECT);

	object_class->destroy = e_delegate_dialog_destroy;
}

/* Object initialization function for the event editor */
static void
e_delegate_dialog_init (EDelegateDialog *edd)
{
	EDelegateDialogPrivate *priv;

	priv = g_new0 (EDelegateDialogPrivate, 1);
	edd->priv = priv;

	priv->address = NULL;
}

/* Destroy handler for the event editor */
static void
e_delegate_dialog_destroy (GtkObject *object)
{
	EDelegateDialog *edd;
	EDelegateDialogPrivate *priv;
	GtkWidget *dialog;

	g_return_if_fail (object != NULL);
	g_return_if_fail (E_IS_DELEGATE_DIALOG (object));

	edd = E_DELEGATE_DIALOG (object);
	priv = edd->priv;

	/* Destroy the actual dialog. */
	dialog = e_delegate_dialog_get_toplevel (edd);
	gtk_widget_destroy (dialog);

	g_free (priv->address);
	priv->address = NULL;

	g_free (priv);
	edd->priv = NULL;

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


EDelegateDialog *
e_delegate_dialog_construct (EDelegateDialog *edd, const char *name, const char *address)
{
	EDelegateDialogPrivate *priv;
	EDestination *dest;
	EDestination *destv[2] = {NULL, NULL};
	Bonobo_Control corba_control;
	CORBA_Environment ev;

	g_return_val_if_fail (edd != NULL, NULL);
	g_return_val_if_fail (E_IS_DELEGATE_DIALOG (edd), NULL);

	priv = edd->priv;

	/* Load the content widgets */

	priv->xml = glade_xml_new (EVOLUTION_GLADEDIR "/e-delegate-dialog.glade",
				   NULL);
	if (!priv->xml) {
		g_message ("e_delegate_dialog_construct(): Could not load the Glade XML file!");
		goto error;
	}

	if (!get_widgets (edd)) {
		g_message ("e_delegate_dialog_construct(): Could not find all widgets in the XML file!");
		goto error;
	}
	
	CORBA_exception_init (&ev);
	
	priv->corba_select_names = oaf_activate_from_id (SELECT_NAMES_OAFID, 0, NULL, &ev);
	GNOME_Evolution_Addressbook_SelectNames_addSectionWithLimit (priv->corba_select_names, 
								     section_name, 
								     section_name,
								     1, &ev);

	if (BONOBO_EX (&ev)) {
		g_message ("e_delegate_dialog_construct(): Unable to add section!");
		goto error;
	}

	corba_control = GNOME_Evolution_Addressbook_SelectNames_getEntryBySection (priv->corba_select_names, 
										   section_name, &ev);

	if (BONOBO_EX (&ev)) {
		g_message ("e_delegate_dialog_construct(): Unable to get addressbook entry!");
		goto error;
	}
	
	CORBA_exception_free (&ev);

	priv->entry = bonobo_widget_new_control_from_objref (corba_control, CORBA_OBJECT_NIL);
	gtk_widget_show (priv->entry);
	gtk_box_pack_start (GTK_BOX (priv->hbox), priv->entry, TRUE, TRUE, 0);

	dest = e_destination_new ();
	destv[0] = dest;
	if (name != NULL && *name)
		e_destination_set_name (dest, name);
	if (address != NULL && *address)
		e_destination_set_email (dest, address);
	bonobo_widget_set_property (BONOBO_WIDGET (priv->entry), "destinations", e_destination_exportv (destv), NULL);
	gtk_object_unref (GTK_OBJECT (dest));
		
	gtk_signal_connect (GTK_OBJECT (priv->addressbook), "clicked",
			    GTK_SIGNAL_FUNC (addressbook_clicked_cb), edd);

	return edd;

 error:

	gtk_object_unref (GTK_OBJECT (edd));
	return NULL;
}

static gboolean
get_widgets (EDelegateDialog *edd)
{
	EDelegateDialogPrivate *priv;

	priv = edd->priv;

#define GW(name) glade_xml_get_widget (priv->xml, name)

	priv->app		= GW ("delegate-dialog");
	priv->hbox              = GW ("delegate-hbox");
	priv->addressbook	= GW ("addressbook");	

	return (priv->app
		&& priv->hbox
		&& priv->addressbook);
}

static void
addressbook_clicked_cb (GtkWidget *widget, gpointer data)
{
	EDelegateDialog *edd = data;
	EDelegateDialogPrivate *priv;
	CORBA_Environment ev;
	
	priv = edd->priv;

	CORBA_exception_init (&ev);

	GNOME_Evolution_Addressbook_SelectNames_activateDialog (priv->corba_select_names, section_name, &ev);
	
	CORBA_exception_free (&ev);
}


/**
 * e_delegate_dialog_new:
 *
 * Creates a new event editor dialog.
 *
 * Return value: A newly-created event editor dialog, or NULL if the event
 * editor could not be created.
 **/
EDelegateDialog *
e_delegate_dialog_new (const char *name, const char *address)
{
	EDelegateDialog *edd;

	edd = E_DELEGATE_DIALOG (gtk_type_new (E_TYPE_DELEGATE_DIALOG));
	return e_delegate_dialog_construct (E_DELEGATE_DIALOG (edd), name, address);
}

char *
e_delegate_dialog_get_delegate		(EDelegateDialog  *edd)
{
	EDelegateDialogPrivate *priv;
	EDestination **destv;
	char *string = NULL;
	
	g_return_val_if_fail (edd != NULL, NULL);
	g_return_val_if_fail (E_IS_DELEGATE_DIALOG (edd), NULL);

	priv = edd->priv;
	
	bonobo_widget_get_property (BONOBO_WIDGET (priv->entry), "destinations", &string, NULL);
	destv = e_destination_importv (string);
	
	if (destv && destv[0] != NULL) {
		g_free (priv->address);
		priv->address = g_strdup (e_destination_get_email (destv[0]));
		g_free (destv);
	}
	
	g_free (string);
	
	return g_strdup (priv->address);
}


char *
e_delegate_dialog_get_delegate_name		(EDelegateDialog  *edd)
{
	EDelegateDialogPrivate *priv;
	EDestination **destv;
	char *string = NULL;
	
	g_return_val_if_fail (edd != NULL, NULL);
	g_return_val_if_fail (E_IS_DELEGATE_DIALOG (edd), NULL);

	priv = edd->priv;

	bonobo_widget_get_property (BONOBO_WIDGET (priv->entry), "destinations", &string, NULL);
	destv = e_destination_importv (string);
	
	g_message ("importv: [%s]", string);
	
	if (destv && destv[0] != NULL) {
		g_free (priv->name);
		priv->name = g_strdup (e_destination_get_name (destv[0]));
		g_free (destv);
	}
	
	g_free (string);
	
	return g_strdup (priv->name);
}

GtkWidget*
e_delegate_dialog_get_toplevel	(EDelegateDialog  *edd)
{
	EDelegateDialogPrivate *priv;

	g_return_val_if_fail (edd != NULL, NULL);
	g_return_val_if_fail (E_IS_DELEGATE_DIALOG (edd), NULL);

	priv = edd->priv;

	return priv->app;
}

