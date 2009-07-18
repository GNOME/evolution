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

#include "gal-define-views-dialog.h"
#include "gal-define-views-model.h"
#include "gal-view-new-dialog.h"

static void gal_define_views_dialog_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void gal_define_views_dialog_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static void gal_define_views_dialog_dispose	 (GObject *object);

/* The properties we support */
enum {
	PROP_0,
	PROP_COLLECTION
};

enum {
	COL_GALVIEW_NAME,
	COL_GALVIEW_DATA
};

typedef struct {
	gchar         *title;

	GtkTreeView *treeview;
	GtkTreeModel *model;

	GalDefineViewsDialog *names;
} GalDefineViewsDialogChild;

G_DEFINE_TYPE (GalDefineViewsDialog, gal_define_views_dialog, GTK_TYPE_DIALOG)

static void
gal_define_views_dialog_class_init (GalDefineViewsDialogClass *klass)
{
	GObjectClass *object_class;

	object_class = (GObjectClass*) klass;

	object_class->set_property = gal_define_views_dialog_set_property;
	object_class->get_property = gal_define_views_dialog_get_property;
	object_class->dispose = gal_define_views_dialog_dispose;

	g_object_class_install_property (object_class, PROP_COLLECTION,
					 g_param_spec_object ("collection",
							      _("Collection"),
							      /*_( */"XXX blurb" /*)*/,
							      GAL_VIEW_COLLECTION_TYPE,
							      G_PARAM_READWRITE));
}

/* Button callbacks */

static void
gdvd_button_new_dialog_callback (GtkWidget *widget, gint id, GalDefineViewsDialog *dialog)
{
	gchar *name;
	GtkTreeIter iter;
	GalView *view;
	GalViewCollectionItem *item;
	GalViewFactory *factory;

	switch (id) {
	case GTK_RESPONSE_OK:
		g_object_get (widget,
			     "name", &name,
			     "factory", &factory,
			     NULL);

		if (name && factory) {
			g_strchomp(name);
			if (*name != '\0') {
				view = gal_view_factory_new_view (factory, name);
				gal_view_collection_append(dialog->collection, view);

				item = dialog->collection->view_data[dialog->collection->view_count-1];
				gtk_list_store_append (GTK_LIST_STORE (dialog->model), &iter);
				gtk_list_store_set (GTK_LIST_STORE (dialog->model), &iter,
						    COL_GALVIEW_NAME, name,
						    COL_GALVIEW_DATA, item,
						    -1);

				gal_view_edit (view, GTK_WINDOW (dialog));
				g_object_unref (view);
			}
		}
		g_object_unref (factory);
		g_free (name);
		break;
	}
	gtk_widget_destroy (widget);
}

static void
gdvd_button_new_callback(GtkWidget *widget, GalDefineViewsDialog *dialog)
{
	GtkWidget *view_new_dialog = gal_view_new_dialog_new (dialog->collection);
	gtk_window_set_transient_for (GTK_WINDOW (view_new_dialog), GTK_WINDOW (dialog));
	g_signal_connect (view_new_dialog, "response",
			 G_CALLBACK (gdvd_button_new_dialog_callback), dialog);
	gtk_widget_show (view_new_dialog);
}

static void
gdvd_button_modify_callback(GtkWidget *widget, GalDefineViewsDialog *dialog)
{
	GtkTreeIter iter;
	GalViewCollectionItem *item;

	if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (dialog->treeview),
					 &dialog->model,
					 &iter)) {
		gtk_tree_model_get (dialog->model, &iter, COL_GALVIEW_DATA, &item, -1);
		gal_view_edit (item->view, GTK_WINDOW (dialog));
	}
}

static void
gdvd_button_delete_callback(GtkWidget *widget, GalDefineViewsDialog *dialog)
{
	gint row;
	GtkTreeIter iter;
	GtkTreePath *path;
	GtkTreeSelection *selection;
	GalViewCollectionItem *item;

	selection = gtk_tree_view_get_selection (dialog->treeview);

	if (gtk_tree_selection_get_selected (selection,
					 &dialog->model,
					 &iter)) {
		gtk_tree_model_get (dialog->model, &iter, COL_GALVIEW_DATA, &item, -1);

		for (row=0; row<dialog->collection->view_count; row++) {
			if (item == dialog->collection->view_data[row]) {
				gal_view_collection_delete_view (dialog->collection, row);
				path = gtk_tree_model_get_path (dialog->model, &iter);
				gtk_list_store_remove (GTK_LIST_STORE (dialog->model), &iter);

				if (gtk_tree_path_prev (path)) {
					gtk_tree_model_get_iter (dialog->model, &iter, path);
				} else {
					gtk_tree_model_get_iter_first (dialog->model, &iter);
				}

				gtk_tree_selection_select_iter (selection, &iter);
				break;
			}
		}
	}
}

