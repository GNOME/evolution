/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-table-config.c
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *   Miguel de Icaza <miguel@ximian.com>
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
/*
 * FIXME:
 *    Sort Dialog: when text is selected, the toggle button switches state.
 *    Make Clear all work.
 */

#include <config.h>

#include "e-table-config.h"

#include <stdlib.h>
#include <string.h>
#include <gtk/gtkentry.h>
#include <gtk/gtklabel.h>
#include <gtk/gtknotebook.h>
#include <gtk/gtksignal.h>
#include <gtk/gtktogglebutton.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-propertybox.h>
#include <glade/glade.h>
#include "gal/util/e-util.h"
#include "gal/util/e-i18n.h"

#include <e-table-scrolled.h>
#include <e-table-without.h>
#include <e-table-memory-store.h>


#define PARENT_TYPE (gtk_object_get_type())

static GtkObjectClass *config_parent_class;

enum {
	CHANGED,
	LAST_SIGNAL
};

enum {
	ARG_0,
	ARG_STATE
};

static guint e_table_config_signals [LAST_SIGNAL] = { 0, };

static void
config_destroy (GtkObject *object)
{
	ETableConfig *config = E_TABLE_CONFIG (object);

	gtk_object_destroy (GTK_OBJECT (config->state));
	gtk_object_unref (GTK_OBJECT (config->source_state));
	gtk_object_unref (GTK_OBJECT (config->source_spec));
	g_free (config->header);

	g_slist_free (config->column_names);
	config->column_names = NULL;
	
	GTK_OBJECT_CLASS (config_parent_class)->destroy (object);
}

static void
config_get_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	ETableConfig *config = E_TABLE_CONFIG (o);

	switch (arg_id){
	case ARG_STATE:
		GTK_VALUE_OBJECT (*arg) = (GtkObject *) config->state;
		break;

	default:
		break;
	}
}

static void
e_table_config_changed (ETableConfig *config, ETableState *state)
{
	g_return_if_fail (config != NULL);
	g_return_if_fail (E_IS_TABLE_CONFIG (config));

	
	gtk_signal_emit(GTK_OBJECT(config),
			e_table_config_signals [CHANGED],
			state);
}

