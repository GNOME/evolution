/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *		Miguel de Icaza <miguel@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/*
 * FIXME:
 *    Sort Dialog: when text is selected, the toggle button switches state.
 *    Make Clear all work.
 */

#include "evolution-config.h"

#include <glib/gi18n.h>

#include "e-table-column-selector.h"
#include "e-table-config.h"

G_DEFINE_TYPE (ETableConfig, e_table_config, G_TYPE_OBJECT)

enum {
	CHANGED,
	LAST_SIGNAL
};

enum {
	PROP_0,
	PROP_STATE
};

enum {
	COLUMN_ITEM,
	COLUMN_VALUE
};

static guint e_table_config_signals[LAST_SIGNAL] = { 0, };

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

	G_OBJECT_CLASS (e_table_config_parent_class)->finalize (object);
}

static void
e_table_config_changed (ETableConfig *config,
                        ETableState *state)
{
	g_return_if_fail (E_IS_TABLE_CONFIG (config));

	g_signal_emit (config, e_table_config_signals[CHANGED], 0, state);
}

static void
config_dialog_changed (ETableConfig *config)
{
	/* enable the apply/ok buttons */
	gtk_dialog_set_response_sensitive (
		GTK_DIALOG (config->dialog_toplevel),
		GTK_RESPONSE_APPLY, TRUE);
	gtk_dialog_set_response_sensitive (
		GTK_DIALOG (config->dialog_toplevel),
		GTK_RESPONSE_OK, TRUE);
}

static void
config_get_property (GObject *object,
                     guint property_id,
                     GValue *value,
                     GParamSpec *pspec)
{
	ETableConfig *config = E_TABLE_CONFIG (object);

	switch (property_id) {
	case PROP_STATE:
		g_value_set_object (value, config->state);
		break;
	default:
		break;
	}
}

