/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * E-table-config.c: The ETable config dialog.
 *
 * Author:
 *   Chris Lahey <clahey@helixcode.com>
 *
 * (C) 2000 Helix Code, Inc.
 */

#include <config.h>
#include <stdlib.h>
#include <libgnomeui/gnome-dialog.h>
#include <glade/glade.h>
#include "e-table-config.h"
#include "e-table-config-field.h"
#include "gal/util/e-util.h"


#define PARENT_TYPE (gtk_object_get_type())

static GtkObjectClass *config_parent_class;

static void
config_destroy (GtkObject *object)
{
	ETableConfig *config = E_TABLE_CONFIG (object);

	if (config->state) {
		if (config->sorting_changed_id)
			gtk_signal_disconnect(GTK_OBJECT(config->state->sort_info), config->sorting_changed_id);
		if (config->grouping_changed_id)
			gtk_signal_disconnect(GTK_OBJECT(config->state->sort_info), config->grouping_changed_id);
		gtk_object_unref(GTK_OBJECT(config->state));
	}

	gtk_object_unref(GTK_OBJECT(config->spec));

	GTK_OBJECT_CLASS (config_parent_class)->destroy (object);
}

static void
config_class_init (GtkObjectClass *klass)
{
	config_parent_class = gtk_type_class (PARENT_TYPE);
	
	klass->destroy = config_destroy;
}

static void
config_clear_sort (GtkWidget *widget, ETableConfig *config)
{
	config->sort_dialog = NULL;
	gtk_object_unref(GTK_OBJECT(config));
}

static void
config_clear_group (GtkWidget *widget, ETableConfig *config)
{
	config->group_dialog = NULL;
	gtk_object_unref(GTK_OBJECT(config));
}

static void
config_sort_config_show (GtkWidget *widget, ETableConfig *config)
{
	if (config->sort_dialog)
		gdk_window_raise (GTK_WIDGET (config->sort_dialog)->window);
	else {
		GtkWidget *etcf;
		config->sort_dialog = gnome_dialog_new(_("Sort"),
						     GNOME_STOCK_BUTTON_OK,
						     NULL);
		etcf = GTK_WIDGET(e_table_config_field_new(config->spec,
							   config->state->sort_info,
							   FALSE));
		gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(config->sort_dialog)->vbox), etcf, FALSE, FALSE, 0);
		gnome_dialog_set_parent(GNOME_DIALOG(config->sort_dialog),
					GTK_WINDOW(config));

		gtk_signal_connect(GTK_OBJECT(config->sort_dialog), "destroy",
				   GTK_SIGNAL_FUNC(config_clear_sort), config);
		gtk_object_ref(GTK_OBJECT(config));

		gtk_signal_connect(GTK_OBJECT(config->sort_dialog), "clicked",
				   GTK_SIGNAL_FUNC(gnome_dialog_close), config);

		gtk_widget_show(GTK_WIDGET(etcf));
		gtk_widget_show(GTK_WIDGET(config->sort_dialog));
	}
}

static void
config_group_config_show(GtkWidget *widget, ETableConfig *config)
{
	if (config->group_dialog)
		gdk_window_raise(GTK_WIDGET(config->group_dialog)->window);
	else {
		GtkWidget *etcf;
		config->group_dialog = gnome_dialog_new(_("Group"),
						      GNOME_STOCK_BUTTON_OK,
						      NULL);
		etcf = GTK_WIDGET(e_table_config_field_new(config->spec,
							   config->state->sort_info,
							   TRUE));
		gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(config->group_dialog)->vbox), etcf, FALSE, FALSE, 0);
		gnome_dialog_set_parent(GNOME_DIALOG(config->group_dialog),
					GTK_WINDOW(config));

		gtk_signal_connect(GTK_OBJECT(config->group_dialog), "destroy",
				   GTK_SIGNAL_FUNC(config_clear_group), config);
		gtk_signal_connect(GTK_OBJECT(config->group_dialog), "clicked",
				   GTK_SIGNAL_FUNC(gnome_dialog_close), config);
		gtk_object_ref(GTK_OBJECT(config));

		gtk_widget_show(GTK_WIDGET(etcf));
		gtk_widget_show(GTK_WIDGET(config->group_dialog));
	}
}

static void
config_sort_info_update (ETableConfig *config)
{
	ETableSortInfo *info = config->state->sort_info;
	GString *res;
	int count, i;

	count = e_table_sort_info_sorting_get_count (info);
	res = g_string_new ("");

	for (i = 0; i < count; i++) {
		ETableSortColumn col = e_table_sort_info_sorting_get_nth (info, i);
		ETableColumnSpecification **column;

		for (column = config->spec->columns; *column; column++) {
			if (col.column == (*column)->model_col) {
				g_string_append (res, (*column)->title_);
				g_string_append_c (res, ' ');
				g_string_append (
					res,
					col.ascending ?
					_("(Ascending)") : _("(Descending)"));
				break;
			}
		}
	}
	if (res->str [0] == 0)
		g_string_append (res, _("Not sorted"));
	
	gtk_label_set_text (GTK_LABEL(config->sort_label), res->str);

	g_string_free (res, TRUE);
}

