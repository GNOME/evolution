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

#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-stock.h>
#include "gal-define-views-model.h"
#include "gal-view-new-dialog.h"
#include <gal/e-table/e-table-scrolled.h>
#include <gal/util/e-i18n.h>

static void gal_define_views_dialog_init		(GalDefineViewsDialog		 *card);
static void gal_define_views_dialog_class_init	(GalDefineViewsDialogClass	 *klass);
static void gal_define_views_dialog_set_arg (GtkObject *o, GtkArg *arg, guint arg_id);
static void gal_define_views_dialog_get_arg (GtkObject *object, GtkArg *arg, guint arg_id);
static void gal_define_views_dialog_destroy (GtkObject *object);

static GnomeDialogClass *parent_class = NULL;
#define PARENT_TYPE gnome_dialog_get_type()

/* The arguments we take */
enum {
	ARG_0,
	ARG_COLLECTION
};

typedef struct {
	char         *title;
	ETableModel  *model;
	GalDefineViewsDialog *names;
} GalDefineViewsDialogChild;

GtkType
gal_define_views_dialog_get_type (void)
{
	static GtkType type = 0;

	if (!type) {
		static const GtkTypeInfo info =
		{
			"GalDefineViewsDialog",
			sizeof (GalDefineViewsDialog),
			sizeof (GalDefineViewsDialogClass),
			(GtkClassInitFunc) gal_define_views_dialog_class_init,
			(GtkObjectInitFunc) gal_define_views_dialog_init,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		type = gtk_type_unique (PARENT_TYPE, &info);
	}

	return type;
}

static void
gal_define_views_dialog_class_init (GalDefineViewsDialogClass *klass)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass*) klass;

	parent_class = gtk_type_class (PARENT_TYPE);

	object_class->set_arg = gal_define_views_dialog_set_arg;
	object_class->get_arg = gal_define_views_dialog_get_arg;
	object_class->destroy = gal_define_views_dialog_destroy;

	gtk_object_add_arg_type("GalDefineViewsDialog::collection", GAL_VIEW_COLLECTION_TYPE,
				GTK_ARG_READWRITE, ARG_COLLECTION);
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
	gtk_object_set_data(GTK_OBJECT(table), "GalDefineViewsDialog::model", model);
	return table;
}

/* Button callbacks */

static void
gdvd_button_new_dialog_callback(GtkWidget *widget, int button, GalDefineViewsDialog *dialog)
{
	gchar *name;
	GalView *view;
	GalViewFactory *factory;
	switch (button) {
	case 0:
		gtk_object_get(GTK_OBJECT(widget),
			       "name", &name,
			       "factory", &factory,
			       NULL);
		if (name && factory) {
			gchar *dup_of_name = name;
			g_strchomp(dup_of_name);
			if (*dup_of_name != '\0') {
				view = gal_view_factory_new_view(factory, dup_of_name);
				gal_define_views_model_append(GAL_DEFINE_VIEWS_MODEL(dialog->model), view);
				gal_view_edit(view);
				gtk_object_unref(GTK_OBJECT(view));
			}
			g_free(name);
		}
		break;
	}
	gnome_dialog_close(GNOME_DIALOG(widget));
}

