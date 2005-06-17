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

#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>
#include <libgnomeui/gnome-propertybox.h>
#include <glade/glade.h>

#include "e-util/e-i18n.h"
#include "e-util/e-util-private.h"
#include "e-util/e-util.h"
#include "widgets/misc/e-unicode.h"

#include "e-table-config.h"
#include "e-table-memory-store.h"
#include "e-table-scrolled.h"
#include "e-table-without.h"

static GObjectClass *config_parent_class;

enum {
	CHANGED,
	LAST_SIGNAL
};

enum {
	PROP_0,
	PROP_STATE,
};

static guint e_table_config_signals [LAST_SIGNAL] = { 0, };

static void
config_finalize (GObject *object)
{
	ETableConfig *config = E_TABLE_CONFIG (object);

	if (config->state)
		g_object_unref (config->state);
	config->state = NULL;

	if (config->source_state)
		g_object_unref (config->source_state);
	config->source_state = NULL;

	if (config->source_spec)
		g_object_unref (config->source_spec);
	config->source_spec = NULL;

	g_free (config->header);
	config->header = NULL;

	g_slist_free (config->column_names);
	config->column_names = NULL;

	g_free (config->domain);
	config->domain = NULL;

	config_parent_class->finalize (object);
}

static void
e_table_config_changed (ETableConfig *config, ETableState *state)
{
	g_return_if_fail (E_IS_TABLE_CONFIG (config));

	g_signal_emit(G_OBJECT(config), e_table_config_signals [CHANGED], 0, state);
}

static void
config_dialog_changed (ETableConfig *config)
{
	/* enable the apply/ok buttons */
	gtk_dialog_set_response_sensitive (GTK_DIALOG (config->dialog_toplevel),
					   GTK_RESPONSE_APPLY, TRUE);
	gtk_dialog_set_response_sensitive (GTK_DIALOG (config->dialog_toplevel),
					   GTK_RESPONSE_OK, TRUE);
}

static void
config_get_property (GObject *object,
		     guint prop_id,
		     GValue *value,
		     GParamSpec *pspec)
{
	ETableConfig *config = E_TABLE_CONFIG (object);

	switch (prop_id) {
	case PROP_STATE:
		g_value_set_object (value, config->state);
		break;
	default:
		break;
	}
}