static void
config_group_info_update (ETableConfig *config)
{
	ETableSortInfo *info = config->state->sort_info;
	GString *res;
	int count, i;

	count = e_table_sort_info_grouping_get_count (info);
	res = g_string_new ("");

	for (i = 0; i < count; i++) {
		ETableSortColumn col = e_table_sort_info_grouping_get_nth (info, i);
		ETableColumnSpecification **column;

		for (column = config->spec->columns; *column; column++) {
			if (col.column == (*column)->model_col) {
				g_string_append (res, (*column)->title_);
				g_string_append_c (res, ' ');
				g_string_append (
					res,
					col.ascending ?
					_("(Ascending)") : _("(Descending)"));
			}
		}
	}
	if (res->str [0] == 0)
		g_string_append (res, _("No grouping"));
	
	gtk_label_set_text (GTK_LABEL (config->group_label), res->str);
	g_string_free (res, TRUE);
}

static void
config_fields_info_update (ETableConfig *config)
{
	ETableColumnSpecification **column;
	GString *res = g_string_new ("");

	for (column = config->spec->columns; *column; *column++){
		g_string_append (res, (*column)->title_);
		if (column [1])
			g_string_append (res, ", ");
	}

	gtk_label_set_text (GTK_LABEL (config->fields_label), res->str);
	g_string_free (res, TRUE);
}

static void
apply_changes (ETableConfig *config)
{
	/* Do apply changes here */
}

static void
dialog_destroyed (GtkObject *dialog, ETableConfig *config)
{
	gtk_object_destroy (GTK_OBJECT (config));
}

static void
setup_gui (ETableConfig *config)
{
	GtkWidget *sort_button;
	GtkWidget *group_button;
	GladeXML *gui = glade_xml_new (
		ETABLE_GLADEDIR "/e-table-config.glade", NULL);
	
	config->dialog_toplevel = glade_xml_get_widget (
		gui, "e-table-config");

	gtk_notebook_set_show_tabs (
		GTK_NOTEBOOK (GNOME_PROPERTY_BOX (
			config->dialog_toplevel)->notebook),
		FALSE);
	
	gtk_signal_connect (
		GTK_OBJECT (config->dialog_toplevel), "apply",
		GTK_SIGNAL_FUNC (apply_changes), config);
	
	config->dialog_show_fields = glade_xml_get_widget (
		gui, "dialog-show-fields");
	config->dialog_group_by = glade_xml_get_widget (
		gui, "dialog-group-by");
	config->dialog_sort = glade_xml_get_widget (
		gui, "dialog-sort");

	config->sort_label = glade_xml_get_widget (
		gui, "label-sort");
	config->group_label = glade_xml_get_widget (
		gui, "label-group");
	config->fields_label = glade_xml_get_widget (
		gui, "label-fields");

	sort_button = glade_xml_get_widget (gui, "button-sort");
	gtk_signal_connect(
		GTK_OBJECT(sort_button), "clicked",
		GTK_SIGNAL_FUNC (config_sort_config_show), config);

	group_button = glade_xml_get_widget (gui, "button-group");
	gtk_signal_connect(
		GTK_OBJECT(group_button), "clicked",
		GTK_SIGNAL_FUNC (config_group_config_show), config);

	gtk_signal_connect (
		GTK_OBJECT (config->dialog_toplevel), "destroy",
		GTK_SIGNAL_FUNC (dialog_destroyed), config);
	
	gtk_object_unref (GTK_OBJECT (gui));
}

static void
config_init (ETableConfig *config)
{
	glade_gnome_init ();
	setup_gui (config);

	config->sorting_changed_id = 0;
	config->grouping_changed_id = 0;
}

E_MAKE_TYPE(e_table_config, "ETableConfig", ETableConfig, config_class_init, config_init, PARENT_TYPE);

ETableConfig *
e_table_config_new (const char          *header,
		    ETableSpecification *spec,
		    ETableState         *state)
{
	ETableConfig *config = gtk_type_new (E_TABLE_CONFIG_TYPE);

	if (e_table_config_construct (config, config, spec, state) == NULL){
		gtk_object_destroy (GTK_OBJECT (config));
		return NULL;
	}

	gtk_widget_show (config->dialog_toplevel);
	return E_TABLE_CONFIG (config);
}

ETableConfig *
e_table_config_construct (ETableConfig        *config,
			  const char          *header,
			  ETableSpecification *spec,
			  ETableState         *state)
{
	config->spec = spec;
	config->state = state;

	if (config->spec)
		gtk_object_ref (GTK_OBJECT(config->spec));
	if (config->state)
		gtk_object_ref (GTK_OBJECT(config->state));

	/*
	 * FIXME:
	 *
	 * Are we going to allow async changes to the ETable?  If so,
	 * we are in for some more work than required
	 *
	 */
	config->sorting_changed_id = gtk_signal_connect (
		GTK_OBJECT(config->state->sort_info), "sort_info_changed",
		GTK_SIGNAL_FUNC(config_sort_info_update), config);
	config->grouping_changed_id = gtk_signal_connect (
		GTK_OBJECT(config->state->sort_info), "group_info_changed",
		GTK_SIGNAL_FUNC (config_group_info_update), config);

	config_sort_info_update   (config);
	config_group_info_update  (config);
	config_fields_info_update (config);
	
	return E_TABLE_CONFIG (config);
}

void
e_table_config_raise (ETableConfig *config)
{
	gdk_window_raise (GTK_WIDGET (config->dialog_toplevel)->window);
}