#if 0
static void
gdvd_button_copy_callback(GtkWidget *widget, GalDefineViewsDialog *dialog)
{
	gint row;
	GtkWidget *scrolled;
	ETable *etable;

	scrolled = glade_xml_get_widget(dialog->gui, "custom-table");
	etable = e_table_scrolled_get_table(E_TABLE_SCROLLED(scrolled));
	row = e_table_get_cursor_row (E_TABLE(etable));

	if (row != -1) {
		gal_define_views_model_copy_view(GAL_DEFINE_VIEWS_MODEL(dialog->model),
						 row);
	}

}
#endif

static void
gdvd_cursor_changed_callback (GtkWidget *widget, GalDefineViewsDialog *dialog)
{
	GtkWidget *button;
	GtkTreeIter iter;
	GalViewCollectionItem *item;

	if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (dialog->treeview),
					 &dialog->model,
					 &iter)) {
		gtk_tree_model_get (dialog->model, &iter, COL_GALVIEW_DATA, &item, -1);

		button = glade_xml_get_widget (dialog->gui, "button-delete");
		gtk_widget_set_sensitive (GTK_WIDGET (button), !item->built_in);

		button = glade_xml_get_widget (dialog->gui, "button-modify");
		gtk_widget_set_sensitive (GTK_WIDGET (button), !item->built_in);
	}
}

static void
gdvd_connect_signal(GalDefineViewsDialog *dialog, const gchar *widget_name, const gchar *signal, GCallback handler)
{
	GtkWidget *widget;

	widget = glade_xml_get_widget (dialog->gui, widget_name);

	if (widget)
		g_signal_connect (widget, signal, handler, dialog);
}

static void
dialog_response (GalDefineViewsDialog *dialog, gint response_id, gpointer data)
{
	gal_view_collection_save (dialog->collection);
}

static void
gal_define_views_dialog_init (GalDefineViewsDialog *dialog)
{
	GladeXML *gui;
	GtkWidget *widget;

	gchar *filename = g_build_filename (EVOLUTION_GLADEDIR,
					    "gal-define-views.glade",
					    NULL);

	dialog->collection = NULL;

	gui = glade_xml_new (filename, NULL, GETTEXT_PACKAGE);
	g_free (filename);
	dialog->gui = gui;

	widget = glade_xml_get_widget (gui, "table-top");
	if (!widget) {
		return;
	}

	g_object_ref (widget);
	gtk_container_remove (GTK_CONTAINER (widget->parent), widget);
	gtk_window_set_default_size (GTK_WINDOW (dialog), 360, 270);
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 6);
	gtk_container_set_border_width (GTK_CONTAINER (widget), 6);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), widget, TRUE, TRUE, 0);
	g_object_unref (widget);

	gtk_dialog_add_buttons (GTK_DIALOG (dialog),
				GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
				NULL);

	dialog->treeview = GTK_TREE_VIEW (glade_xml_get_widget (dialog->gui, "treeview1"));
        gtk_tree_view_set_reorderable (GTK_TREE_VIEW (dialog->treeview), FALSE);
	gtk_tree_view_set_headers_visible (dialog->treeview, TRUE);

	gtk_window_set_resizable (GTK_WINDOW (dialog), TRUE);

	gdvd_connect_signal (dialog, "button-new",    "clicked", G_CALLBACK (gdvd_button_new_callback));
	gdvd_connect_signal (dialog, "button-modify", "clicked", G_CALLBACK (gdvd_button_modify_callback));
	gdvd_connect_signal (dialog, "button-delete", "clicked", G_CALLBACK (gdvd_button_delete_callback));
