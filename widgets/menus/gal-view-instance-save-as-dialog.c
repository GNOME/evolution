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

#include <gtk/gtk.h>

#include "gal/e-table/e-table-scrolled.h"
#include "gal/util/e-i18n.h"
#include "gal/util/e-util-private.h"

#include "gal-define-views-model.h"
#include "gal-view-instance-save-as-dialog.h"
#include "gal-view-new-dialog.h"

static GtkDialogClass *parent_class = NULL;
#define PARENT_TYPE GTK_TYPE_DIALOG

/* The arguments we take */
enum {
	PROP_0,
	PROP_INSTANCE,
};

typedef struct {
	char         *title;
	ETableModel  *model;
	GalViewInstanceSaveAsDialog *names;
} GalViewInstanceSaveAsDialogChild;


/* Static functions */
static void
gal_view_instance_save_as_dialog_set_instance(GalViewInstanceSaveAsDialog *dialog,
					      GalViewInstance *instance)
{
	dialog->instance = instance;
	if (dialog->model) {
		GtkWidget *table;
		g_object_set(dialog->model,
			     "collection", instance ? instance->collection : NULL,
			     NULL);
		table = glade_xml_get_widget(dialog->gui, "custom-replace");
		if (table) {
			ETable *etable;
			etable = e_table_scrolled_get_table (E_TABLE_SCROLLED (table));
			e_selection_model_select_single_row (e_table_get_selection_model (etable), 0);
			e_selection_model_change_cursor (e_table_get_selection_model (etable), 0, 0);
		}
	}
}

static void
gvisad_setup_radio_buttons (GalViewInstanceSaveAsDialog *dialog)
{
	GtkWidget   *radio_replace = glade_xml_get_widget (dialog->gui, "radiobutton-replace");
	GtkWidget   *radio_create  = glade_xml_get_widget (dialog->gui, "radiobutton-create" );
	GtkWidget   *widget;

	widget = glade_xml_get_widget (dialog->gui, "custom-replace");
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (radio_replace))) {
		gtk_widget_set_sensitive (widget, TRUE);
		dialog->toggle = GAL_VIEW_INSTANCE_SAVE_AS_DIALOG_TOGGLE_REPLACE;
	} else {
		gtk_widget_set_sensitive (widget, FALSE);
	}

	widget = glade_xml_get_widget (dialog->gui, "entry-create");
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (radio_create))) {
		gtk_widget_set_sensitive (widget, TRUE);
		dialog->toggle = GAL_VIEW_INSTANCE_SAVE_AS_DIALOG_TOGGLE_CREATE;
	} else {
		gtk_widget_set_sensitive (widget, FALSE);
	}
}

static void
gvisad_radio_toggled (GtkWidget *widget, GalViewInstanceSaveAsDialog *dialog)
{
	gvisad_setup_radio_buttons (dialog);
}

static void
gvisad_connect_signal(GalViewInstanceSaveAsDialog *dialog, char *widget_name, char *signal, GCallback handler)
{
	GtkWidget *widget;

	widget = glade_xml_get_widget(dialog->gui, widget_name);

	if (widget)
		g_signal_connect (G_OBJECT (widget), signal, handler, dialog);
}