static void
config_class_init (GObjectClass *object_class)
{
	ETableConfigClass *klass = E_TABLE_CONFIG_CLASS(object_class);

	config_parent_class   = g_type_class_peek_parent (klass);
	
	klass->changed        = NULL;

	object_class->finalize = config_finalize;
	object_class->get_property = config_get_property;

	e_table_config_signals [CHANGED] =
		g_signal_new ("changed",
			      E_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETableConfigClass, changed),
			      (GSignalAccumulator) NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	g_object_class_install_property (object_class, PROP_STATE,
					 g_param_spec_object ("state",
							      _("State"),
							      /*_( */"XXX blurb" /*)*/,
							      E_TABLE_STATE_TYPE,
							      G_PARAM_READABLE));

	glade_init ();
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
		g_signal_handler_block (
			widgets [i].radio_ascending,
			widgets [i].toggled_id);
		g_signal_handler_block (
			widgets [i].combo->entry,
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

			text = dgettext (config->domain, column->title);

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
		gal_combo_text_set_text (widgets [i].combo, text);

		g_signal_handler_unblock (
			widgets [i].radio_ascending,
			widgets [i].toggled_id);
		g_signal_handler_unblock (
			widgets [i].combo->entry,
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
		
		g_string_append (res, dgettext (config->domain, (column)->title));
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

		g_string_append (res, dgettext (config->domain, (column)->title));
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

	e_table_model_freeze (config->available_model);
	e_table_model_freeze (config->shown_model);
	e_table_without_show_all (config->available_model);
	e_table_subset_variable_clear (config->shown_model);

	if (config->temp_state) {
		for (i = 0; i < config->temp_state->col_count; i++) {
			gint j, idx;
			for (j = 0, idx = 0; j < config->temp_state->columns[i]; j++)
				if (!config->source_spec->columns[j]->disabled)
					idx++;

			e_table_subset_variable_add (config->shown_model, idx);
			e_table_without_hide (config->available_model, GINT_TO_POINTER(idx));
		}
	}
	e_table_model_thaw (config->available_model);
	e_table_model_thaw (config->shown_model);
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

			g_string_append (res, dgettext (config->domain, (*column)->title));
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
	GtkDialog *dialog;
	int response, running = 1;

	config->temp_state = e_table_state_duplicate (config->state);

	update_sort_and_group_config_dialog (config, is_sort);

	gtk_widget_grab_focus (GTK_WIDGET (
		is_sort
		? config->sort [0].combo
		: config->group [0].combo));
		

	if (is_sort)
		dialog = GTK_DIALOG (config->dialog_sort);
	else
		dialog = GTK_DIALOG (config->dialog_group_by);
	
	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (config->dialog_toplevel));

	do {
		response = gtk_dialog_run (dialog);
		switch (response){
		case 0: /* clear fields */
			if (is_sort){
				e_table_sort_info_sorting_truncate (
					config->temp_state->sort_info, 0);
			} else {
				e_table_sort_info_grouping_truncate (
					config->temp_state->sort_info, 0);
			}
			update_sort_and_group_config_dialog (config, is_sort);
			break;

		case GTK_RESPONSE_OK:
			g_object_unref (config->state);
			config->state = config->temp_state;
			config->temp_state = 0;
			running = 0;
			config_dialog_changed (config);
			break;

		case GTK_RESPONSE_DELETE_EVENT:
		case GTK_RESPONSE_CANCEL:
			g_object_unref (config->temp_state);
			config->temp_state = 0;
			running = 0;
			break;
		}
		
	} while (running);
	gtk_widget_hide (GTK_WIDGET (dialog));
	
	if (is_sort)
		config_sort_info_update (config);
	else
		config_group_info_update (config);
}

static void
do_fields_config_dialog (ETableConfig *config)
{
	int response, running = 1;

	gtk_widget_ensure_style (config->dialog_show_fields);
	gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (config->dialog_show_fields)->vbox), 0);
	gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (config->dialog_show_fields)->action_area), 12);

	config->temp_state = e_table_state_duplicate (config->state);

	setup_fields (config);

	gtk_window_set_transient_for (GTK_WINDOW (config->dialog_show_fields),
				      GTK_WINDOW (config->dialog_toplevel));

	do {
		response = gtk_dialog_run (GTK_DIALOG(config->dialog_show_fields));
		switch (response){
		case GTK_RESPONSE_OK:
			g_object_unref (config->state);
			config->state = config->temp_state;
			config->temp_state = 0;
			running = 0;
			config_dialog_changed (config);
			break;

		case GTK_RESPONSE_DELETE_EVENT:
		case GTK_RESPONSE_CANCEL:
			g_object_unref (config->temp_state);
			config->temp_state = 0;
			running = 0;
			break;
		}
		
	} while (running);
	gtk_widget_hide (GTK_WIDGET (config->dialog_show_fields));

	config_fields_info_update (config);
}


ETableMemoryStoreColumnInfo store_columns[] = {
	E_TABLE_MEMORY_STORE_STRING,
	E_TABLE_MEMORY_STORE_INTEGER,
	E_TABLE_MEMORY_STORE_TERMINATOR
};

static ETableModel *global_store;  /* Glade better not be reentrant any time soon. */

static void
create_global_store (ETableConfig *config)
{
	int i;

	global_store = e_table_memory_store_new (store_columns);
	for (i = 0; config->source_spec->columns[i]; i++) {

		char *text;

		if (config->source_spec->columns[i]->disabled)
			continue;

		text = g_strdup (dgettext (config->domain, config->source_spec->columns[i]->title));
		e_table_memory_store_insert_adopt (E_TABLE_MEMORY_STORE (global_store), -1, NULL, text, i);
	}
}

char *spec = "<ETableSpecification gettext-domain=\"" E_I18N_DOMAIN "\" no-headers=\"true\" cursor-mode=\"line\" "
    " draw-grid=\"false\" draw-focus=\"true\" selection-mode=\"browse\">"
  "<ETableColumn model_col= \"0\" _title=\"Name\" minimum_width=\"30\" resizable=\"true\" cell=\"string\" compare=\"string\"/>"
  "<ETableState> <column source=\"0\"/>"
  "<grouping/>"
  "</ETableState>"
  "</ETableSpecification>";

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
dialog_destroyed (gpointer data, GObject *where_object_was)
{
	ETableConfig *config = data;
	g_object_unref (config);
}

