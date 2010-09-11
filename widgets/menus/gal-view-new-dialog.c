/*
 *
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
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <config.h>

#include <glib/gi18n.h>
#include "e-util/e-util.h"
#include "e-util/e-util-private.h"
#include "e-util/e-unicode.h"

#include "gal-define-views-model.h"
#include "gal-view-new-dialog.h"

static void gal_view_new_dialog_set_property	(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void gal_view_new_dialog_get_property	(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static void gal_view_new_dialog_dispose		(GObject *object);

/* The arguments we take */
enum {
	PROP_0,
	PROP_NAME,
	PROP_FACTORY
};

G_DEFINE_TYPE (GalViewNewDialog, gal_view_new_dialog, GTK_TYPE_DIALOG)

static void
gal_view_new_dialog_class_init (GalViewNewDialogClass *klass)
{
	GObjectClass *object_class;

	object_class = (GObjectClass*) klass;

	object_class->set_property = gal_view_new_dialog_set_property;
	object_class->get_property = gal_view_new_dialog_get_property;
	object_class->dispose      = gal_view_new_dialog_dispose;

	g_object_class_install_property (object_class, PROP_NAME,
					 g_param_spec_string ("name",
							      "Name",
							      NULL,
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_FACTORY,
					 g_param_spec_object ("factory",
							      "Factory",
							      NULL,
							      GAL_TYPE_VIEW_FACTORY,
							      G_PARAM_READWRITE));
}

static void
gal_view_new_dialog_init (GalViewNewDialog *dialog)
{
	GtkWidget *content_area;
	GtkWidget *parent;
	GtkWidget *widget;

	dialog->builder = gtk_builder_new ();
	e_load_ui_builder_definition (
		dialog->builder, "gal-view-new-dialog.ui");

	widget = e_builder_get_widget (dialog->builder, "table-top");
	if (!widget) {
		return;
	}

	g_object_ref (widget);

	parent = gtk_widget_get_parent (widget);
	gtk_container_remove (GTK_CONTAINER (parent), widget);

	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	gtk_box_pack_start (GTK_BOX (content_area), widget, TRUE, TRUE, 0);

	g_object_unref (widget);

	gtk_dialog_add_buttons (
		GTK_DIALOG (dialog),
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);

	gtk_window_set_resizable (GTK_WINDOW (dialog), TRUE);
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	gtk_window_set_title (GTK_WINDOW(dialog), _("Define New View"));

	dialog->collection = NULL;
	dialog->selected_factory = NULL;
}

static void
gal_view_new_dialog_dispose (GObject *object)
{
	GalViewNewDialog *gal_view_new_dialog = GAL_VIEW_NEW_DIALOG (object);

	if (gal_view_new_dialog->builder)
		g_object_unref (gal_view_new_dialog->builder);
	gal_view_new_dialog->builder = NULL;

	if (G_OBJECT_CLASS (gal_view_new_dialog_parent_class)->dispose)
		(* G_OBJECT_CLASS (gal_view_new_dialog_parent_class)->dispose) (object);
}

GtkWidget*
gal_view_new_dialog_new (GalViewCollection *collection)
{
	GtkWidget *widget =
		gal_view_new_dialog_construct (g_object_new (GAL_VIEW_NEW_DIALOG_TYPE, NULL),
					      collection);
	return widget;
}

static void
sensitize_ok_response (GalViewNewDialog *dialog)
{
	gboolean ok = TRUE;
	const gchar *text;

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
	dialog->list = e_builder_get_widget(dialog->builder,"list-type-list");
	dialog->entry = e_builder_get_widget(dialog->builder, "entry-name");
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
	for (; iterator; iterator = g_list_next (iterator) ) {
		GalViewFactory *factory = iterator->data;
		GtkTreeIter iter;

		g_object_ref (factory);
		gtk_list_store_append (dialog->list_store,
				       &iter);
		gtk_list_store_set (dialog->list_store,
				    &iter,
				    0, gal_view_factory_get_title (factory),
				    1, factory,
				    -1);
	}

	gtk_tree_view_set_model (GTK_TREE_VIEW (dialog->list), GTK_TREE_MODEL (dialog->list_store));

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->list));
	gtk_tree_selection_set_select_function (selection, selection_func, dialog, NULL);

	g_signal_connect (dialog->entry, "changed",
			  G_CALLBACK (entry_changed), dialog);

	sensitize_ok_response (dialog);

	return GTK_WIDGET (dialog);
}

static void
gal_view_new_dialog_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	GalViewNewDialog *dialog;
	GtkWidget *entry;

	dialog = GAL_VIEW_NEW_DIALOG (object);

	switch (prop_id) {
	case PROP_NAME:
		entry = e_builder_get_widget(dialog->builder, "entry-name");
		if (entry && GTK_IS_ENTRY (entry)) {
			gtk_entry_set_text (GTK_ENTRY (entry), g_value_get_string (value));
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
		entry = e_builder_get_widget(dialog->builder, "entry-name");
		if (entry && GTK_IS_ENTRY (entry)) {
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