static void
config_class_init (GtkObjectClass *object_class)
{
	ETableConfigClass *klass = E_TABLE_CONFIG_CLASS(object_class);

	config_parent_class   = gtk_type_class (PARENT_TYPE);
	
	klass->changed        = NULL;

	object_class->get_arg = config_get_arg;
	object_class->destroy = config_destroy;

	e_table_config_signals [CHANGED] =
		gtk_signal_new ("changed",
				GTK_RUN_LAST,
				E_OBJECT_CLASS_TYPE (object_class),
				GTK_SIGNAL_OFFSET (ETableConfigClass, changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	E_OBJECT_CLASS_ADD_SIGNALS (object_class, e_table_config_signals, LAST_SIGNAL);

	gtk_object_add_arg_type ("ETableConfig::state", E_TABLE_STATE_TYPE,
				 GTK_ARG_READABLE, ARG_STATE);
}

static ETableColumnSpecification *
find_column_in_spec (ETableSpecification *spec, int model_col)
{
	ETableColumnSpecification **column;

	for (column = spec->columns; *column; column++){
		if ((*column)->disabled)
			continue;
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

		if ((*column)->disabled)
			continue;
		if (g_strcasecmp ((*column)->title, s) == 0)
			return (*column)->model_col;
	}
	return -1;
}

static void
update_sort_and_group_config_dialog (ETableConfig *config, gboolean is_sort)
{
	ETableConfigSortWidgets *widgets;
	int count, i;

	if (is_sort){
		count = e_table_sort_info_sorting_get_count (
			config->temp_state->sort_info);
		widgets = &config->sort [0];
	} else {
		count = e_table_sort_info_grouping_get_count (
			config->temp_state->sort_info);
		widgets = &config->group [0];
	}
	
	for (i = 0; i < 4; i++){
		gboolean sensitive = (i <= count);
		char *text = "";
		
		gtk_widget_set_sensitive (widgets [i].frames, sensitive);

		/*
		 * Sorting is set, auto select the text
		 */
		gtk_signal_handler_block (
			GTK_OBJECT (widgets [i].radio_ascending),
			widgets [i].toggled_id);
		gtk_signal_handler_block (
			GTK_OBJECT (widgets [i].combo->entry),
			widgets [i].changed_id);
		
		if (i < count){
			GtkToggleButton *a, *d;
			ETableSortColumn col =
				is_sort
				? e_table_sort_info_sorting_get_nth (
					config->temp_state->sort_info,
					i)
				:  e_table_sort_info_grouping_get_nth (
					config->temp_state->sort_info,
					i);
			
			ETableColumnSpecification *column =
				find_column_in_spec (config->source_spec, col.column);

			if (!column){
				/*
				 * This is a bug in the programmer
				 * stuff, but by the time we arrive
				 * here, the user has been given a
				 * warning
				 */
				continue;
			}

			text = gettext (column->title);

			/*
			 * Update radio buttons
			 */
			a = GTK_TOGGLE_BUTTON (
				widgets [i].radio_ascending);
			d = GTK_TOGGLE_BUTTON (
				widgets [i].radio_descending);
			
			gtk_toggle_button_set_active (col.ascending ? a:d, 1);
		} else {
			GtkToggleButton *t;
			
			t = GTK_TOGGLE_BUTTON (
				widgets [i].radio_ascending);

			if (is_sort)
				g_assert (widgets [i].radio_ascending != config->group [i].radio_ascending);
			else
				g_assert (widgets [i].radio_ascending != config->sort [i].radio_ascending);
			gtk_toggle_button_set_active (t, 1);
		}

		/* Set the text */
		gtk_combo_text_set_text (widgets [i].combo, text);

		gtk_signal_handler_unblock (
			GTK_OBJECT (widgets [i].radio_ascending),
			widgets [i].toggled_id);
		gtk_signal_handler_unblock (
			GTK_OBJECT (widgets [i].combo->entry),
			widgets [i].changed_id);
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
		ETableColumnSpecification *column;
		
		column = find_column_in_spec (config->source_spec, col.column);
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
config_group_info_update (ETableConfig *config)
{
	ETableSortInfo *info = config->state->sort_info;
	GString *res;
	int count, i;

	if (!e_table_sort_info_get_can_group (info))
		return;

	count = e_table_sort_info_grouping_get_count (info);
	res = g_string_new ("");

	for (i = 0; i < count; i++) {
		ETableSortColumn col = e_table_sort_info_grouping_get_nth (info, i);
		ETableColumnSpecification *column;

		column = find_column_in_spec (config->source_spec, col.column);
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
setup_fields (ETableConfig *config)
{
	int i;

	e_table_without_show_all (config->available_model);
	e_table_subset_variable_clear (config->shown_model);

	if (config->temp_state) {
		for (i = 0; i < config->temp_state->col_count; i++) {
			e_table_subset_variable_add (config->shown_model, config->temp_state->columns[i]);
			e_table_without_hide (config->available_model, (void *) config->temp_state->columns[i]);
		}
	}
}

static void
config_fields_info_update (ETableConfig *config)
{
	ETableColumnSpecification **column;
	GString *res = g_string_new ("");
	int i;

	for (i = 0; i < config->state->col_count; i++){
		for (column = config->source_spec->columns; *column; column++){

			if ((*column)->disabled)
				continue;

			if (config->state->columns [i] != (*column)->model_col)
				continue;
			
			g_string_append (res, gettext ((*column)->title));
			if (column [1])
				g_string_append (res, ", ");
		}
	}
	
	gtk_label_set_text (GTK_LABEL (config->fields_label), res->str);
	g_string_free (res, TRUE);
}

static void
do_sort_and_group_config_dialog (ETableConfig *config, gboolean is_sort)
{
	GnomeDialog *dialog;
	int button, running = 1;

	config->temp_state = e_table_state_duplicate (config->state);

	update_sort_and_group_config_dialog (config, is_sort);

	gtk_widget_grab_focus (GTK_WIDGET (
		is_sort
		? config->sort [0].combo
		: config->group [0].combo));
		

	if (is_sort)
		dialog = GNOME_DIALOG (config->dialog_sort);
	else
		dialog = GNOME_DIALOG (config->dialog_group_by);
	
	do {
		button = gnome_dialog_run (dialog);
		switch (button){
		case 0:
			if (is_sort){
				e_table_sort_info_sorting_truncate (
					config->temp_state->sort_info, 0);
			} else {
				e_table_sort_info_grouping_truncate (
					config->temp_state->sort_info, 0);
			}
			update_sort_and_group_config_dialog (config, is_sort);
			continue;

			/* OK */
		case 1:
			gtk_object_unref (GTK_OBJECT (config->state));
			config->state = config->temp_state;
			config->temp_state = 0;
			running = 0;
			gnome_property_box_changed (
				GNOME_PROPERTY_BOX (config->dialog_toplevel));
			break;

			/* CANCEL */
		case 2:
			gtk_object_unref (GTK_OBJECT (config->temp_state));
			config->temp_state = 0;
			running = 0;
			break;
		}
		
	} while (running);
	gnome_dialog_close (GNOME_DIALOG (dialog));

	if (is_sort)
		config_sort_info_update (config);
	else
		config_group_info_update (config);
}

static void
do_fields_config_dialog (ETableConfig *config)
{
	int button, running = 1;

	config->temp_state = e_table_state_duplicate (config->state);

	setup_fields (config);

	do {
		button = gnome_dialog_run (GNOME_DIALOG(config->dialog_show_fields));
		switch (button){
			/* OK */
		case 0:
			gtk_object_unref (GTK_OBJECT (config->state));
			config->state = config->temp_state;
			config->temp_state = 0;
			running = 0;
			gnome_property_box_changed (
				GNOME_PROPERTY_BOX (config->dialog_toplevel));
			break;

			/* CANCEL */
		case 1:
			gtk_object_unref (GTK_OBJECT (config->temp_state));
			config->temp_state = 0;
			running = 0;
			break;
		}
		
	} while (running);
	gnome_dialog_close (GNOME_DIALOG (config->dialog_show_fields));

	config_fields_info_update (config);
}


ETableMemoryStoreColumnInfo store_columns[] = {
	E_TABLE_MEMORY_STORE_STRING,
	E_TABLE_MEMORY_STORE_TERMINATOR
};

static ETableModel *global_store;  /* Glade better not be reentrant any time soon. */

static void
create_global_store (ETableConfig *config)
{
	int i;

	global_store = e_table_memory_store_new (store_columns);
	for (i = 0; config->source_spec->columns[i]; i++) {
		e_table_memory_store_insert (E_TABLE_MEMORY_STORE (global_store), i, NULL, config->source_spec->columns[i]->title);
	}
}

char *spec = "<ETableSpecification no-headers=\"true\" cursor-mode=\"line\" draw-grid=\"false\" draw-focus=\"true\" selection-mode=\"browse\"> \
  <ETableColumn model_col= \"0\" _title=\"Name\" minimum_width=\"30\" resizable=\"true\" cell=\"string\" compare=\"string\"/> \
  <ETableState> <column source=\"0\"/> \
  <grouping/> \
  </ETableState> \
  </ETableSpecification>";

GtkWidget *e_table_proxy_etable_shown_new (void);

GtkWidget *
e_table_proxy_etable_shown_new (void)
{
	ETableModel *model = NULL;

	model = e_table_subset_variable_new (global_store);

	return e_table_scrolled_new (model, NULL, spec, NULL);
}

GtkWidget *e_table_proxy_etable_available_new (void);

GtkWidget *
e_table_proxy_etable_available_new (void)
{
	ETableModel *model;

	model = e_table_without_new (global_store,
				     NULL, NULL, NULL, NULL, NULL, NULL, NULL);

	e_table_without_show_all (E_TABLE_WITHOUT (model));

	return e_table_scrolled_new (model, NULL, spec, NULL);
}

static void
config_button_fields (GtkWidget *widget, ETableConfig *config)
{
	do_fields_config_dialog (config);
}

static void
config_button_sort (GtkWidget *widget, ETableConfig *config)
{
	do_sort_and_group_config_dialog (config, TRUE);
}

static void
config_button_group (GtkWidget *widget, ETableConfig *config)
{
	do_sort_and_group_config_dialog (config, FALSE);
}

static void
dialog_destroyed (GtkObject *dialog, ETableConfig *config)
{
	gtk_object_destroy (GTK_OBJECT (config));
}

static void
dialog_apply (GnomePropertyBox *pbox, gint page_num, ETableConfig *config)
{
	if (page_num != -1)
		return;

	e_table_config_changed (config, config->state);
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

#if 0
static GtkWidget *
configure_dialog (GladeXML *gui, const char *widget_name, ETableConfig *config)
{
	GtkWidget *w;
	
	w = glade_xml_get_widget (gui, widget_name);

	return w;
}
#endif

static void
connect_button (ETableConfig *config, GladeXML *gui, const char *widget_name, GtkSignalFunc cback)
{
	GtkWidget *button = glade_xml_get_widget (gui, widget_name);

	if (button) {
		gtk_signal_connect (GTK_OBJECT (button), "clicked",
				    cback, config);
	}
}

static void
sort_entry_changed (GtkEntry *entry, ETableConfigSortWidgets *sort)
{
	ETableConfig *config = sort->e_table_config;
	ETableSortInfo *sort_info = config->temp_state->sort_info;
	ETableConfigSortWidgets *base = &config->sort[0];
	int idx = sort - base;
	
	char *s = gtk_entry_get_text (entry);

	if (s && s [0] && g_hash_table_lookup (sort->combo->elements, s)){
		ETableSortColumn c;
		int col;
		
		col = find_model_column_by_name (config->source_spec, s);
		if (col == -1){
			g_warning ("sort: This should not happen (%s)", s);
			return;
		}

		c.ascending = GTK_TOGGLE_BUTTON (
			config->sort [idx].radio_ascending)->active;
		c.column = col;
		e_table_sort_info_sorting_set_nth (sort_info, idx, c);
		  
		update_sort_and_group_config_dialog (config, TRUE);
	}  else {
		e_table_sort_info_sorting_truncate (sort_info, idx);
		update_sort_and_group_config_dialog (config, TRUE);
	}
}

static void
sort_ascending_toggled (GtkToggleButton *t, ETableConfigSortWidgets *sort)
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
	GSList *l;
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

	for (l = config->column_names; l; l = l->next){
		char *label = l->data;

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
			"changed", sort_entry_changed, &config->sort [i]);

		config->sort [i].toggled_id = gtk_signal_connect (
			GTK_OBJECT (config->sort [i].radio_ascending),
			"toggled", sort_ascending_toggled, &config->sort [i]);
	}
}

static void
group_entry_changed (GtkEntry *entry, ETableConfigSortWidgets *group)
{
	ETableConfig *config = group->e_table_config;
	ETableSortInfo *sort_info = config->temp_state->sort_info;
	ETableConfigSortWidgets *base = &config->group[0];
	int idx = group - base;
	char *s = gtk_entry_get_text (entry);

	if (s && s [0] && g_hash_table_lookup (group->combo->elements, s)){
		ETableSortColumn c;
		int col;
		
		col = find_model_column_by_name (config->source_spec, s);
		if (col == -1){
			g_warning ("grouping: this should not happen, %s", s);
			return;
		}

		c.ascending = GTK_TOGGLE_BUTTON (
			config->group [idx].radio_ascending)->active;
		c.column = col;
		e_table_sort_info_grouping_set_nth (sort_info, idx, c);
		  
		update_sort_and_group_config_dialog (config, FALSE);
	}  else {
		e_table_sort_info_grouping_truncate (sort_info, idx);
		update_sort_and_group_config_dialog (config, FALSE);
	}
}

static void
group_ascending_toggled (GtkToggleButton *t, ETableConfigSortWidgets *group)
{
	ETableConfig *config = group->e_table_config;
	ETableSortInfo *si = config->temp_state->sort_info;
	ETableConfigSortWidgets *base = &config->group[0];
	int idx = group - base;
	ETableSortColumn c;
	
	c = e_table_sort_info_grouping_get_nth (si, idx);
	c.ascending = t->active;
	e_table_sort_info_grouping_set_nth (si, idx, c);
}

static void
configure_group_dialog (ETableConfig *config, GladeXML *gui)
{
	GSList *l;
	int i;
	
	for (i = 0; i < 4; i++){
		char buffer [80];

		snprintf (buffer, sizeof (buffer), "group-combo-%d", i + 1);
		config->group [i].combo = GTK_COMBO_TEXT (
			glade_xml_get_widget (gui, buffer));

		gtk_combo_text_add_item (config->group [i].combo, "", "");

		snprintf (buffer, sizeof (buffer), "frame-group-%d", i + 1);
		config->group [i].frames = 
			glade_xml_get_widget (gui, buffer);

		snprintf (
			buffer, sizeof (buffer),
			"radiobutton-ascending-group-%d", i+1);
		config->group [i].radio_ascending = glade_xml_get_widget (
			gui, buffer);

		snprintf (
			buffer, sizeof (buffer),
			"radiobutton-descending-group-%d", i+1);
		config->group [i].radio_descending = glade_xml_get_widget (
			gui, buffer);

		snprintf (
			buffer, sizeof (buffer),
			"checkbutton-group-%d", i+1);
		config->group [i].view_check = glade_xml_get_widget (
			gui, buffer);
		
		config->group [i].e_table_config = config;
	}

	
	for (l = config->column_names; l; l = l->next){
		char *label = l->data;

		for (i = 0; i < 4; i++){
			gtk_combo_text_add_item (
				config->group [i].combo,
				gettext (label), label);
		}
	}

	/*
	 * After we have runtime modified things, signal connect
	 */
	for (i = 0; i < 4; i++){
		config->group [i].changed_id = gtk_signal_connect (
			GTK_OBJECT (config->group [i].combo->entry),
			"changed", group_entry_changed, &config->group [i]);

		config->group [i].toggled_id = gtk_signal_connect (
			GTK_OBJECT (config->group [i].radio_ascending),
			"toggled", group_ascending_toggled, &config->group [i]);
	}
}

static void
add_column (int model_row, gpointer closure)
{
	GList **columns = closure;
	*columns = g_list_prepend (*columns, GINT_TO_POINTER (model_row));
}

static void
config_button_add (GtkWidget *widget, ETableConfig *config)
{
	GList *columns = NULL;
	GList *column;
	int count;
	int i;

	e_table_selected_row_foreach (config->available, add_column, &columns);
	columns = g_list_reverse (columns);

	count = g_list_length (columns);

	config->temp_state->columns = g_renew (int, config->temp_state->columns, config->temp_state->col_count + count);
	config->temp_state->expansions = g_renew (double, config->temp_state->expansions, config->temp_state->col_count + count);
	i = config->temp_state->col_count;
	for (column = columns; column; column = column->next) {
		config->temp_state->columns[i] = e_table_subset_view_to_model_row (E_TABLE_SUBSET (config->available_model), GPOINTER_TO_INT (column->data));
		config->temp_state->expansions[i] = config->source_spec->columns[config->temp_state->columns[i]]->expansion;
		i++;
	}
	config->temp_state->col_count += count;

	g_list_free (columns);

	setup_fields (config);
}

static void
config_button_remove (GtkWidget *widget, ETableConfig *config)
{
	GList *columns = NULL;
	GList *column;

	e_table_selected_row_foreach (config->shown, add_column, &columns);

	for (column = columns; column; column = column->next) {
		int row = GPOINTER_TO_INT (column->data);

		memmove (config->temp_state->columns + row, config->temp_state->columns + row + 1, sizeof (int) * (config->temp_state->col_count - row - 1));
		memmove (config->temp_state->expansions + row, config->temp_state->expansions + row + 1, sizeof (double) * (config->temp_state->col_count - row - 1));
		config->temp_state->col_count --;
	}
	config->temp_state->columns = g_renew (int, config->temp_state->columns, config->temp_state->col_count);
	config->temp_state->expansions = g_renew (double, config->temp_state->expansions, config->temp_state->col_count);

	g_list_free (columns);

	setup_fields (config);
}

static void
config_button_up (GtkWidget *widget, ETableConfig *config)
{
	GList *columns = NULL;
	GList *column;
	int *new_shown;
	double *new_expansions;
	int next_col;
	double next_expansion;
	int i;

	e_table_selected_row_foreach (config->shown, add_column, &columns);
	columns = g_list_reverse (columns);

	new_shown = g_new (int, config->temp_state->col_count);
	new_expansions = g_new (double, config->temp_state->col_count);

	column = columns;

	next_col = config->temp_state->columns[0];
	next_expansion = config->temp_state->expansions[0];

	for (i = 1; i < config->temp_state->col_count; i++) {
		if (column && (GPOINTER_TO_INT (column->data) == i)) {
			new_expansions[i - 1] = config->temp_state->expansions[i];
			new_shown[i - 1] = config->temp_state->columns[i];
			column = column->next;
		} else {
			new_shown[i - 1] = next_col;
			next_col = config->temp_state->columns[i];

			new_expansions[i - 1] = next_expansion;
			next_expansion = config->temp_state->expansions[i];
		}
	}

	new_shown[i - 1] = next_col;
	new_expansions[i - 1] = next_expansion;

	g_free (config->temp_state->columns);
	g_free (config->temp_state->expansions);

	config->temp_state->columns = new_shown;
	config->temp_state->expansions = new_expansions;

	g_list_free (columns);

	setup_fields (config);
}

static void
config_button_down (GtkWidget *widget, ETableConfig *config)
{
	GList *columns = NULL;
	GList *column;
	int *new_shown;
	double *new_expansions;
	int next_col;
	double next_expansion;
	int i;

	e_table_selected_row_foreach (config->shown, add_column, &columns);

	new_shown = g_new (int, config->temp_state->col_count);
	new_expansions = g_new (double, config->temp_state->col_count);

	column = columns;

	next_col = config->temp_state->columns[config->temp_state->col_count - 1];
	next_expansion = config->temp_state->expansions[config->temp_state->col_count - 1];

	for (i = config->temp_state->col_count - 1; i > 0; i--) {
		if (column && (GPOINTER_TO_INT (column->data) == i - 1)) {
			new_expansions[i] = config->temp_state->expansions[i - 1];
			new_shown[i] = config->temp_state->columns[i - 1];
			column = column->next;
		} else {
			new_shown[i] = next_col;
			next_col = config->temp_state->columns[i - 1];

			new_expansions[i] = next_expansion;
			next_expansion = config->temp_state->expansions[i - 1];
		}
	}

	new_shown[0] = next_col;
	new_expansions[0] = next_expansion;

	g_free (config->temp_state->columns);
	g_free (config->temp_state->expansions);

	config->temp_state->columns = new_shown;
	config->temp_state->expansions = new_expansions;

	g_list_free (columns);

	setup_fields (config);
}

static void
configure_fields_dialog (ETableConfig *config, GladeXML *gui)
{
	config->available = e_table_scrolled_get_table (E_TABLE_SCROLLED (glade_xml_get_widget (gui, "custom-available")));
	gtk_object_get (GTK_OBJECT (config->available),
			"model", &config->available_model,
			NULL);

	config->shown = e_table_scrolled_get_table (E_TABLE_SCROLLED (glade_xml_get_widget (gui, "custom-shown")));
	gtk_object_get (GTK_OBJECT (config->shown),
			"model", &config->shown_model,
			NULL);

	connect_button (config, gui, "button-add",    config_button_add);
	connect_button (config, gui, "button-remove", config_button_remove);
	connect_button (config, gui, "button-up",     config_button_up);
	connect_button (config, gui, "button-down",   config_button_down);

	setup_fields (config);
}

static void
setup_gui (ETableConfig *config)
{
	GladeXML *gui;

	create_global_store (config);

	if (e_table_sort_info_get_can_group (config->state->sort_info)) {
		gui = glade_xml_new_with_domain (ETABLE_GLADEDIR "/e-table-config.glade", NULL, PACKAGE);
	} else {
		gui = glade_xml_new_with_domain (ETABLE_GLADEDIR "/e-table-config-no-group.glade", NULL, PACKAGE);
	}

	gtk_object_unref (GTK_OBJECT (global_store));
	
	config->dialog_toplevel = glade_xml_get_widget (
		gui, "e-table-config");

	if (config->header)
		gtk_window_set_title (GTK_WINDOW (config->dialog_toplevel), config->header);

	gtk_widget_hide (GNOME_PROPERTY_BOX(config->dialog_toplevel)->help_button);

	gtk_notebook_set_show_tabs (
		GTK_NOTEBOOK (GNOME_PROPERTY_BOX (
			config->dialog_toplevel)->notebook),
		FALSE);
	
	config->dialog_show_fields = glade_xml_get_widget (
		gui, "dialog-show-fields");
	config->dialog_group_by =  glade_xml_get_widget (
		gui, "dialog-group-by");
	config->dialog_sort = glade_xml_get_widget (
		gui, "dialog-sort");

	config->sort_label = glade_xml_get_widget (
		gui, "label-sort");
	config->group_label = glade_xml_get_widget (
		gui, "label-group");
	config->fields_label = glade_xml_get_widget (
		gui, "label-fields");

	connect_button (config, gui, "button-sort", config_button_sort);
	connect_button (config, gui, "button-group", config_button_group);
	connect_button (config, gui, "button-fields", config_button_fields);
	
	configure_sort_dialog (config, gui);
	configure_group_dialog (config, gui);
	configure_fields_dialog (config, gui);
	
	gtk_signal_connect (
		GTK_OBJECT (config->dialog_toplevel), "destroy",
		GTK_SIGNAL_FUNC (dialog_destroyed), config);

	gtk_signal_connect (
		GTK_OBJECT (config->dialog_toplevel), "apply",
		GTK_SIGNAL_FUNC (dialog_apply), config);

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
	ETableColumnSpecification **column;

	g_return_val_if_fail (config != NULL, NULL);
	g_return_val_if_fail (header != NULL, NULL);
	g_return_val_if_fail (spec != NULL, NULL);
	g_return_val_if_fail (state != NULL, NULL);
	
	config->source_spec = spec;
	config->source_state = state;
	config->header = g_strdup (header);

	gtk_object_ref (GTK_OBJECT (config->source_spec));
	gtk_object_ref (GTK_OBJECT (config->source_state));

	config->state = e_table_state_duplicate (state);

	for (column = config->source_spec->columns; *column; column++){
		char *label = (*column)->title;

		if ((*column)->disabled)
			continue;

		config->column_names = g_slist_append (
			config->column_names, label);
	}

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

E_MAKE_TYPE(e_table_config, "ETableConfig", ETableConfig, config_class_init, config_init, PARENT_TYPE)
