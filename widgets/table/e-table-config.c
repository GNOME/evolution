/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * E-table-config.c: The ETable config dialog.
 *
 * Authors:
 *   Chris Lahey (clahey@ximian.com)
 *   Miguel de Icaza (miguel@ximian.com)
 
 * (C) 2000, 2001 Ximian, Inc.
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

	gtk_object_destroy (GTK_OBJECT (config->state));
	gtk_object_destroy (GTK_OBJECT (config->spec));

	gtk_object_unref (GTK_OBJECT (config->state));
	gtk_object_unref (GTK_OBJECT (config->spec));
	
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
	gtk_object_unref (GTK_OBJECT(config));
}

static void
config_clear_group (GtkWidget *widget, ETableConfig *config)
{
	gtk_object_unref (GTK_OBJECT (config));
}

static ETableColumnSpecification *
find_column_in_spec (ETableSpecification *spec, int model_col)
{
	ETableColumnSpecification **column;

	for (column = spec->columns; *column; column++){
		if ((*column)->model_col != model_col)
			continue;

		return *column;
	}

	return NULL;
}

static void
config_sort_config_show (GtkWidget *widget, ETableConfig *config)
{
	GnomeDialog *dialog = GNOME_DIALOG (config->dialog_sort);
	int button, count, i;

	/*
	 * Make the dialog reflect the current state
	 */
	count = e_table_sort_info_sorting_get_count (config->state->sort_info);
	
	for (i = 0; i < 4; i++){
		gboolean sensitive = (i <= count);
		
		gtk_widget_set_sensitive (config->frames [i], sensitive);

		/*
		 * Sorting is set, auto select the text
		 */
		if ((i + 1) >= count){
			
		}
	}
	
	button = gnome_dialog_run (dialog);
}