static void
dialog_response (GtkWidget *dialog, int response_id, ETableConfig *config)
{
	if (response_id == GTK_RESPONSE_APPLY
	    || response_id == GTK_RESPONSE_OK) {
		e_table_config_changed (config, config->state);
	}

	if (response_id == GTK_RESPONSE_CANCEL
	    || response_id == GTK_RESPONSE_OK) {
		gtk_widget_destroy (dialog);
	}
}	

/*
 * Invoked by the Glade auto-connect code
 */
GtkWidget *e_table_proxy_gtk_combo_text_new (void);
GtkWidget *
e_table_proxy_gtk_combo_text_new (void)
{
	return gal_combo_text_new (TRUE);
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
connect_button (ETableConfig *config, GladeXML *gui, const char *widget_name, GCallback cback)
{
	GtkWidget *button = glade_xml_get_widget (gui, widget_name);

	if (button) {
		g_signal_connect (G_OBJECT (button), "clicked",
				  cback, config);
	}
}

static gint
get_source_model_col_index (ETableConfig *config, gint idx)
{
	gint visible_index;
	ETableModel *src_model = E_TABLE_SUBSET (config->available_model)->source;

        visible_index = e_table_subset_view_to_model_row (E_TABLE_SUBSET (config->available_model), idx);
	
	return GPOINTER_TO_INT (e_table_model_value_at (src_model, 1, visible_index));
}

static void
sort_entry_changed (GtkEntry *entry, ETableConfigSortWidgets *sort)
{
	ETableConfig *config = sort->e_table_config;
	ETableSortInfo *sort_info = config->temp_state->sort_info;
	ETableConfigSortWidgets *base = &config->sort[0];
	int idx = sort - base;
	
	const char *s = gtk_entry_get_text (entry);

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
		config->sort [i].combo = GAL_COMBO_TEXT (
			glade_xml_get_widget (gui, buffer));
		gtk_widget_show (GTK_WIDGET (config->sort [i].combo));
		gal_combo_text_add_item (config->sort [i].combo, "", "");

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
			gal_combo_text_add_item (config->sort [i].combo,
						 dgettext (config->domain, label), label);
		}
	}

	/*
	 * After we have runtime modified things, signal connect
	 */
	for (i = 0; i < 4; i++){
		config->sort [i].changed_id = g_signal_connect (
			config->sort [i].combo->entry,
			"changed", G_CALLBACK (sort_entry_changed),
			&config->sort [i]);

		config->sort [i].toggled_id = g_signal_connect (
			config->sort [i].radio_ascending,
			"toggled", G_CALLBACK (sort_ascending_toggled),
			&config->sort [i]);
	}
}

static void
group_entry_changed (GtkEntry *entry, ETableConfigSortWidgets *group)
{
	ETableConfig *config = group->e_table_config;
	ETableSortInfo *sort_info = config->temp_state->sort_info;
	ETableConfigSortWidgets *base = &config->group[0];
	int idx = group - base;
	const char *s = gtk_entry_get_text (entry);

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
		config->group [i].combo = GAL_COMBO_TEXT (
			glade_xml_get_widget (gui, buffer));
		gtk_widget_show (GTK_WIDGET (config->group [i].combo));

		gal_combo_text_add_item (config->group [i].combo, "", "");

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
			gal_combo_text_add_item (
				config->group [i].combo,
				dgettext (config->domain, label), label);
		}
	}

	/*
	 * After we have runtime modified things, signal connect
	 */
	for (i = 0; i < 4; i++){
		config->group [i].changed_id = g_signal_connect (
			config->group [i].combo->entry,
			"changed", G_CALLBACK (group_entry_changed),
			&config->group [i]);

		config->group [i].toggled_id = g_signal_connect (
			config->group [i].radio_ascending,
			"toggled", G_CALLBACK (group_ascending_toggled),
			&config->group [i]);
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
		config->temp_state->columns[i] = get_source_model_col_index (config, GPOINTER_TO_INT (column->data));
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

	/* if no columns left, just return */
	if (columns == NULL)
		return;

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

	/* if no columns left, just return */
	if (columns == NULL)
		return;


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
	GtkWidget *scrolled;

	scrolled = glade_xml_get_widget (gui, "custom-available");
	config->available = e_table_scrolled_get_table (E_TABLE_SCROLLED (scrolled));
	g_object_get (config->available,
		      "model", &config->available_model,
		      NULL);
	gtk_widget_show_all (scrolled);

	scrolled = glade_xml_get_widget (gui, "custom-shown");
	config->shown = e_table_scrolled_get_table (E_TABLE_SCROLLED (scrolled));
	g_object_get (config->shown,
		      "model", &config->shown_model,
		      NULL);
	gtk_widget_show_all (scrolled);

	connect_button (config, gui, "button-add",    G_CALLBACK (config_button_add));
	connect_button (config, gui, "button-remove", G_CALLBACK (config_button_remove));
	connect_button (config, gui, "button-up",     G_CALLBACK (config_button_up));
	connect_button (config, gui, "button-down",   G_CALLBACK (config_button_down));

	setup_fields (config);
}