/* Method override implementations */
static void
gal_view_instance_save_as_dialog_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	GalViewInstanceSaveAsDialog *dialog;

	dialog = GAL_VIEW_INSTANCE_SAVE_AS_DIALOG (object);
	
	switch (prop_id){
	case PROP_INSTANCE:
		if (g_value_get_object (value))
			gal_view_instance_save_as_dialog_set_instance(dialog, GAL_VIEW_INSTANCE(g_value_get_object (value)));
		else
			gal_view_instance_save_as_dialog_set_instance(dialog, NULL);
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
	GalViewInstanceSaveAsDialog *gal_view_instance_save_as_dialog = GAL_VIEW_INSTANCE_SAVE_AS_DIALOG(object);

	if (gal_view_instance_save_as_dialog->gui)
		g_object_unref(gal_view_instance_save_as_dialog->gui);
	gal_view_instance_save_as_dialog->gui = NULL;

	if (G_OBJECT_CLASS (parent_class)->dispose)
		(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

/* Init functions */
static void
gal_view_instance_save_as_dialog_class_init (GalViewInstanceSaveAsDialogClass *klass)
{
	GObjectClass *object_class;

	object_class = (GObjectClass*) klass;

	parent_class = g_type_class_ref (PARENT_TYPE);

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
	GtkWidget *table;
	gchar *filename = g_build_filename (GAL_GLADEDIR,
					    "gal-view-instance-save-as-dialog.glade",
					    NULL);

	dialog->instance = NULL;

	gui = glade_xml_new_with_domain (filename , NULL, E_I18N_DOMAIN);
	g_free (filename);
	dialog->gui = gui;

	widget = glade_xml_get_widget(gui, "vbox-top");
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

	gvisad_connect_signal(dialog, "radiobutton-replace", "toggled", G_CALLBACK(gvisad_radio_toggled));
	gvisad_connect_signal(dialog, "radiobutton-create",  "toggled", G_CALLBACK(gvisad_radio_toggled));

	dialog->model = NULL;
	table = glade_xml_get_widget(dialog->gui, "custom-replace");
	if (table) {
		dialog->model = g_object_get_data(G_OBJECT (table), "GalViewInstanceSaveAsDialog::model");

		gal_view_instance_save_as_dialog_set_instance (dialog, dialog->instance);
		gtk_widget_show_all (table);
	}
	
	gvisad_setup_radio_buttons (dialog);
	gtk_window_set_policy(GTK_WINDOW(dialog), FALSE, TRUE, FALSE);
	gtk_window_set_title (GTK_WINDOW (dialog), _("Save Current View"));
}


/* For use from libglade. */
/* ETable creation */
#define SPEC "<ETableSpecification no-headers=\"true\" cursor-mode=\"line\" draw-grid=\"false\" selection-mode=\"single\" gettext-domain=\"" E_I18N_DOMAIN "\">" \
	     "<ETableColumn model_col= \"0\" _title=\"Name\" expansion=\"1.0\" minimum_width=\"18\" resizable=\"true\" cell=\"string\" compare=\"string\"/>" \
             "<ETableState> <column source=\"0\"/> <grouping> </grouping> </ETableState>" \
	     "</ETableSpecification>"

GtkWidget *gal_view_instance_save_as_dialog_create_etable(char *name, char *string1, char *string2, int int1, int int2);

GtkWidget *
gal_view_instance_save_as_dialog_create_etable(char *name, char *string1, char *string2, int int1, int int2)
{
	GtkWidget *table;
	ETableModel *model;
	model = gal_define_views_model_new ();
	table = e_table_scrolled_new(model, NULL, SPEC, NULL);
	g_object_set_data(G_OBJECT (table), "GalViewInstanceSaveAsDialog::model", model);

	return table;
}

/* External methods */
/**
 * gal_view_instance_save_as_dialog_new
 *
 * Returns a new dialog for defining views.
 *
 * Returns: The GalViewInstanceSaveAsDialog.
 */
GtkWidget*
gal_view_instance_save_as_dialog_new (GalViewInstance *instance)
{
	GtkWidget *widget = g_object_new (GAL_VIEW_INSTANCE_SAVE_AS_DIALOG_TYPE, NULL);
	gal_view_instance_save_as_dialog_set_instance(GAL_VIEW_INSTANCE_SAVE_AS_DIALOG (widget), instance);
	return widget;
}

E_MAKE_TYPE(gal_view_instance_save_as_dialog, "GalViewInstanceSaveAsDialog",
	    GalViewInstanceSaveAsDialog,
	    gal_view_instance_save_as_dialog_class_init,
	    gal_view_instance_save_as_dialog_init, PARENT_TYPE)

void
gal_view_instance_save_as_dialog_save (GalViewInstanceSaveAsDialog *dialog)
{
	GalView *view = gal_view_instance_get_current_view (dialog->instance);
	GtkWidget *widget;
	const char *title;
	int n;
	const char *id = NULL;

	view = gal_view_clone (view);
	switch (dialog->toggle) {
	case GAL_VIEW_INSTANCE_SAVE_AS_DIALOG_TOGGLE_REPLACE:
		widget = glade_xml_get_widget(dialog->gui, "custom-replace");
		if (widget && E_IS_TABLE_SCROLLED (widget)) {
			n = e_table_get_cursor_row (e_table_scrolled_get_table (E_TABLE_SCROLLED (widget)));
			id = gal_view_collection_set_nth_view (dialog->instance->collection, n, view);
			gal_view_collection_save (dialog->instance->collection);
		}
		break;
	case GAL_VIEW_INSTANCE_SAVE_AS_DIALOG_TOGGLE_CREATE:
		widget = glade_xml_get_widget(dialog->gui, "entry-create");
		if (widget && GTK_IS_ENTRY (widget)) {
			title = gtk_entry_get_text (GTK_ENTRY (widget));
			id = gal_view_collection_append_with_title (dialog->instance->collection, title, view);
			gal_view_collection_save (dialog->instance->collection);
		}
		break;
	}

	if (id) {
		gal_view_instance_set_current_view_id (dialog->instance, id);
	}
}
