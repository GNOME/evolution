/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * gal-define-views-dialog.c
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

#include "gal-define-views-dialog.h"

#include <gtk/gtk.h>
#include "gal-define-views-model.h"
#include "gal-view-new-dialog.h"
#include <gal/e-table/e-table-scrolled.h>
#include <gal/util/e-i18n.h>
#include <gal/util/e-util.h>

static void gal_define_views_dialog_init	 (GalDefineViewsDialog		 *card);
static void gal_define_views_dialog_class_init	 (GalDefineViewsDialogClass	 *klass);
static void gal_define_views_dialog_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void gal_define_views_dialog_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static void gal_define_views_dialog_dispose	 (GObject *object);

static GtkDialogClass *parent_class = NULL;
#define PARENT_TYPE GTK_TYPE_DIALOG

/* The properties we support */
enum {
	PROP_0,
	PROP_COLLECTION
};

typedef struct {
	char         *title;
	ETableModel  *model;
	GalDefineViewsDialog *names;
} GalDefineViewsDialogChild;


E_MAKE_TYPE(gal_define_views_dialog, "GalDefineViewsDialog", GalDefineViewsDialog, gal_define_views_dialog_class_init, gal_define_views_dialog_init, PARENT_TYPE)

static void
gal_define_views_dialog_class_init (GalDefineViewsDialogClass *klass)
{
	GObjectClass *object_class;

	object_class = (GObjectClass*) klass;

	parent_class = g_type_class_ref (PARENT_TYPE);

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

/* ETable creation */
#define SPEC "<ETableSpecification cursor-mode=\"line\" draw-grid=\"true\" selection-mode=\"single\" gettext-domain=\"" E_I18N_DOMAIN "\">" \
	     "<ETableColumn model_col= \"0\" _title=\"Name\" expansion=\"1.0\" minimum_width=\"18\" resizable=\"true\" cell=\"string\" compare=\"string\"/>" \
             "<ETableState> <column source=\"0\"/> <grouping> </grouping> </ETableState>" \
	     "</ETableSpecification>"

/* For use from libglade. */
GtkWidget *gal_define_views_dialog_create_etable(char *name, char *string1, char *string2, int int1, int int2);

GtkWidget *
gal_define_views_dialog_create_etable(char *name, char *string1, char *string2, int int1, int int2)
{
	GtkWidget *table;
	ETableModel *model;
	model = gal_define_views_model_new();
	table = e_table_scrolled_new(model, NULL, SPEC, NULL);
	g_object_set_data(G_OBJECT (table), "GalDefineViewsDialog::model", model);
	return table;
}

/* Button callbacks */

static void
gdvd_button_new_dialog_callback(GtkWidget *widget, int id, GalDefineViewsDialog *dialog)
{
	gchar *name;
	GalView *view;
	GalViewFactory *factory;
	switch (id) {
	case GTK_RESPONSE_OK:
		g_object_get(widget,
			     "name", &name,
			     "factory", &factory,
			     NULL);
		if (name && factory) {
			g_strchomp(name);
			if (*name != '\0') {
				view = gal_view_factory_new_view(factory, name);
				gal_define_views_model_append(GAL_DEFINE_VIEWS_MODEL(dialog->model), view);
				gal_view_edit(view, GTK_WINDOW (dialog));
				g_object_unref(view);
			}
		}
		g_object_unref(factory);
		g_free(name);
		break;
	}
	gtk_widget_destroy (widget);
}

static void
gdvd_button_new_callback(GtkWidget *widget, GalDefineViewsDialog *dialog)
{
	GtkWidget *view_new_dialog = gal_view_new_dialog_new(dialog->collection);
	gtk_window_set_transient_for (GTK_WINDOW (view_new_dialog), GTK_WINDOW (dialog));
	g_signal_connect(view_new_dialog, "response",
			 G_CALLBACK(gdvd_button_new_dialog_callback), dialog);
	gtk_widget_show(view_new_dialog);
}

static void
gdvd_button_modify_callback(GtkWidget *widget, GalDefineViewsDialog *dialog)
{
	int row;
	GtkWidget *scrolled;
	ETable *etable;

	scrolled = glade_xml_get_widget(dialog->gui, "custom-table");
	etable = e_table_scrolled_get_table(E_TABLE_SCROLLED(scrolled));
	row = e_table_get_cursor_row (E_TABLE(etable));

	if (row != -1) {
		GalView *view;
		view = gal_define_views_model_get_view(GAL_DEFINE_VIEWS_MODEL(dialog->model),
						       row);
		gal_view_edit(view, GTK_WINDOW (dialog));
	}
}

static void
gdvd_button_delete_callback(GtkWidget *widget, GalDefineViewsDialog *dialog)
{
	int row;
	GtkWidget *scrolled;
	ETable *etable;

	scrolled = glade_xml_get_widget(dialog->gui, "custom-table");
	etable = e_table_scrolled_get_table(E_TABLE_SCROLLED(scrolled));
	row = e_table_get_cursor_row (E_TABLE(etable));

	if (row != -1) {
		gal_define_views_model_delete_view(GAL_DEFINE_VIEWS_MODEL(dialog->model),
						   row);
	}

}

#if 0
static void
gdvd_button_copy_callback(GtkWidget *widget, GalDefineViewsDialog *dialog)
{
	int row;
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
gdvd_connect_signal(GalDefineViewsDialog *dialog, char *widget_name, char *signal, GCallback handler)
{
	GtkWidget *widget;

	widget = glade_xml_get_widget(dialog->gui, widget_name);

	if (widget)
		g_signal_connect(widget, signal, handler, dialog);
}

static void
etable_selection_change_forall_cb (int row, GalDefineViewsDialog *dialog)
{
	if (row != -1) {
		GalViewCollectionItem *item = gal_view_collection_get_view_item (dialog->collection, row);

		if (item) {
			gtk_widget_set_sensitive (glade_xml_get_widget (dialog->gui, "button-delete"),
						  !item->built_in);
			if (GAL_VIEW_GET_CLASS (item->view)->edit)
				gtk_widget_set_sensitive (glade_xml_get_widget (dialog->gui, "button-modify"),
						item->built_in);
			else
				gtk_widget_set_sensitive (glade_xml_get_widget (dialog->gui, "button-modify"),
						!item->built_in);
		}
	}
}

static void
etable_selection_change (ETable *etable, GalDefineViewsDialog *dialog)
{
	e_table_selected_row_foreach (etable, (EForeachFunc) etable_selection_change_forall_cb, dialog);
}

static void
dialog_response (GalDefineViewsDialog *dialog, int response_id, gpointer data)
{
	gal_view_collection_save (dialog->collection);
}	

static void
gal_define_views_dialog_init (GalDefineViewsDialog *dialog)
{
	GladeXML *gui;
	GtkWidget *widget;
	GtkWidget *etable;

	dialog->collection = NULL;

	gui = glade_xml_new (GAL_GLADEDIR "/gal-define-views.glade", NULL, E_I18N_DOMAIN);
	dialog->gui = gui;

	widget = glade_xml_get_widget(gui, "table-top");
	if (!widget) {
		return;
	}
	gtk_widget_ref(widget);
	gtk_container_remove (GTK_CONTAINER (widget->parent), widget);
	gtk_window_set_default_size(GTK_WINDOW(dialog), 360, 270);
	gtk_container_set_border_width(GTK_CONTAINER(dialog), 6);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), widget, TRUE, TRUE, 0);
	gtk_widget_unref(widget);

	gtk_dialog_add_buttons (GTK_DIALOG (dialog),
				GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
				NULL);

	gdvd_connect_signal(dialog, "button-new",    "clicked", G_CALLBACK(gdvd_button_new_callback));
	gdvd_connect_signal(dialog, "button-modify", "clicked", G_CALLBACK(gdvd_button_modify_callback));
	gdvd_connect_signal(dialog, "button-delete", "clicked", G_CALLBACK(gdvd_button_delete_callback));
#if 0
	gdvd_connect_signal(dialog, "button-copy",   "clicked", G_CALLBACK(gdvd_button_copy_callback));
#endif

	dialog->model = NULL;
	etable = glade_xml_get_widget(dialog->gui, "custom-table");
	if (etable) {
		dialog->model = g_object_get_data(G_OBJECT (etable), "GalDefineViewsDialog::model");
		g_object_set(dialog->model,
			     "collection", dialog->collection,
			     NULL);
		g_signal_connect (e_table_scrolled_get_table (E_TABLE_SCROLLED (etable)),
				  "selection_change",
				  G_CALLBACK (etable_selection_change), dialog);
		gtk_widget_show_all (etable);
	}

	gtk_window_set_policy(GTK_WINDOW(dialog), FALSE, TRUE, FALSE);

	g_signal_connect (dialog, "response", G_CALLBACK (dialog_response), NULL);

}

