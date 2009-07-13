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
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <config.h>

#include <glib/gi18n.h>
#include "e-util/e-util.h"
#include "e-util/e-util-private.h"

#include "gal-define-views-model.h"
#include "gal-view-instance-save-as-dialog.h"
#include "gal-view-new-dialog.h"

G_DEFINE_TYPE (GalViewInstanceSaveAsDialog, gal_view_instance_save_as_dialog, GTK_TYPE_DIALOG)

/* The arguments we take */
enum {
	PROP_0,
	PROP_INSTANCE
};

enum {
	COL_GALVIEW_NAME,
	COL_GALVIEW_DATA
};

/* Static functions */
static void
gal_view_instance_save_as_dialog_set_instance (GalViewInstanceSaveAsDialog *dialog,
					       GalViewInstance *instance)
{
	gint i;
	GtkListStore *store;
	GtkCellRenderer *renderer;
	dialog->instance = instance;

	store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_POINTER);

	for (i=0; i<instance->collection->view_count; i++) {
		GalViewCollectionItem *item = instance->collection->view_data[i];
		GtkTreeIter iter;
		gchar *title = NULL;

		/* hide built in views */
		/*if (item->built_in == 1)
			continue;*/

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
}

static void
gvisad_setup_validate_button (GalViewInstanceSaveAsDialog *dialog)
{
	if ((dialog->toggle == GAL_VIEW_INSTANCE_SAVE_AS_DIALOG_TOGGLE_CREATE
	      && g_utf8_strlen (gtk_entry_get_text (GTK_ENTRY (dialog->entry_create)), -1) > 0)
	    || dialog->toggle == GAL_VIEW_INSTANCE_SAVE_AS_DIALOG_TOGGLE_REPLACE) {
		gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_OK, TRUE);
	} else {
		gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_OK, FALSE);
	}
}

static void
gvisad_setup_radio_buttons (GalViewInstanceSaveAsDialog *dialog)
{
	GtkWidget        *widget;

	widget = dialog->scrolledwindow;
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->radiobutton_replace))) {
		GtkTreeIter       iter;
		GtkTreeSelection *selection;

		selection = gtk_tree_view_get_selection (dialog->treeview);
		if (!gtk_tree_selection_get_selected (selection, &dialog->model, &iter)) {
			if (gtk_tree_model_get_iter_first (dialog->model, &iter)) {
				gtk_tree_selection_select_iter (selection, &iter);
			}
		}

		gtk_widget_set_sensitive (widget, TRUE);
		dialog->toggle = GAL_VIEW_INSTANCE_SAVE_AS_DIALOG_TOGGLE_REPLACE;
	} else {
		gtk_widget_set_sensitive (widget, FALSE);
	}

	widget = dialog->entry_create;
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->radiobutton_create))) {
		gtk_widget_set_sensitive (widget, TRUE);
		dialog->toggle = GAL_VIEW_INSTANCE_SAVE_AS_DIALOG_TOGGLE_CREATE;
	} else {
		gtk_widget_set_sensitive (widget, FALSE);
	}

	gvisad_setup_validate_button (dialog);
}

static void
gvisad_radio_toggled (GtkWidget *widget, GalViewInstanceSaveAsDialog *dialog)
{
	gvisad_setup_radio_buttons (dialog);
}

static void
gvisad_entry_changed (GtkWidget *widget, GalViewInstanceSaveAsDialog *dialog)
{
	gvisad_setup_validate_button (dialog);
}

/* Method override implementations */
static void
gal_view_instance_save_as_dialog_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	GalViewInstanceSaveAsDialog *dialog;

	dialog = GAL_VIEW_INSTANCE_SAVE_AS_DIALOG (object);

	switch (prop_id) {
	case PROP_INSTANCE:
		if (g_value_get_object (value))
			gal_view_instance_save_as_dialog_set_instance (dialog, GAL_VIEW_INSTANCE (g_value_get_object (value)));
		else
			gal_view_instance_save_as_dialog_set_instance (dialog, NULL);
		break;

	default:
		return;
	}
}

