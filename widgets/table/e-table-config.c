/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * E-table-config.c: The ETable config dialog.
 *
 * Authors:
 *   Chris Lahey (clahey@ximian.com)
 *   Miguel de Icaza (miguel@ximian.com)
 *
 * FIXME:
 *    Sort Dialog: when text is selected, the toggle button switches state.
 *    Make Clear all work.
 *
 * (C) 2000, 2001 Ximian, Inc.
 */

#include <config.h>
#include <stdlib.h>
#include "gal/util/e-i18n.h"
#include <libgnomeui/gnome-dialog.h>
#include <glade/glade.h>
#include "e-table-config.h"
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

static int
find_model_column_by_name (ETableSpecification *spec, const char *s)
{
	ETableColumnSpecification **column;

	for (column = spec->columns; *column; column++){

		if (strcasecmp ((*column)->title, s) == 0)
			return (*column)->model_col;
	}
	return -1;
}

static void
update_sort_config_dialog (ETableConfig *config)
{
	int count, i;
	
	count = e_table_sort_info_sorting_get_count (config->temp_state->sort_info);
	
	for (i = 0; i < 4; i++){
		gboolean sensitive = (i <= count);
		
		gtk_widget_set_sensitive (config->sort [i].frames, sensitive);

		/*
		 * Sorting is set, auto select the text
		 */
		gtk_signal_handler_block (
			GTK_OBJECT (config->sort [i].radio_ascending),
			config->sort [i].toggled_id);
		
		if (i < count){
			GtkToggleButton *a, *d;
			ETableSortColumn col =
				e_table_sort_info_sorting_get_nth (
					config->temp_state->sort_info,
					i);
			
			ETableColumnSpecification *column =
				find_column_in_spec (config->temp_spec, col.column);

			if (!column){
				/*
				 * This is a bug in the programmer
				 * stuff, but by the time we arrive
				 * here, the user has been given a
				 * warning
				 */
				continue;
			}

			/*
			 * Change the text
			 */
			gtk_signal_handler_block (
				GTK_OBJECT (config->sort [i].combo->entry),
				config->sort [i].changed_id);
			gtk_combo_text_set_text (
				config->sort [i].combo, gettext (
					column->title));
			gtk_signal_handler_unblock (
				GTK_OBJECT (config->sort [i].combo->entry),
				config->sort [i].changed_id);

			/*
			 * Update radio buttons
			 */
			a = GTK_TOGGLE_BUTTON (
				config->sort [i].radio_ascending);
			d = GTK_TOGGLE_BUTTON (
				config->sort [i].radio_descending);
			
			gtk_toggle_button_set_active (col.ascending ? a:d, 1);
		} else {
			GtkToggleButton *t;
			
			t = GTK_TOGGLE_BUTTON (
				config->sort [i].radio_ascending);

			gtk_toggle_button_set_active (t, 1);
		}
		gtk_signal_handler_unblock (
			GTK_OBJECT (config->sort [i].radio_ascending),
			config->sort [i].toggled_id);
	}

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

		if ((i + 1) != count)
			g_string_append (res, ", ");
	}
	
	if (res->str [0] == 0)
		g_string_append (res, _("Not sorted"));
	
	gtk_label_set_text (GTK_LABEL(config->sort_label), res->str);

	g_string_free (res, TRUE);
}


