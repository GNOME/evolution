/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * gal-view-new-dialog.c
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <config.h>
#include <libgnomeui/gnome-dialog.h>
#include <gtk/gtk.h>
#include "gal-view-new-dialog.h"
#include "gal-define-views-model.h"
#include <gal/widgets/e-unicode.h>
#include <gal/e-table/e-table-scrolled.h>
#include <gal/util/e-i18n.h>
#include <gal/util/e-util.h>

static void gal_view_new_dialog_init		(GalViewNewDialog		 *card);
static void gal_view_new_dialog_class_init	(GalViewNewDialogClass	 *klass);
static void gal_view_new_dialog_set_property	(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void gal_view_new_dialog_get_property	(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static void gal_view_new_dialog_dispose		(GObject *object);

static GtkDialogClass *parent_class = NULL;
#define PARENT_TYPE GTK_TYPE_DIALOG

/* The arguments we take */
enum {
	PROP_0,
	PROP_NAME,
	PROP_FACTORY
};

E_MAKE_TYPE(gal_view_new_dialog, "GalViewNewDialog",
	    GalViewNewDialog,
	    gal_view_new_dialog_class_init,
	    gal_view_new_dialog_init, PARENT_TYPE)

static void
gal_view_new_dialog_class_init (GalViewNewDialogClass *klass)
{
	GObjectClass *object_class;

	object_class = (GObjectClass*) klass;

	parent_class = g_type_class_ref (PARENT_TYPE);

	object_class->set_property = gal_view_new_dialog_set_property;
	object_class->get_property = gal_view_new_dialog_get_property;
	object_class->dispose      = gal_view_new_dialog_dispose;

	g_object_class_install_property (object_class, PROP_FACTORY, 
					 g_param_spec_string ("name",
							      _("Name"),
							      /*_( */"XXX blurb" /*)*/,
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_FACTORY, 
					 g_param_spec_object ("factory",
							      _("Factory"),
							      /*_( */"XXX blurb" /*)*/,
							      GAL_VIEW_FACTORY_TYPE,
							      G_PARAM_READWRITE));
}

static void
gal_view_new_dialog_init (GalViewNewDialog *dialog)
{
	GladeXML *gui;
	GtkWidget *widget;

	gui = glade_xml_new (GAL_GLADEDIR "/gal-view-new-dialog.glade", NULL, PACKAGE);
	dialog->gui = gui;

	widget = glade_xml_get_widget(gui, "table-top");
	if (!widget) {
		return;
	}
	gtk_widget_ref(widget);
	gtk_widget_unparent(widget);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), widget, TRUE, TRUE, 0);
	gtk_widget_unref(widget);

	gtk_dialog_add_buttons (GTK_DIALOG (dialog),
				GTK_STOCK_OK, GTK_RESPONSE_OK,
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				NULL);

	gtk_window_set_policy(GTK_WINDOW(dialog), FALSE, TRUE, FALSE);

	dialog->collection = NULL;
	dialog->selected_factory = NULL;
}

static void
gal_view_new_dialog_dispose (GObject *object)
{
	GalViewNewDialog *gal_view_new_dialog = GAL_VIEW_NEW_DIALOG(object);
	
	if (gal_view_new_dialog->gui)
		g_object_unref(gal_view_new_dialog->gui);
	gal_view_new_dialog->gui = NULL;

	if (G_OBJECT_CLASS (parent_class)->dispose)
		(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

GtkWidget*
gal_view_new_dialog_new (GalViewCollection *collection)
{
	GtkWidget *widget =
		gal_view_new_dialog_construct(g_object_new (GAL_VIEW_NEW_DIALOG_TYPE, NULL),
					      collection);
	return widget;
}


static void
gal_view_new_dialog_select_row_callback(GtkCList *list,
					gint row,
					gint column,
					GdkEventButton *event,
					GalViewNewDialog *dialog)
{
	dialog->selected_factory = gtk_clist_get_row_data(list,
							  row);
}

GtkWidget*
gal_view_new_dialog_construct (GalViewNewDialog  *dialog,
			       GalViewCollection *collection)
{
	GtkWidget *list = glade_xml_get_widget(dialog->gui,
					       "clist-type-list");
	GList *iterator;
	dialog->collection = collection;

	iterator = dialog->collection->factory_list;

	for ( ; iterator; iterator = g_list_next(iterator) ) {
		GalViewFactory *factory = iterator->data;
		char *text[1];
		int row;

		g_object_ref(factory);
		text[0] = (char *) gal_view_factory_get_title(factory);
		row = gtk_clist_append(GTK_CLIST(list), text);
		gtk_clist_set_row_data(GTK_CLIST(list), row, factory);
	}

	g_signal_connect(list,
			 "select_row",
			 G_CALLBACK(gal_view_new_dialog_select_row_callback),
			 dialog);

	return GTK_WIDGET(dialog);
}

static void
gal_view_new_dialog_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	GalViewNewDialog *dialog;
	GtkWidget *entry;

	dialog = GAL_VIEW_NEW_DIALOG (object);
	
	switch (prop_id){
	case PROP_NAME:
		entry = glade_xml_get_widget(dialog->gui, "entry-name");
		if (entry && GTK_IS_ENTRY(entry)) {
			gtk_entry_set_text(GTK_ENTRY(entry), g_value_get_string (value));
		}
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		return;
	}
}


static void
gal_view_new_dialog_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GalViewNewDialog *dialog;
	GtkWidget *entry;

	dialog = GAL_VIEW_NEW_DIALOG (object);

	switch (prop_id) {
	case PROP_NAME:
		entry = glade_xml_get_widget(dialog->gui, "entry-name");
		if (entry && GTK_IS_ENTRY(entry)) {
			g_value_set_string (value, gtk_entry_get_text (GTK_ENTRY (entry)));
		}
		break;
	case PROP_FACTORY:
		g_value_set_object (value, dialog->selected_factory);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}
