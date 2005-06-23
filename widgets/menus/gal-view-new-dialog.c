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

#include <gtk/gtk.h>

#include "table/e-table-scrolled.h"
#include "e-util/e-i18n.h"
#include "e-util/e-util.h"
#include "e-util/e-util-private.h"
#include "misc/e-unicode.h"

#include "gal-define-views-model.h"
#include "gal-view-new-dialog.h"

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

	g_object_class_install_property (object_class, PROP_NAME, 
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
	gchar *filename = g_build_filename (EVOLUTION_GLADEDIR,
					    "gal-view-new-dialog.glade",
					    NULL);

	gui = glade_xml_new (filename, NULL, E_I18N_DOMAIN);
	g_free (filename);
	dialog->gui = gui;

	widget = glade_xml_get_widget(gui, "table-top");
	if (!widget) {
		return;
	}
	gtk_widget_ref(widget);
	gtk_container_remove (GTK_CONTAINER (widget->parent), widget);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), widget, TRUE, TRUE, 0);
	gtk_widget_unref(widget);

	gtk_dialog_add_buttons (GTK_DIALOG (dialog),
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				GTK_STOCK_OK, GTK_RESPONSE_OK,
				NULL);

	gtk_window_set_policy(GTK_WINDOW(dialog), FALSE, TRUE, FALSE);
	gtk_window_set_modal (GTK_WINDOW(dialog), TRUE);
	gtk_window_set_title (GTK_WINDOW(dialog), _("Define New View"));

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
sensitize_ok_response (GalViewNewDialog *dialog)
{
	gboolean ok = TRUE;
	const char *text;
	
	text = gtk_entry_get_text (GTK_ENTRY (dialog->entry));
	if (!text || !text[0])
		ok = FALSE;

	if (!dialog->selected_factory)
		ok = FALSE;

	gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_OK, ok);
}

static gboolean
selection_func (GtkTreeSelection  *selection,
		GtkTreeModel      *model,
		GtkTreePath       *path,
		gboolean           path_currently_selected,
		gpointer           data)
{
	GtkTreeIter iter;
	GalViewNewDialog *dialog = data;

	if (path_currently_selected)
		return TRUE;

	gtk_tree_model_get_iter (GTK_TREE_MODEL (dialog->list_store),
				 &iter,
				 (GtkTreePath*)path);

	gtk_tree_model_get (GTK_TREE_MODEL (dialog->list_store),
			    &iter,
			    1, &dialog->selected_factory,
			    -1);

	printf ("%s factory selected\n", gal_view_factory_get_title(dialog->selected_factory));

	sensitize_ok_response (dialog);

	return TRUE;
}

static void
entry_changed (GtkWidget *entry, gpointer data)
{
	GalViewNewDialog *dialog = data;

	sensitize_ok_response (dialog);
}

GtkWidget*
gal_view_new_dialog_construct (GalViewNewDialog  *dialog,
			       GalViewCollection *collection)
{
	GList *iterator;
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;
	GtkCellRenderer *rend;

	dialog->collection = collection;
	dialog->list = glade_xml_get_widget(dialog->gui,"list-type-list");
	dialog->entry = glade_xml_get_widget(dialog->gui, "entry-name");
	dialog->list_store = gtk_list_store_new (2,
						 G_TYPE_STRING,
						 G_TYPE_POINTER);

	rend = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ("factory title",
							   rend,
							   "text", 0,
							   NULL);

	gtk_tree_view_append_column (GTK_TREE_VIEW (dialog->list), column);

	iterator = dialog->collection->factory_list;
	for ( ; iterator; iterator = g_list_next(iterator) ) {
		GalViewFactory *factory = iterator->data;
		GtkTreeIter iter;

		g_object_ref(factory);
		gtk_list_store_append (dialog->list_store,
				       &iter);
		gtk_list_store_set (dialog->list_store,
				    &iter,
				    0, gal_view_factory_get_title(factory),
				    1, factory,
				    -1);
	}

	gtk_tree_view_set_model (GTK_TREE_VIEW (dialog->list), GTK_TREE_MODEL (dialog->list_store));

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->list));
	gtk_tree_selection_set_select_function (selection, selection_func, dialog, NULL);

	g_signal_connect (dialog->entry, "changed",
			  G_CALLBACK (entry_changed), dialog);

	sensitize_ok_response (dialog);

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