static void
config_sort_config_show (GtkWidget *widget, ETableConfig *config)
{
	GnomeDialog *dialog = GNOME_DIALOG (config->dialog_sort);
	int button, running = 1;

	config->temp_spec = e_table_specification_duplicate (config->spec);
	config->temp_state = e_table_state_duplicate (config->state);
	
	update_sort_config_dialog (config);
	gtk_widget_grab_focus (GTK_WIDGET (config->sort [0].combo));

	do {
		button = gnome_dialog_run (dialog);
		switch (button){
		case 0:
			e_table_sort_info_sorting_truncate (
				config->state->sort_info, 0);
			update_sort_config_dialog (config);
			continue;
		case 1:
			gtk_object_unref (GTK_OBJECT (config->spec));
			gtk_object_unref (GTK_OBJECT (config->state));
			config->spec = config->temp_spec;
			config->state = config->temp_state;
			running = 0;
			break;
			
		case 2:
			gtk_object_unref (GTK_OBJECT (config->temp_state));
			gtk_object_unref (GTK_OBJECT (config->temp_spec));
			config->temp_state = 0;
			config->temp_spec = 0;
			running = 0;
			break;
		}
		
	} while (running);
	gnome_dialog_close (GNOME_DIALOG (dialog));

	config_sort_info_update (config);
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

		if ((i+1) != count)
			g_string_append (res, ", ");
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
entry_changed (GtkEntry *entry, ETableConfigSortWidgets *sort)
{
	ETableConfig *config = sort->e_table_config;
	ETableSortInfo *sort_info = config->temp_state->sort_info;
	ETableConfigSortWidgets *base = &config->sort[0];
	int idx = sort - base;
	
	char *s = gtk_entry_get_text (entry);

	if (s [0] == 0){
		printf ("Entry %d is empty!\n", idx);
		e_table_sort_info_sorting_truncate (sort_info, idx);
		update_sort_config_dialog (config);
		return;
	}

	if (g_hash_table_lookup (sort->combo->elements, s)){
		ETableSortColumn c;
		int col;
		
		col = find_model_column_by_name (config->temp_spec, s);
		if (col == -1){
			g_warning ("This should not happen");
			return;
		}

		c.ascending = 1;
		c.column = col;
		e_table_sort_info_sorting_set_nth (sort_info, idx, c);
		  
		update_sort_config_dialog (config);
		return;
	}
}

static void
ascending_toggled (GtkToggleButton *t, ETableConfigSortWidgets *sort)
{
	ETableConfig *config = sort->e_table_config;
	ETableSortInfo *si = config->temp_state->sort_info;
	ETableConfigSortWidgets *base = &config->sort[0];
	int idx = sort - base;
	ETableSortColumn c;
	
	c = e_table_sort_info_sorting_get_nth (si, idx);
	c.ascending = t->active;
	e_table_sort_info_sorting_set_nth (si, idx, c);
}

static void
configure_sort_dialog (ETableConfig *config, GladeXML *gui)
{
	ETableColumnSpecification **column;
	int i;
	
	for (i = 0; i < 4; i++){
		char buffer [80];

		snprintf (buffer, sizeof (buffer), "sort-combo-%d", i + 1);
		config->sort [i].combo = GTK_COMBO_TEXT (
			glade_xml_get_widget (gui, buffer));

		gtk_combo_text_add_item (config->sort [i].combo, "", "");

		snprintf (buffer, sizeof (buffer), "frame-sort-%d", i + 1);
		config->sort [i].frames = 
			glade_xml_get_widget (gui, buffer);

		snprintf (
			buffer, sizeof (buffer),
			"radiobutton-ascending-sort-%d", i+1);
		config->sort [i].radio_ascending = glade_xml_get_widget (
			gui, buffer);

		snprintf (
			buffer, sizeof (buffer),
			"radiobutton-descending-sort-%d", i+1);
		config->sort [i].radio_descending = glade_xml_get_widget (
			gui, buffer);

		config->sort [i].e_table_config = config;
	}

	for (column = config->spec->columns; *column; column++){
		char *label = (*column)->title;

		for (i = 0; i < 4; i++){
			gtk_combo_text_add_item (
				config->sort [i].combo,
				gettext (label), label);
		}
	}

	/*
	 * After we have runtime modified things, signal connect
	 */
	for (i = 0; i < 4; i++){
		config->sort [i].changed_id = gtk_signal_connect (
			GTK_OBJECT (config->sort [i].combo->entry),
			"changed", entry_changed, &config->sort [i]);

		config->sort [i].toggled_id = gtk_signal_connect (
			GTK_OBJECT (config->sort [i].radio_ascending),
			"toggled", ascending_toggled, &config->sort [i]);
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