static void
config_group_config_show (GtkWidget *widget, ETableConfig *config)
{
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
		ETableColumnSpecification *column;
		
		column = find_column_in_spec (config->spec, col.column);
		if (!column){
			g_warning ("Could not find column model in specification");
			continue;
		}
		
		g_string_append (res, gettext ((column)->title));
		g_string_append_c (res, ' ');
		g_string_append (
			res,
			col.ascending ?
			_("(Ascending)") : _("(Descending)"));
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
		ETableColumnSpecification *column;

		column = find_column_in_spec (config->spec, col.column);
		if (!column){
			g_warning ("Could not find model column in specification");
			continue;
		}
		
		g_string_append (res, gettext ((column)->title));
		g_string_append_c (res, ' ');
		g_string_append (
			res,
			col.ascending ?
			_("(Ascending)") : _("(Descending)"));
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
	int i, items = 0;

	for (i = 0; i < config->state->col_count; i++){
		for (column = config->spec->columns; *column; column++){

			if (config->state->columns [i] != (*column)->model_col)
				continue;
			
			g_string_append (res, gettext ((*column)->title));
			if (column [1])
				g_string_append (res, ", ");
			items++;
			
			if (items > 5)
				g_string_append_c (res, '\n');
		}
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
connect_button (ETableConfig *config, GladeXML *gui, const char *widget_name, void *cback)
{
	GtkWidget *button = glade_xml_get_widget (gui, widget_name);

	gtk_signal_connect(
		GTK_OBJECT (button), "clicked",
		GTK_SIGNAL_FUNC (cback), config);
}

/*
 * Invoked by the Glade auto-connect code
 */
GtkWidget *e_table_proxy_gtk_combo_text_new (void);
GtkWidget *
e_table_proxy_gtk_combo_text_new (void)
{
	return gtk_combo_text_new (TRUE);
}

static GtkWidget *
configure_dialog (GladeXML *gui, const char *widget_name, ETableConfig *config)
{
	GtkWidget *w;
	
	w = glade_xml_get_widget (gui, widget_name);

	return w;
}

static void
configure_sort_dialog (ETableConfig *config, GladeXML *gui)
{
	ETableColumnSpecification **column;
	int i;
	
	for (i = 0; i < 4; i++){
		char buffer [30];

		snprintf (buffer, sizeof (buffer), "sort-combo-%d", i + 1);
		config->sort_combos [i] = GTK_COMBO_TEXT (
			glade_xml_get_widget (gui, buffer));
		gtk_combo_text_add_item (config->sort_combos [i], "", "");

		snprintf (buffer, sizeof (buffer), "frame-sort-%d", i + 1);
		config->frames [i] = 
			glade_xml_get_widget (gui, buffer);
	}

	for (column = config->spec->columns; *column; column++){
		char *label = (*column)->title;

		for (i = 0; i < 4; i++){
			gtk_combo_text_add_item (
				config->sort_combos [i],
				gettext (label), label);
		}
	}
}

static void
setup_gui (ETableConfig *config)
{
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
	
	config->dialog_show_fields = configure_dialog (gui, "dialog-show-fields", config);
	config->dialog_group_by =  configure_dialog (gui, "dialog-group-by", config);
	config->dialog_sort = configure_dialog (gui, "dialog-sort", config);

	config->sort_label = glade_xml_get_widget (
		gui, "label-sort");
	config->group_label = glade_xml_get_widget (
		gui, "label-group");
	config->fields_label = glade_xml_get_widget (
		gui, "label-fields");

	connect_button (config, gui, "button-sort", config_sort_config_show);
	connect_button (config, gui, "button-group", config_group_config_show);

	configure_sort_dialog (config, gui);
	
	gtk_signal_connect (
		GTK_OBJECT (config->dialog_toplevel), "destroy",
		GTK_SIGNAL_FUNC (dialog_destroyed), config);
	
	gtk_object_unref (GTK_OBJECT (gui));
}

static void
config_init (ETableConfig *config)
{
	glade_gnome_init ();
}

ETableConfig *
e_table_config_construct (ETableConfig        *config,
			  const char          *header,
			  ETableSpecification *spec,
			  ETableState         *state)
{
	g_return_val_if_fail (config != NULL, NULL);
	g_return_val_if_fail (header != NULL, NULL);
	g_return_val_if_fail (spec != NULL, NULL);
	g_return_val_if_fail (state != NULL, NULL);
	
	config->source_spec = spec;
	config->source_state = state;

	gtk_object_ref (GTK_OBJECT (config->source_spec));
	gtk_object_ref (GTK_OBJECT (config->source_state));

	config->spec = e_table_specification_duplicate (spec);
	config->state = e_table_state_duplicate (state);

	setup_gui (config);

	config_sort_info_update   (config);
	config_group_info_update  (config);
	config_fields_info_update (config);
	
	return E_TABLE_CONFIG (config);
}

/**
 * e_table_config_new:
 * @header: The title of the dialog for the ETableConfig.
 * @spec: The specification for the columns to allow.
 * @state: The current state of the configuration.
 *
 * Creates a new ETable config object.
 *
 * Returns: The config object.
 */
ETableConfig *
e_table_config_new (const char          *header,
		    ETableSpecification *spec,
		    ETableState         *state)
{
	ETableConfig *config = gtk_type_new (E_TABLE_CONFIG_TYPE);

	if (e_table_config_construct (config, header, spec, state) == NULL){
		gtk_object_destroy (GTK_OBJECT (config));
		return NULL;
	}

	gtk_widget_show (config->dialog_toplevel);
	return E_TABLE_CONFIG (config);
}

/**
 * e_table_config_raise:
 * @config: The ETableConfig object.
 *
 * Raises the dialog associated with this ETableConfig object.
 */
void
e_table_config_raise (ETableConfig *config)
{
	gdk_window_raise (GTK_WIDGET (config->dialog_toplevel)->window);
}

E_MAKE_TYPE(e_table_config, "ETableConfig", ETableConfig, config_class_init, config_init, PARENT_TYPE);