static void
gal_define_views_dialog_dispose (GObject *object)
{
	GalDefineViewsDialog *gal_define_views_dialog = GAL_DEFINE_VIEWS_DIALOG(object);

	if (gal_define_views_dialog->gui)
		g_object_unref(gal_define_views_dialog->gui);
	gal_define_views_dialog->gui = NULL;

	if (G_OBJECT_CLASS (parent_class)->dispose)
		(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static void
gal_define_views_dialog_set_collection(GalDefineViewsDialog *dialog,
				       GalViewCollection *collection)
{
	dialog->collection = collection;
	if (dialog->model) {
		g_object_set(dialog->model,
			     "collection", collection,
			     NULL);
	}
	if (dialog->gui) {
		GtkWidget *widget = glade_xml_get_widget(dialog->gui, "label-views");
		if (widget && GTK_IS_LABEL (widget)) {
			if (collection->title) {
				char *text = g_strdup_printf (_("Define Views for %s"),
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
	gal_define_views_dialog_set_collection(GAL_DEFINE_VIEWS_DIALOG (widget), collection);
	return widget;
}

static void
gal_define_views_dialog_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	GalDefineViewsDialog *dialog;

	dialog = GAL_DEFINE_VIEWS_DIALOG (object);
	
	switch (prop_id){
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