static void
gal_view_instance_save_as_dialog_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GalViewInstanceSaveAsDialog *dialog;

	dialog = GAL_VIEW_INSTANCE_SAVE_AS_DIALOG (object);

	switch (prop_id) {
	case PROP_INSTANCE:
		g_value_set_object (value, dialog->instance);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gal_view_instance_save_as_dialog_dispose (GObject *object)
{
	GalViewInstanceSaveAsDialog *gal_view_instance_save_as_dialog = GAL_VIEW_INSTANCE_SAVE_AS_DIALOG (object);

	if (gal_view_instance_save_as_dialog->gui)
		g_object_unref (gal_view_instance_save_as_dialog->gui);
	gal_view_instance_save_as_dialog->gui = NULL;

	if (G_OBJECT_CLASS (gal_view_instance_save_as_dialog_parent_class)->dispose)
		(* G_OBJECT_CLASS (gal_view_instance_save_as_dialog_parent_class)->dispose) (object);
}

/* Init functions */
static void
gal_view_instance_save_as_dialog_class_init (GalViewInstanceSaveAsDialogClass *klass)
{
	GObjectClass *object_class;

	object_class = (GObjectClass*) klass;

	object_class->set_property = gal_view_instance_save_as_dialog_set_property;
	object_class->get_property = gal_view_instance_save_as_dialog_get_property;
	object_class->dispose      = gal_view_instance_save_as_dialog_dispose;

	g_object_class_install_property (object_class, PROP_INSTANCE,
					 g_param_spec_object ("instance",
							      _("Instance"),
							      /*_( */"XXX blurb" /*)*/,
							      GAL_VIEW_INSTANCE_TYPE,
							      G_PARAM_READWRITE));
}

static void
gal_view_instance_save_as_dialog_init (GalViewInstanceSaveAsDialog *dialog)
{
	GladeXML *gui;
	GtkWidget *widget;

	gchar *filename = g_build_filename (EVOLUTION_GLADEDIR,
					    "gal-view-instance-save-as-dialog.glade",
					    NULL);

	dialog->instance = NULL;
	dialog->model = NULL;
	dialog->collection = NULL;

	gui = glade_xml_new_with_domain (filename , NULL, GETTEXT_PACKAGE);
	g_free (filename);
	dialog->gui = gui;

	widget = glade_xml_get_widget (gui, "vbox-top");
	if (!widget) {
		return;
	}

	g_object_ref (widget);
	gtk_container_remove (GTK_CONTAINER (widget->parent), widget);

	/* TODO: add position/size saving/restoring */
	gtk_window_set_default_size (GTK_WINDOW (dialog), 300, 360);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), widget, TRUE, TRUE, 0);
	g_object_unref (widget);

	gtk_dialog_add_buttons (GTK_DIALOG (dialog),
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				GTK_STOCK_OK, GTK_RESPONSE_OK,
				NULL);

	dialog->scrolledwindow = glade_xml_get_widget (dialog->gui, "scrolledwindow2");
	dialog->treeview = GTK_TREE_VIEW (glade_xml_get_widget (dialog->gui, "custom-replace"));
	dialog->entry_create = glade_xml_get_widget (dialog->gui, "entry-create");
	dialog->radiobutton_replace = glade_xml_get_widget (dialog->gui, "radiobutton-replace");
	dialog->radiobutton_create = glade_xml_get_widget (dialog->gui, "radiobutton-create");

	gtk_tree_view_set_reorderable (GTK_TREE_VIEW (dialog->treeview), FALSE);
	gtk_tree_view_set_headers_visible (dialog->treeview, FALSE);

	g_signal_connect (dialog->radiobutton_replace, "toggled", G_CALLBACK (gvisad_radio_toggled), dialog);
	g_signal_connect (dialog->radiobutton_create,  "toggled", G_CALLBACK (gvisad_radio_toggled), dialog);
	g_signal_connect (dialog->entry_create,        "changed", G_CALLBACK (gvisad_entry_changed), dialog);

	gvisad_setup_radio_buttons (dialog);
	gvisad_setup_validate_button (dialog);

	gtk_window_set_title (GTK_WINDOW (dialog), _("Save Current View"));
	gtk_widget_show (GTK_WIDGET (dialog));
}

/* External methods */
/**
 * gal_view_instance_save_as_dialog_new
 *
 * Returns a new dialog for defining views.
 *
 * Returns: The GalViewInstanceSaveAsDialog.
 */
GtkWidget *
gal_view_instance_save_as_dialog_new (GalViewInstance *instance)
{
	GtkWidget *widget = g_object_new (GAL_VIEW_INSTANCE_SAVE_AS_DIALOG_TYPE, NULL);
	gal_view_instance_save_as_dialog_set_instance (GAL_VIEW_INSTANCE_SAVE_AS_DIALOG (widget), instance);
	return widget;
}

void
gal_view_instance_save_as_dialog_save (GalViewInstanceSaveAsDialog *dialog)
{
	GalView *view = gal_view_instance_get_current_view (dialog->instance);
	const gchar *title;
	gint n;
	const gchar *id = NULL;
	GalViewCollectionItem *item;

	view = gal_view_clone (view);
	switch (dialog->toggle) {
	case GAL_VIEW_INSTANCE_SAVE_AS_DIALOG_TOGGLE_REPLACE:
		if (dialog->treeview) {
			GtkTreeIter iter;
			GtkTreeSelection *selection;

			selection = gtk_tree_view_get_selection (dialog->treeview);
			if (gtk_tree_selection_get_selected (selection, &dialog->model, &iter)) {
				gtk_tree_model_get (dialog->model, &iter, COL_GALVIEW_DATA, &item, -1);

				for (n=0; n<dialog->instance->collection->view_count; n++) {
					if (item == dialog->instance->collection->view_data[n]) {
						id = gal_view_collection_set_nth_view (dialog->instance->collection, n, view);
						gal_view_collection_save (dialog->instance->collection);
					}
				}
			}

		}
		break;

	case GAL_VIEW_INSTANCE_SAVE_AS_DIALOG_TOGGLE_CREATE:
		if (dialog->entry_create && GTK_IS_ENTRY (dialog->entry_create)) {
			title = gtk_entry_get_text (GTK_ENTRY (dialog->entry_create));
			id = gal_view_collection_append_with_title (dialog->instance->collection, title, view);
			gal_view_collection_save (dialog->instance->collection);
		}
		break;
	}

	if (id) {
		gal_view_instance_set_current_view_id (dialog->instance, id);
	}
}