static void
e_table_config_class_init (ETableConfigClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	class->changed = NULL;

	object_class->finalize = config_finalize;
	object_class->get_property = config_get_property;

	e_table_config_signals[CHANGED] = g_signal_new (
		"changed",
		G_TYPE_FROM_CLASS (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETableConfigClass, changed),
		(GSignalAccumulator) NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	g_object_class_install_property (
		object_class,
		PROP_STATE,
		g_param_spec_object (
			"state",
			"State",
			NULL,
			E_TYPE_TABLE_STATE,
			G_PARAM_READABLE));
}

static void
configure_combo_box_add (GtkComboBox *combo_box,
                         const gchar *item,
                         const gchar *value)
{
	GtkTreeRowReference *reference;
	GtkTreeModel *model;
	GtkTreePath *path;
	GHashTable *index;
	GtkTreeIter iter;

	model = gtk_combo_box_get_model (combo_box);
	gtk_list_store_append (GTK_LIST_STORE (model), &iter);

	gtk_list_store_set (
		GTK_LIST_STORE (model), &iter,
		COLUMN_ITEM, item, COLUMN_VALUE, value, -1);

	index = g_object_get_data (G_OBJECT (combo_box), "index");
	g_return_if_fail (index != NULL);

	/* Add an entry to the tree model index. */
	path = gtk_tree_model_get_path (model, &iter);
	reference = gtk_tree_row_reference_new (model, path);
	g_return_if_fail (reference != NULL);
	g_hash_table_insert (index, g_strdup (value), reference);
	gtk_tree_path_free (path);
}

static gchar *
configure_combo_box_get_active (GtkComboBox *combo_box)
{
	GtkTreeIter iter;
	gchar *value = NULL;

	if (gtk_combo_box_get_active_iter (combo_box, &iter))
		gtk_tree_model_get (
			gtk_combo_box_get_model (combo_box), &iter,
			COLUMN_VALUE, &value, -1);

	if (value != NULL && *value == '\0') {
		g_free (value);
		value = NULL;
	}

	return value;
}

static void
configure_combo_box_set_active (GtkComboBox *combo_box,
                                const gchar *value)
{
	GtkTreeRowReference *reference;
	GHashTable *index;

	index = g_object_get_data (G_OBJECT (combo_box), "index");
	g_return_if_fail (index != NULL);

	reference = g_hash_table_lookup (index, value);
	if (reference != NULL) {
		GtkTreeModel *model;
		GtkTreePath *path;
		GtkTreeIter iter;

		model = gtk_tree_row_reference_get_model (reference);
		path = gtk_tree_row_reference_get_path (reference);

		if (path == NULL)
			return;

		if (gtk_tree_model_get_iter (model, &iter, path))
			gtk_combo_box_set_active_iter (combo_box, &iter);

		gtk_tree_path_free (path);
	}
}

static ETableColumnSpecification *
find_column_spec_by_name (ETableSpecification *spec,
                          const gchar *s)
{
	ETableColumnSpecification *column = NULL;
	GPtrArray *array;
	guint ii;

	array = e_table_specification_ref_columns (spec);

	for (ii = 0; ii < array->len; ii++) {
		ETableColumnSpecification *candidate;

		candidate = g_ptr_array_index (array, ii);

		if (candidate->disabled)
			continue;

		if (g_ascii_strcasecmp (candidate->title, s) == 0) {
			column = candidate;
			break;
		}
	}

	g_ptr_array_unref (array);

	return column;
}

static void
update_sort_and_group_config_dialog (ETableConfig *config,
                                     gboolean is_sort)
{
	ETableConfigSortWidgets *widgets;
	gint count, i;

	if (is_sort) {
		count = e_table_sort_info_sorting_get_count (
			config->temp_state->sort_info);
		widgets = &config->sort[0];
	} else {
		count = e_table_sort_info_grouping_get_count (
			config->temp_state->sort_info);
		widgets = &config->group[0];
	}

	for (i = 0; i < 4; i++) {
		gboolean sensitive = (i <= count);
		const gchar *text = "";

		gtk_widget_set_sensitive (widgets[i].frames, sensitive);

		/*
		 * Sorting is set, auto select the text
		 */
		g_signal_handler_block (
			widgets[i].radio_ascending,
			widgets[i].toggled_id);
		g_signal_handler_block (
			widgets[i].combo,
			widgets[i].changed_id);

		if (i < count) {
			GtkWidget *toggle_button;
			ETableColumnSpecification *column;
			GtkSortType sort_type;

			if (is_sort)
				column = e_table_sort_info_sorting_get_nth (
					config->temp_state->sort_info, i,
					&sort_type);
			else
				column = e_table_sort_info_grouping_get_nth (
					config->temp_state->sort_info, i,
					&sort_type);

			if (column == NULL) {
				/*
				 * This is a bug in the programmer
				 * stuff, but by the time we arrive
				 * here, the user has been given a
				 * warning
				 */
				continue;
			}

			text = column->title;

			/*
			 * Update radio buttons
			 */

			if (sort_type == GTK_SORT_ASCENDING)
				toggle_button = widgets[i].radio_ascending;
			else
				toggle_button = widgets[i].radio_descending;

			gtk_toggle_button_set_active (
				GTK_TOGGLE_BUTTON (toggle_button), TRUE);

		} else {
			GtkToggleButton *t;

			t = GTK_TOGGLE_BUTTON (
				widgets[i].radio_ascending);

			if (is_sort)
				g_return_if_fail (
					widgets[i].radio_ascending !=
					config->group[i].radio_ascending);
			else
				g_return_if_fail (
					widgets[i].radio_ascending !=
					config->sort[i].radio_ascending);
			gtk_toggle_button_set_active (t, 1);
		}

		/* Set the text */
		configure_combo_box_set_active (
			GTK_COMBO_BOX (widgets[i].combo), text);

		g_signal_handler_unblock (
			widgets[i].radio_ascending,
			widgets[i].toggled_id);
		g_signal_handler_unblock (
			widgets[i].combo,
			widgets[i].changed_id);
	}
}

static void
config_sort_info_update (ETableConfig *config)
{
	ETableSortInfo *sort_info;
	GString *res;
	gint count, i;

	sort_info = config->state->sort_info;

	count = e_table_sort_info_sorting_get_count (sort_info);
	res = g_string_new ("");

	for (i = 0; i < count; i++) {
		ETableColumnSpecification *column;
		GtkSortType sort_type;

		column = e_table_sort_info_sorting_get_nth (
			sort_info, i, &sort_type);

		if (column == NULL) {
			g_warning ("Could not find column model in specification");
			continue;
		}

		g_string_append (res, dgettext (config->domain, (column)->title));
		g_string_append_c (res, ' ');
		g_string_append (
			res,
			(sort_type == GTK_SORT_ASCENDING) ?
			_("(Ascending)") : _("(Descending)"));

		if ((i + 1) != count)
			g_string_append (res, ", ");
	}

	if (res->str[0] == 0)
		g_string_append (res, _("Not sorted"));

	gtk_label_set_text (GTK_LABEL (config->sort_label), res->str);

	g_string_free (res, TRUE);
}

static void
config_group_info_update (ETableConfig *config)
{
	ETableSortInfo *sort_info;
	GString *res;
	gint count, i;

	sort_info = config->state->sort_info;

	if (!e_table_sort_info_get_can_group (sort_info))
		return;

	count = e_table_sort_info_grouping_get_count (sort_info);
	res = g_string_new ("");

	for (i = 0; i < count; i++) {
		ETableColumnSpecification *column;
		GtkSortType sort_type;

		column = e_table_sort_info_grouping_get_nth (
			sort_info, i, &sort_type);

		if (column == NULL) {
			g_warning ("Could not find model column in specification");
			continue;
		}

		g_string_append (res, dgettext (config->domain, (column)->title));
		g_string_append_c (res, ' ');
		g_string_append (
			res,
			(sort_type == GTK_SORT_ASCENDING) ?
			_("(Ascending)") : _("(Descending)"));

		if ((i + 1) != count)
			g_string_append (res, ", ");
	}
	if (res->str[0] == 0)
		g_string_append (res, _("No grouping"));

	gtk_label_set_text (GTK_LABEL (config->group_label), res->str);
	g_string_free (res, TRUE);
}

static void
config_fields_info_update (ETableConfig *config)
{
	GString *res = g_string_new ("");
	gint ii;

	for (ii = 0; ii < config->state->col_count; ii++) {
		ETableColumnSpecification *column;
		const gchar *title;

		column = config->state->column_specs[ii];

		if (column->disabled)
			continue;

		title = dgettext (config->domain, column->title);
		g_string_append (res, title);

		if (ii + 1 < config->state->col_count)
			g_string_append (res, ", ");
	}

	gtk_label_set_text (GTK_LABEL (config->fields_label), res->str);
	g_string_free (res, TRUE);
}

static void
do_sort_and_group_config_dialog (ETableConfig *config,
                                 gboolean is_sort)
{
	GtkDialog *dialog;
	gint response, running = 1;

	config->temp_state = e_table_state_duplicate (config->state);

	update_sort_and_group_config_dialog (config, is_sort);

	gtk_widget_grab_focus (GTK_WIDGET (
		is_sort
		? config->sort[0].combo
		: config->group[0].combo));

	if (is_sort)
		dialog = GTK_DIALOG (config->dialog_sort);
	else
		dialog = GTK_DIALOG (config->dialog_group_by);

	gtk_window_set_transient_for (
		GTK_WINDOW (dialog), GTK_WINDOW (config->dialog_toplevel));

	do {
		response = gtk_dialog_run (dialog);
		switch (response) {
		case 0: /* clear fields */
			if (is_sort) {
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
			config->temp_state = NULL;
			running = 0;
			config_dialog_changed (config);
			break;

		case GTK_RESPONSE_DELETE_EVENT:
		case GTK_RESPONSE_CANCEL:
			g_object_unref (config->temp_state);
			config->temp_state = NULL;
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
	GtkWidget *dialog;
	GtkWidget *content_area;
	GtkWidget *selector;
	GtkWidget *label;
	gint response, running = 1;

	dialog = gtk_dialog_new_with_buttons (
		_("Show Fields"),
		GTK_WINDOW (config->dialog_toplevel),
		0, /* no flags */
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		_("_OK"), GTK_RESPONSE_OK,
		NULL);

	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	gtk_window_set_default_size (GTK_WINDOW (dialog), 300, 400);

	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	gtk_box_set_spacing (GTK_BOX (content_area), 6);

	label = gtk_label_new (
		_("Choose the order of information "
		"to appear in the message list."));
	gtk_box_pack_start (GTK_BOX (content_area), label, FALSE, FALSE, 0);
	gtk_widget_show (label);

	selector = e_table_column_selector_new (config->state);
	gtk_container_set_border_width (GTK_CONTAINER (selector), 5);
	gtk_box_pack_start (GTK_BOX (content_area), selector, TRUE, TRUE, 0);
	gtk_widget_show (selector);

	do {
		response = gtk_dialog_run (GTK_DIALOG (dialog));
		switch (response) {
		case GTK_RESPONSE_OK:
			e_table_column_selector_apply (
				E_TABLE_COLUMN_SELECTOR (selector));
			running = 0;
			config_dialog_changed (config);
			break;

		case GTK_RESPONSE_DELETE_EVENT:
		case GTK_RESPONSE_CANCEL:
			running = 0;
			break;
		}

	} while (running);

	gtk_widget_destroy (dialog);

	config_fields_info_update (config);
}

static void
config_button_fields (GtkWidget *widget,
                      ETableConfig *config)
{
	do_fields_config_dialog (config);
}

static void
config_button_sort (GtkWidget *widget,
                    ETableConfig *config)
{
	do_sort_and_group_config_dialog (config, TRUE);
}

static void
config_button_group (GtkWidget *widget,
                     ETableConfig *config)
{
	do_sort_and_group_config_dialog (config, FALSE);
}

static void
dialog_destroyed (gpointer data,
                  GObject *where_object_was)
{
	ETableConfig *config = data;
	g_object_unref (config);
}

static void
dialog_response (GtkWidget *dialog,
                 gint response_id,
                 ETableConfig *config)
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
 * Invoked by the GtkBuilder auto-connect code
 */
static GtkWidget *
e_table_proxy_gtk_combo_text_new (void)
{
	GtkCellRenderer *renderer;
	GtkListStore *store;
	GtkWidget *combo_box;
	GHashTable *index;

	store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
	combo_box = gtk_combo_box_new_with_model (GTK_TREE_MODEL (store));

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (
		GTK_CELL_LAYOUT (combo_box), renderer, FALSE);
	gtk_cell_layout_add_attribute (
		GTK_CELL_LAYOUT (combo_box), renderer, "text", COLUMN_ITEM);

	/* Embed a reverse-lookup index into the widget. */
	index = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) gtk_tree_row_reference_free);
	g_object_set_data_full (
		G_OBJECT (combo_box), "index", index,
		(GDestroyNotify) g_hash_table_destroy);

	return combo_box;
}

static void
connect_button (ETableConfig *config,
                GtkBuilder *builder,
                const gchar *widget_name,
                GCallback cback)
{
	GtkWidget *button = e_builder_get_widget (builder, widget_name);

	if (button)
		g_signal_connect (button, "clicked", cback, config);
}

static void
sort_combo_changed (GtkComboBox *combo_box,
                    ETableConfigSortWidgets *sort)
{
	ETableConfig *config = sort->e_table_config;
	ETableSortInfo *sort_info = config->temp_state->sort_info;
	ETableConfigSortWidgets *base = &config->sort[0];
	gint idx = sort - base;
	gchar *s;

	s = configure_combo_box_get_active (combo_box);

	if (s != NULL) {
		ETableColumnSpecification *column;
		GtkToggleButton *toggle_button;
		GtkSortType sort_type;

		column = find_column_spec_by_name (config->source_spec, s);
		if (column == NULL) {
			g_warning ("sort: This should not happen (%s)", s);
			g_free (s);
			return;
		}

		toggle_button = GTK_TOGGLE_BUTTON (
			config->sort[idx].radio_ascending);
		if (gtk_toggle_button_get_active (toggle_button))
			sort_type = GTK_SORT_ASCENDING;
		else
			sort_type = GTK_SORT_DESCENDING;

		e_table_sort_info_sorting_set_nth (
			sort_info, idx, column, sort_type);

		update_sort_and_group_config_dialog (config, TRUE);
	}  else {
		e_table_sort_info_sorting_truncate (sort_info, idx);
		update_sort_and_group_config_dialog (config, TRUE);
	}

	g_free (s);
}

static void
sort_ascending_toggled (GtkToggleButton *toggle_button,
                        ETableConfigSortWidgets *sort)
{
	ETableConfig *config = sort->e_table_config;
	ETableSortInfo *si = config->temp_state->sort_info;
	ETableConfigSortWidgets *base = &config->sort[0];
	ETableColumnSpecification *column;
	GtkSortType sort_type;
	gint idx = sort - base;

	if (gtk_toggle_button_get_active (toggle_button))
		sort_type = GTK_SORT_ASCENDING;
	else
		sort_type = GTK_SORT_DESCENDING;

	column = e_table_sort_info_sorting_get_nth (si, idx, NULL);
	e_table_sort_info_sorting_set_nth (si, idx, column, sort_type);
}

static void
configure_sort_dialog (ETableConfig *config,
                       GtkBuilder *builder)
{
	GSList *l;
	gint i;

	const gchar *algs[] = {
		"alignment4",
		"alignment3",
		"alignment2",
		"alignment1",
		NULL
	};

	for (i = 0; i < 4; i++) {
		gchar buffer[80];

		snprintf (buffer, sizeof (buffer), "sort-combo-%d", i + 1);
		config->sort[i].combo = e_table_proxy_gtk_combo_text_new ();
		gtk_widget_show (GTK_WIDGET (config->sort[i].combo));
		gtk_container_add (
			GTK_CONTAINER (e_builder_get_widget (
			builder, algs[i])), config->sort[i].combo);
		configure_combo_box_add (
			GTK_COMBO_BOX (config->sort[i].combo), "", "");

		snprintf (buffer, sizeof (buffer), "frame-sort-%d", i + 1);
		config->sort[i].frames =
			e_builder_get_widget (builder, buffer);

		snprintf (
			buffer, sizeof (buffer),
			"radiobutton-ascending-sort-%d", i + 1);
		config->sort[i].radio_ascending = e_builder_get_widget (
			builder, buffer);

		snprintf (
			buffer, sizeof (buffer),
			"radiobutton-descending-sort-%d", i + 1);
		config->sort[i].radio_descending = e_builder_get_widget (
			builder, buffer);

		config->sort[i].e_table_config = config;
	}

	for (l = config->column_names; l; l = l->next) {
		gchar *label = l->data;

		for (i = 0; i < 4; i++) {
			configure_combo_box_add (
				GTK_COMBO_BOX (config->sort[i].combo),
				dgettext (config->domain, label), label);
		}
	}

	/*
	 * After we have runtime modified things, signal connect
	 */
	for (i = 0; i < 4; i++) {
		config->sort[i].changed_id = g_signal_connect (
			config->sort[i].combo,
			"changed", G_CALLBACK (sort_combo_changed),
			&config->sort[i]);

		config->sort[i].toggled_id = g_signal_connect (
			config->sort[i].radio_ascending,
			"toggled", G_CALLBACK (sort_ascending_toggled),
			&config->sort[i]);
	}
}

static void
group_combo_changed (GtkComboBox *combo_box,
                     ETableConfigSortWidgets *group)
{
	ETableConfig *config = group->e_table_config;
	ETableSortInfo *sort_info = config->temp_state->sort_info;
	ETableConfigSortWidgets *base = &config->group[0];
	gint idx = group - base;
	gchar *s;

	s = configure_combo_box_get_active (combo_box);

	if (s != NULL) {
		ETableColumnSpecification *column;
		GtkToggleButton *toggle_button;
		GtkSortType sort_type;

		column = find_column_spec_by_name (config->source_spec, s);
		if (column == NULL) {
			g_warning ("grouping: this should not happen, %s", s);
			g_free (s);
			return;
		}

		toggle_button = GTK_TOGGLE_BUTTON (
			config->group[idx].radio_ascending);
		if (gtk_toggle_button_get_active (toggle_button))
			sort_type = GTK_SORT_ASCENDING;
		else
			sort_type = GTK_SORT_DESCENDING;

		e_table_sort_info_grouping_set_nth (
			sort_info, idx, column, sort_type);

		update_sort_and_group_config_dialog (config, FALSE);
	}  else {
		e_table_sort_info_grouping_truncate (sort_info, idx);
		update_sort_and_group_config_dialog (config, FALSE);
	}

	g_free (s);
}

static void
group_ascending_toggled (GtkToggleButton *toggle_button,
                         ETableConfigSortWidgets *group)
{
	ETableConfig *config = group->e_table_config;
	ETableSortInfo *si = config->temp_state->sort_info;
	ETableConfigSortWidgets *base = &config->group[0];
	ETableColumnSpecification *column;
	GtkSortType sort_type;
	gint idx = group - base;

	if (gtk_toggle_button_get_active (toggle_button))
		sort_type = GTK_SORT_ASCENDING;
	else
		sort_type = GTK_SORT_DESCENDING;

	column = e_table_sort_info_grouping_get_nth (si, idx, NULL);
	e_table_sort_info_grouping_set_nth (si, idx, column, sort_type);
}

static void
configure_group_dialog (ETableConfig *config,
                        GtkBuilder *builder)
{
	GSList *l;
	gint i;
	const gchar *vboxes[] = {"vbox7", "vbox9", "vbox11", "vbox13", NULL};

	for (i = 0; i < 4; i++) {
		gchar buffer[80];

		snprintf (buffer, sizeof (buffer), "group-combo-%d", i + 1);
		config->group[i].combo = e_table_proxy_gtk_combo_text_new ();
		gtk_widget_show (GTK_WIDGET (config->group[i].combo));
		gtk_box_pack_start (
			GTK_BOX (e_builder_get_widget (builder, vboxes[i])),
			config->group[i].combo, FALSE, FALSE, 0);

		configure_combo_box_add (
			GTK_COMBO_BOX (config->group[i].combo), "", "");

		snprintf (buffer, sizeof (buffer), "frame-group-%d", i + 1);
		config->group[i].frames =
			e_builder_get_widget (builder, buffer);

		snprintf (
			buffer, sizeof (buffer),
			"radiobutton-ascending-group-%d", i + 1);
		config->group[i].radio_ascending = e_builder_get_widget (
			builder, buffer);

		snprintf (
			buffer, sizeof (buffer),
			"radiobutton-descending-group-%d", i + 1);
		config->group[i].radio_descending = e_builder_get_widget (
			builder, buffer);

		snprintf (
			buffer, sizeof (buffer),
			"checkbutton-group-%d", i + 1);
		config->group[i].view_check = e_builder_get_widget (
			builder, buffer);

		config->group[i].e_table_config = config;
	}

	for (l = config->column_names; l; l = l->next) {
		gchar *label = l->data;

		for (i = 0; i < 4; i++) {
			configure_combo_box_add (
				GTK_COMBO_BOX (config->group[i].combo),
				dgettext (config->domain, label), label);
		}
	}

	/*
	 * After we have runtime modified things, signal connect
	 */
	for (i = 0; i < 4; i++) {
		config->group[i].changed_id = g_signal_connect (
			config->group[i].combo,
			"changed", G_CALLBACK (group_combo_changed),
			&config->group[i]);

		config->group[i].toggled_id = g_signal_connect (
			config->group[i].radio_ascending,
			"toggled", G_CALLBACK (group_ascending_toggled),
			&config->group[i]);
	}
}

static void
setup_gui (ETableConfig *config)
{
	GtkBuilder *builder;
	gboolean can_group;

	can_group = e_table_sort_info_get_can_group (config->state->sort_info);
	builder = gtk_builder_new ();
	e_load_ui_builder_definition (builder, "e-table-config.ui");

	config->dialog_toplevel = e_builder_get_widget (
		builder, "e-table-config");

	if (config->header)
		gtk_window_set_title (
			GTK_WINDOW (config->dialog_toplevel),
			config->header);

	config->dialog_group_by = e_builder_get_widget (
		builder, "dialog-group-by");
	config->dialog_sort = e_builder_get_widget (
		builder, "dialog-sort");

	config->sort_label = e_builder_get_widget (
		builder, "label-sort");
	config->group_label = e_builder_get_widget (
		builder, "label-group");
	config->fields_label = e_builder_get_widget (
		builder, "label-fields");

	connect_button (
		config, builder, "button-sort",
		G_CALLBACK (config_button_sort));
	connect_button (
		config, builder, "button-group",
		G_CALLBACK (config_button_group));
	connect_button (
		config, builder, "button-fields",
		G_CALLBACK (config_button_fields));

	if (!can_group) {
		GtkWidget *w;

		w = e_builder_get_widget (builder, "button-group");
		if (w)
			gtk_widget_hide (w);

		w = e_builder_get_widget (builder, "label3");
		if (w)
			gtk_widget_hide (w);

		w = config->group_label;
		if (w)
			gtk_widget_hide (w);
	}

	configure_sort_dialog (config, builder);
	configure_group_dialog (config, builder);

	g_object_weak_ref (
		G_OBJECT (config->dialog_toplevel),
		dialog_destroyed, config);

	g_signal_connect (
		config->dialog_toplevel, "response",
		G_CALLBACK (dialog_response), config);

	g_object_unref (builder);
}

static void
e_table_config_init (ETableConfig *config)
{
	config->domain = NULL;
}

ETableConfig *
e_table_config_construct (ETableConfig *config,
                          const gchar *header,
                          ETableSpecification *spec,
                          ETableState *state,
                          GtkWindow *parent_window)
{
	GPtrArray *array;
	guint ii;

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

	array = e_table_specification_ref_columns (spec);

	for (ii = 0; ii < array->len; ii++) {
		ETableColumnSpecification *column;

		column = g_ptr_array_index (array, ii);

		if (column->disabled)
			continue;

		config->column_names = g_slist_append (
			config->column_names, column->title);
	}

	g_ptr_array_unref (array);

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
e_table_config_new (const gchar *header,
                    ETableSpecification *spec,
                    ETableState *state,
                    GtkWindow *parent_window)
{
	ETableConfig *config;
	GtkDialog *dialog;
	GtkWidget *widget;

	config = g_object_new (E_TYPE_TABLE_CONFIG, NULL);

	e_table_config_construct (
		config, header, spec, state, parent_window);

	dialog = GTK_DIALOG (config->dialog_toplevel);

	widget = gtk_dialog_get_content_area (dialog);
	gtk_container_set_border_width (GTK_CONTAINER (widget), 0);

	widget = gtk_dialog_get_action_area (dialog);
	gtk_container_set_border_width (GTK_CONTAINER (widget), 12);

	gtk_dialog_set_response_sensitive (
		GTK_DIALOG (config->dialog_toplevel),
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
	GdkWindow *window;

	window = gtk_widget_get_window (GTK_WIDGET (config->dialog_toplevel));
	gdk_window_raise (window);
}