static void
gdvd_button_new_callback(GtkWidget *widget, GalDefineViewsDialog *dialog)
{
	GtkWidget *view_new_dialog = gal_view_new_dialog_new(dialog->collection);
	gtk_signal_connect(GTK_OBJECT(view_new_dialog), "clicked",
			   GTK_SIGNAL_FUNC(gdvd_button_new_dialog_callback), dialog);
	gtk_widget_show(GTK_WIDGET(view_new_dialog));
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
		gal_view_edit(view);
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

static void
gdvd_connect_signal(GalDefineViewsDialog *dialog, char *widget_name, char *signal, GtkSignalFunc handler)
{
	GtkWidget *widget;

	widget = glade_xml_get_widget(dialog->gui, widget_name);

	if (widget)
		gtk_signal_connect(GTK_OBJECT(widget), signal, handler, dialog);
}

static void
gal_define_views_dialog_init (GalDefineViewsDialog *dialog)
{
	GladeXML *gui;
	GtkWidget *widget;
	GtkWidget *etable;

	dialog->collection = NULL;

	gui = glade_xml_new_with_domain (GAL_GLADEDIR "/gal-define-views.glade", NULL, E_I18N_DOMAIN);
	dialog->gui = gui;

	widget = glade_xml_get_widget(gui, "table-top");
	if (!widget) {
		return;
	}
	gtk_widget_ref(widget);
	gtk_widget_unparent(widget);
	gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(dialog)->vbox), widget, TRUE, TRUE, 0);
	gtk_widget_unref(widget);

	gnome_dialog_append_buttons(GNOME_DIALOG(dialog),
				    GNOME_STOCK_BUTTON_OK,
				    GNOME_STOCK_BUTTON_CANCEL,
				    NULL);

	gdvd_connect_signal(dialog, "button-new",    "clicked", GTK_SIGNAL_FUNC(gdvd_button_new_callback));
	gdvd_connect_signal(dialog, "button-modify", "clicked", GTK_SIGNAL_FUNC(gdvd_button_modify_callback));
	gdvd_connect_signal(dialog, "button-delete", "clicked", GTK_SIGNAL_FUNC(gdvd_button_delete_callback));
	gdvd_connect_signal(dialog, "button-copy",   "clicked", GTK_SIGNAL_FUNC(gdvd_button_copy_callback));

	dialog->model = NULL;
	etable = glade_xml_get_widget(dialog->gui, "custom-table");
	if (etable) {
		dialog->model = gtk_object_get_data(GTK_OBJECT(etable), "GalDefineViewsDialog::model");
		gtk_object_set(GTK_OBJECT(dialog->model),
			       "collection", dialog->collection,
			       NULL);
	}

	gtk_window_set_policy(GTK_WINDOW(dialog), FALSE, TRUE, FALSE);
}

static void
gal_define_views_dialog_destroy (GtkObject *object)
{
	GalDefineViewsDialog *gal_define_views_dialog = GAL_DEFINE_VIEWS_DIALOG(object);

	gtk_object_unref(GTK_OBJECT(gal_define_views_dialog->gui));

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
gal_define_views_dialog_set_collection(GalDefineViewsDialog *dialog,
				       GalViewCollection *collection)
{
	dialog->collection = collection;
	if (dialog->model) {
		gtk_object_set(GTK_OBJECT(dialog->model),
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
				g_free (text);
			} else {
				gtk_label_set_text (GTK_LABEL (widget),
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
	GtkWidget *widget = GTK_WIDGET (gtk_type_new (gal_define_views_dialog_get_type ()));
	gal_define_views_dialog_set_collection(GAL_DEFINE_VIEWS_DIALOG (widget), collection);
	return widget;
}

static void
gal_define_views_dialog_set_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	GalDefineViewsDialog *dialog;

	dialog = GAL_DEFINE_VIEWS_DIALOG (o);
	
	switch (arg_id){
	case ARG_COLLECTION:
		if (GTK_VALUE_OBJECT(*arg))
			gal_define_views_dialog_set_collection(dialog, GAL_VIEW_COLLECTION(GTK_VALUE_OBJECT(*arg)));
		else
			gal_define_views_dialog_set_collection(dialog, NULL);
		break;

	default:
		return;
	}
}

static void
gal_define_views_dialog_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	GalDefineViewsDialog *dialog;

	dialog = GAL_DEFINE_VIEWS_DIALOG (object);

	switch (arg_id) {
	case ARG_COLLECTION:
		if (dialog->collection)
			GTK_VALUE_OBJECT(*arg) = GTK_OBJECT(dialog->collection);
		else
			GTK_VALUE_OBJECT(*arg) = NULL;
		break;

	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}