static void
setup_gui (ETableConfig *config)
{
	GladeXML *gui;

	create_global_store (config);

	if (e_table_sort_info_get_can_group (config->state->sort_info)) {
		gchar *filename =
			g_build_filename (EVOLUTION_GLADEDIR,
					  "e-table-config.glade",
					  NULL);
		gui = glade_xml_new (filename, NULL, E_I18N_DOMAIN);
		g_free (filename);
	} else {
		gchar *filename =
			g_build_filename (EVOLUTION_GLADEDIR,
					  "e-table-config-no-group.glade",
					  NULL);
		gui = glade_xml_new (filename, NULL, E_I18N_DOMAIN);
		g_free (filename);
	}

	g_object_unref (global_store);
	
	config->dialog_toplevel = glade_xml_get_widget (
		gui, "e-table-config");

	if (config->header)
		gtk_window_set_title (GTK_WINDOW (config->dialog_toplevel), config->header);

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

	connect_button (config, gui, "button-sort", G_CALLBACK (config_button_sort));
	connect_button (config, gui, "button-group", G_CALLBACK (config_button_group));
	connect_button (config, gui, "button-fields", G_CALLBACK (config_button_fields));
	
	configure_sort_dialog (config, gui);
	configure_group_dialog (config, gui);
	configure_fields_dialog (config, gui);

	g_object_weak_ref (G_OBJECT (config->dialog_toplevel),
			   dialog_destroyed, config);

	g_signal_connect (config->dialog_toplevel, "response",
			  G_CALLBACK (dialog_response), config);

	g_object_unref (gui);
}

static void
config_init (ETableConfig *config)
{
	config->domain = NULL;
}

ETableConfig *
e_table_config_construct (ETableConfig        *config,
			  const char          *header,
			  ETableSpecification *spec,
			  ETableState         *state,
			  GtkWindow           *parent_window)
{
	ETableColumnSpecification **column;

	g_return_val_if_fail (config != NULL, NULL);
	g_return_val_if_fail (header != NULL, NULL);
	g_return_val_if_fail (spec != NULL, NULL);
	g_return_val_if_fail (state != NULL, NULL);
	
	config->source_spec = spec;
	config->source_state = state;
	config->header = g_strdup (header);

	g_object_ref (config->source_spec);
	g_object_ref (config->source_state);

	config->state = e_table_state_duplicate (state);

	config->domain = g_strdup (spec->domain);

	for (column = config->source_spec->columns; *column; column++){
		char *label = (*column)->title;

		if ((*column)->disabled)
			continue;

		config->column_names = g_slist_append (
			config->column_names, label);
	}

	setup_gui (config);

	gtk_window_set_transient_for (GTK_WINDOW (config->dialog_toplevel),
				      parent_window);

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
		    ETableState         *state,
		    GtkWindow           *parent_window)
{
	ETableConfig *config = g_object_new (E_TABLE_CONFIG_TYPE, NULL);

	if (e_table_config_construct (config, header, spec, state, parent_window) == NULL){
		g_object_unref (config);
		return NULL;
	}

	gtk_widget_ensure_style (config->dialog_toplevel);
	gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (config->dialog_toplevel)->vbox), 0);
	gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (config->dialog_toplevel)->action_area), 12);

	gtk_dialog_set_response_sensitive (GTK_DIALOG (config->dialog_toplevel),
					   GTK_RESPONSE_APPLY, FALSE);
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

E_MAKE_TYPE(e_table_config, "ETableConfig", ETableConfig, config_class_init, config_init, G_TYPE_OBJECT)