#if 0
	gdvd_connect_signal (dialog, "button-copy",   "clicked", G_CALLBACK (gdvd_button_copy_callback));
#endif
	gdvd_connect_signal (dialog, "treeview1", "cursor-changed", G_CALLBACK (gdvd_cursor_changed_callback));
	g_signal_connect (dialog, "response", G_CALLBACK (dialog_response), NULL);

	gtk_widget_show (GTK_WIDGET (dialog));
}

static void
gal_define_views_dialog_dispose (GObject *object)
{
	GalDefineViewsDialog *gal_define_views_dialog = GAL_DEFINE_VIEWS_DIALOG(object);

	if (gal_define_views_dialog->gui)
		g_object_unref(gal_define_views_dialog->gui);
	gal_define_views_dialog->gui = NULL;

	if (G_OBJECT_CLASS (gal_define_views_dialog_parent_class)->dispose)
		(* G_OBJECT_CLASS (gal_define_views_dialog_parent_class)->dispose) (object);
}

static void
gal_define_views_dialog_set_collection(GalDefineViewsDialog *dialog,
				       GalViewCollection *collection)
{
	gint i;
	GtkListStore *store;
	GtkCellRenderer *renderer;
	dialog->collection = collection;

	store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_POINTER);

	for (i=0; i<collection->view_count; i++) {
		GalViewCollectionItem *item = collection->view_data[i];
		GtkTreeIter iter;

		/* hide built in views */
		/*if (item->built_in == 1)
			continue;*/

		gchar *title = NULL;
		title = e_str_without_underscores (item->title);

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
				    COL_GALVIEW_NAME, title,
				    COL_GALVIEW_DATA, item,
				    -1);

		g_free (title);
	}

	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
					      COL_GALVIEW_NAME, GTK_SORT_ASCENDING);

	/* attaching treeview to model */
	gtk_tree_view_set_model (dialog->treeview, GTK_TREE_MODEL (store));
	gtk_tree_view_set_search_column (dialog->treeview, COL_GALVIEW_NAME);

	dialog->model = GTK_TREE_MODEL (store);

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (dialog->treeview,
						    COL_GALVIEW_NAME, _("Name"),
						    renderer, "text", COL_GALVIEW_NAME,
						    NULL);

	/* set sort column */
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (dialog->model),
					      COL_GALVIEW_NAME, GTK_SORT_ASCENDING);

	if (dialog->gui) {
		GtkWidget *widget = glade_xml_get_widget(dialog->gui, "label-views");
		if (widget && GTK_IS_LABEL (widget)) {
			if (collection->title) {
				gchar *text = g_strdup_printf (_("Define Views for %s"),
							      collection->title);
				gtk_label_set_text (GTK_LABEL (widget),
						    text);
				gtk_window_set_title (GTK_WINDOW (dialog), text);
				g_free (text);
			} else {
				gtk_label_set_text (GTK_LABEL (widget),
						    _("Define Views"));
				gtk_window_set_title (GTK_WINDOW (dialog),
						      _("Define Views"));
			}
		}
	}
}

/**
 * gal_define_views_dialog_new
 *
 * Returns a new dialog for defining views.
 *
 * Returns: The GalDefineViewsDialog.
 */
GtkWidget*
gal_define_views_dialog_new (GalViewCollection *collection)
{
	GtkWidget *widget = g_object_new (GAL_DEFINE_VIEWS_DIALOG_TYPE, NULL);
	gal_define_views_dialog_set_collection (GAL_DEFINE_VIEWS_DIALOG (widget), collection);
	return widget;
}

static void
gal_define_views_dialog_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	GalDefineViewsDialog *dialog;

	dialog = GAL_DEFINE_VIEWS_DIALOG (object);

	switch (prop_id) {
	case PROP_COLLECTION:
		if (g_value_get_object (value))
			gal_define_views_dialog_set_collection(dialog, GAL_VIEW_COLLECTION(g_value_get_object (value)));
		else
			gal_define_views_dialog_set_collection(dialog, NULL);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		return;
	}
}

static void
gal_define_views_dialog_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GalDefineViewsDialog *dialog;

	dialog = GAL_DEFINE_VIEWS_DIALOG (object);

	switch (prop_id) {
	case PROP_COLLECTION:
		g_value_set_object (value, dialog->collection);

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}
