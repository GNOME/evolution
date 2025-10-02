/*
 * e-contact-editor-dyntable.c
 *
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
 */

#include "e-contact-editor-dyntable.h"

#include "e-util/e-util.h"

struct _EContactEditorDynTablePrivate {

	/* absolute max, dyntable will ignore the rest */
	guint 	max_entries;

	/* current number of entries with text or requested by user*/
	guint 	curr_entries;

	/* minimum to show, with or without text */
	guint 	show_min_entries;

	/* no matter how much data, show only */
	guint 	show_max_entries;

	/* number of entries (combo/text) per row */
	guint 	columns;

	/* if true, fill line with empty slots*/
	gboolean justified;

	GtkWidget 	*add_button;
	GtkListStore 	*combo_store;
	GtkListStore 	*data_store;

	/* array of default values for combo box */
	const gint	*combo_defaults;

	/* number of elements in the array */
	size_t		combo_defaults_n;

	gboolean combo_with_entry;
};

G_DEFINE_TYPE_WITH_PRIVATE (EContactEditorDynTable, e_contact_editor_dyntable, GTK_TYPE_GRID)

/* one position is occupied by two widgets: combo+entry */
#define ENTRY_SIZE 2
#define MAX_CAPACITY 100

enum {
	CHANGED_SIGNAL,
	ACTIVATE_SIGNAL,
	ROW_ADDED_SIGNAL,
	LAST_SIGNAL
};

static guint dyntable_signals[LAST_SIGNAL];

GtkWidget*
e_contact_editor_dyntable_new (void)
{
	GtkWidget* widget;
	widget = GTK_WIDGET (g_object_new (e_contact_editor_dyntable_get_type (), NULL));
	return widget;
}

static void
set_combo_box_active (EContactEditorDynTable *dyntable,
                      GtkComboBox *combo_box,
                      gint active)
{
	g_signal_handlers_block_matched (combo_box,
			G_SIGNAL_MATCH_DATA, 0, 0, NULL,
			NULL, dyntable);
	gtk_combo_box_set_active (combo_box, active);
	g_signal_handlers_unblock_matched (combo_box,
	                                   G_SIGNAL_MATCH_DATA, 0, 0, NULL,
	                                   NULL, dyntable);
}

/* translate position into column/row */
static void
position_to_grid (EContactEditorDynTable *dyntable,
                  guint pos,
                  guint *col,
                  guint *row)
{
	*row = pos / dyntable->priv->columns;
	*col = pos % dyntable->priv->columns * ENTRY_SIZE;
}

static void
move_widget (GtkGrid *grid, GtkWidget *w, guint col, guint row)
{
	GValue rowValue = G_VALUE_INIT, colValue = G_VALUE_INIT;

	g_value_init (&rowValue, G_TYPE_UINT);
	g_value_init (&colValue, G_TYPE_UINT);

	g_value_set_uint (&rowValue, row);
	g_value_set_uint (&colValue, col);

	gtk_container_child_set_property (GTK_CONTAINER (grid), w,
	                                  "left-attach", &colValue);
	gtk_container_child_set_property (GTK_CONTAINER (grid), w,
	                                  "top-attach",	&rowValue);
}

static gboolean
is_button_required (EContactEditorDynTable *dyntable)
{
	if (dyntable->priv->curr_entries >= dyntable->priv->max_entries)
		return FALSE;
	if (dyntable->priv->curr_entries <= dyntable->priv->show_max_entries)
		return TRUE;
	else
		return FALSE;
}

static void
sensitize_button (EContactEditorDynTable *dyntable)
{
	guint row, col, current_entries;
	GtkWidget *w;
	GtkGrid *grid;
	EContactEditorDynTableClass *class;
	gboolean enabled;

	grid = GTK_GRID (dyntable);
	class = E_CONTACT_EDITOR_DYNTABLE_GET_CLASS (dyntable);

	current_entries = dyntable->priv->curr_entries;
	enabled = TRUE;
	if (current_entries > 0) {
		/* last entry */
		current_entries--;
		position_to_grid (dyntable, current_entries, &col, &row);
		w = gtk_grid_get_child_at (grid, col + 1, row);

		enabled = !class->widget_is_empty (dyntable, w);
	}

	gtk_widget_set_sensitive (dyntable->priv->add_button, enabled);
}

static void
show_button (EContactEditorDynTable *dyntable)
{
	guint col,row, pos;
	gboolean visible = FALSE;
	GtkGrid *grid;

	grid = GTK_GRID (dyntable);

	/* move button to end of current line */
	pos = dyntable->priv->curr_entries;
	if (pos > 0)
		pos--;
	position_to_grid(dyntable, pos, &col, &row);
	move_widget (grid, dyntable->priv->add_button,
	             dyntable->priv->columns*ENTRY_SIZE+1, row);

	/* set visibility */
	if (is_button_required (dyntable))
		visible = TRUE;

	gtk_widget_set_visible (dyntable->priv->add_button, visible);

	sensitize_button (dyntable);
}

static void
increment_counter (EContactEditorDynTable *dyntable)
{
	dyntable->priv->curr_entries++;
	show_button (dyntable);
}

static void
decrement_counter (EContactEditorDynTable *dyntable)
{
	dyntable->priv->curr_entries--;
	show_button (dyntable);
}

static gint
get_next_combo_index (EContactEditorDynTable *dyntable)
{
	size_t array_size = dyntable->priv->combo_defaults_n;
	gint idx = 0;

	if (dyntable->priv->combo_defaults != NULL) {
		idx = dyntable->priv->combo_defaults[dyntable->priv->curr_entries % array_size];
	}

	return idx;
}

static GtkWidget*
combo_box_create (EContactEditorDynTable *dyntable)
{
	GtkWidget *w;
	GtkComboBox *combo;
	GtkListStore *store;
	GtkCellRenderer *cell;

	if (dyntable->priv->combo_with_entry) {
		w = gtk_combo_box_new_with_entry ();
		gtk_combo_box_set_entry_text_column (GTK_COMBO_BOX (w), DYNTABLE_COMBO_COLUMN_TEXT);
	} else {
		w = gtk_combo_box_new ();
	}
	combo = GTK_COMBO_BOX (w);
	store = dyntable->priv->combo_store;

	gtk_combo_box_set_model (combo, GTK_TREE_MODEL(store));

	gtk_cell_layout_clear (GTK_CELL_LAYOUT (combo));

	cell = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), cell, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), cell,
	                                "text", DYNTABLE_COMBO_COLUMN_TEXT,
	                                "sensitive", DYNTABLE_COMBO_COLUMN_SENSITIVE,
	                                NULL);

	gtk_combo_box_set_active (combo, get_next_combo_index (dyntable));

	return w;
}

static void
emit_changed (EContactEditorDynTable *dyntable)
{
	g_signal_emit (dyntable, dyntable_signals[CHANGED_SIGNAL], 0);
}

static void
emit_activated (EContactEditorDynTable *dyntable)
{
	g_signal_emit (dyntable, dyntable_signals[ACTIVATE_SIGNAL], 0);
}

static void
emit_row_added (EContactEditorDynTable *dyntable)
{
	g_signal_emit (dyntable, dyntable_signals[ROW_ADDED_SIGNAL], 0);
}

static void
add_empty_entry (EContactEditorDynTable *dyntable)
{
	GtkGrid *grid;
	guint row, col;
	GtkWidget *box, *entry;
	EContactEditorDynTableClass *class;

	if (dyntable->priv->curr_entries >= dyntable->priv->max_entries)
		return;

	grid = GTK_GRID (dyntable);
	position_to_grid (dyntable, dyntable->priv->curr_entries, &col, &row);

	/* create new entry at last position */
	class = E_CONTACT_EDITOR_DYNTABLE_GET_CLASS (dyntable);
	box = combo_box_create (dyntable);
	gtk_grid_attach (grid, box, col, row, 1, 1);
	gtk_widget_show (box);

	entry = class->widget_create (dyntable);
	g_object_set (entry, "margin-start", 2, NULL);
	g_object_set (entry, "margin-end", 5, NULL);
	gtk_widget_set_hexpand (entry, GTK_EXPAND);
	gtk_grid_attach (grid, entry, col + 1, row, 1, 1);
	gtk_widget_show (entry);

	if (!dyntable->priv->combo_with_entry) {
		g_signal_connect_swapped (box, "changed",
			G_CALLBACK (gtk_widget_grab_focus), entry);
	}

	g_signal_connect_swapped(box, "changed",
	                         G_CALLBACK (emit_changed), dyntable);
	g_signal_connect_swapped(entry, "changed",
	                         G_CALLBACK (emit_changed), dyntable);
	g_signal_connect_swapped(entry, "changed",
		                 G_CALLBACK (sensitize_button), dyntable);
	g_signal_connect_swapped(entry, "activate",
	                         G_CALLBACK (emit_activated), dyntable);

	increment_counter (dyntable);

	if ( (dyntable->priv->justified && col < dyntable->priv->columns-1) ||
	     (dyntable->priv->curr_entries < dyntable->priv->show_min_entries) )
		add_empty_entry (dyntable);

	gtk_widget_grab_focus (entry);
}

static void
remove_empty_entries (EContactEditorDynTable *dyntable, gboolean fillup)
{
	guint row, col = G_MAXUINT, pos;
	GtkGrid* grid;
	GtkWidget* w;
	EContactEditorDynTableClass *class;

	grid = GTK_GRID (dyntable);
	class = E_CONTACT_EDITOR_DYNTABLE_GET_CLASS (dyntable);

	for (pos = 0; pos < dyntable->priv->curr_entries; pos++) {
		position_to_grid (dyntable, pos, &col, &row);
		w = gtk_grid_get_child_at (grid, col + 1, row);

		if (w != NULL && class->widget_is_empty (dyntable, w)) {
			guint pos2, next_col, next_row;

			gtk_widget_destroy (w);
			w = gtk_grid_get_child_at (grid, col, row);
			gtk_widget_destroy (w);

			/* now fill gap */
			for (pos2 = pos + 1; pos2 < dyntable->priv->curr_entries; pos2++) {
				position_to_grid (dyntable, pos2, &next_col, &next_row);
				w = gtk_grid_get_child_at (grid, next_col, next_row);
				move_widget (grid, w, col, row);
				w = gtk_grid_get_child_at (grid, next_col + 1, next_row);
				move_widget (grid, w, col + 1, row);
				col = next_col;
				row = next_row;
			}
			decrement_counter (dyntable);
			pos--; /* check the new widget on our current position */
		}

	}

	if (fillup &&
	    (dyntable->priv->curr_entries < dyntable->priv->show_min_entries ||
	    (dyntable->priv->justified && col < dyntable->priv->columns-1)))
		add_empty_entry (dyntable);
}

/* clears data, not the combo box list store */
void
e_contact_editor_dyntable_clear_data (EContactEditorDynTable *dyntable)
{
	guint i, col, row;
	GtkGrid *grid;
	GtkWidget *w;
	EContactEditorDynTableClass *class;

	grid = GTK_GRID(dyntable);
	class = E_CONTACT_EDITOR_DYNTABLE_GET_CLASS(dyntable);

	for (i = 0; i < dyntable->priv->curr_entries; i++) {
		position_to_grid (dyntable, i, &col, &row);
		w = gtk_grid_get_child_at (grid, col + 1, row);
		class->widget_clear (dyntable, w);
	}
	remove_empty_entries (dyntable, TRUE);

	gtk_list_store_clear (dyntable->priv->data_store);
}

static void
adjust_visibility_of_widgets (EContactEditorDynTable *dyntable)
{
	guint pos, col, row;
	GtkGrid *grid;
	GtkWidget *w;

	grid = GTK_GRID (dyntable);
	for (pos = 0; pos < dyntable->priv->curr_entries; pos++) {
		gboolean visible = FALSE;

		if (pos < dyntable->priv->show_max_entries)
			visible = TRUE;

		position_to_grid (dyntable, pos, &col, &row);
		w = gtk_grid_get_child_at (grid, col, row);
		gtk_widget_set_visible (w, visible);
		w = gtk_grid_get_child_at (grid, col + 1, row);
		gtk_widget_set_visible (w, visible);
	}

	show_button (dyntable);
}

/* number of columns can only be set before any data is added to this dyntable */
void e_contact_editor_dyntable_set_num_columns (EContactEditorDynTable *dyntable,
                                                guint number_of_columns,
                                                gboolean justified)
{
	GtkTreeIter iter;
	GtkTreeModel *store;
	gboolean holds_data;

	g_return_if_fail (number_of_columns > 0);

	store = GTK_TREE_MODEL (dyntable->priv->data_store);
	holds_data = gtk_tree_model_get_iter_first (store, &iter);
	g_return_if_fail (!holds_data);

	remove_empty_entries(dyntable, FALSE);

	dyntable->priv->columns = number_of_columns;
	dyntable->priv->justified = justified;

	remove_empty_entries(dyntable, TRUE);
}

void
e_contact_editor_dyntable_set_max_entries (EContactEditorDynTable *dyntable,
                                           guint max)
{
	GtkTreeModel *store;
	gint n_children;

	g_return_if_fail (max > 0);

	store = GTK_TREE_MODEL (dyntable->priv->data_store);

	n_children = gtk_tree_model_iter_n_children (store, NULL);
	if (n_children > max) {
		g_warning ("Dyntable holds %i items, setting max to %i, instead of %i",
				n_children, n_children, max);
		max = n_children;
	}

	dyntable->priv->max_entries = max;
	if (dyntable->priv->show_max_entries>max)
		dyntable->priv->show_max_entries = max;
	if (dyntable->priv->show_min_entries>max)
			dyntable->priv->show_min_entries = max;

	remove_empty_entries (dyntable, TRUE);
	show_button (dyntable);
}

/* show at least number_of_entries, with or without data */
void
e_contact_editor_dyntable_set_show_min (EContactEditorDynTable *dyntable,
                                        guint number_of_entries)
{
	if (number_of_entries > dyntable->priv->show_max_entries)
		dyntable->priv->show_min_entries = dyntable->priv->show_max_entries;
	else
		dyntable->priv->show_min_entries = number_of_entries;

	if (dyntable->priv->curr_entries < dyntable->priv->show_min_entries)
		add_empty_entry (dyntable);

	adjust_visibility_of_widgets (dyntable);
}

/* show no more than number_of_entries, hide the rest */
void
e_contact_editor_dyntable_set_show_max (EContactEditorDynTable *dyntable,
                                        guint number_of_entries)
{
	if (number_of_entries > dyntable->priv->max_entries) {
		dyntable->priv->show_max_entries = dyntable->priv->max_entries;
	} else if (number_of_entries < dyntable->priv->show_min_entries) {
		dyntable->priv->show_max_entries = dyntable->priv->show_min_entries;
	} else {
		dyntable->priv->show_max_entries = number_of_entries;
	}

	adjust_visibility_of_widgets (dyntable);
}

void
e_contact_editor_dyntable_set_combo_with_entry (EContactEditorDynTable *self,
						gboolean value)
{
	g_return_if_fail (E_IS_CONTACT_EDITOR_DYNTABLE (self));

	self->priv->combo_with_entry = value;
}

gboolean
e_contact_editor_dyntable_get_combo_with_entry (EContactEditorDynTable *self)
{
	g_return_val_if_fail (E_IS_CONTACT_EDITOR_DYNTABLE (self), FALSE);

	return self->priv->combo_with_entry;
}

/* use data_store to fill data into widgets */
void
e_contact_editor_dyntable_fill_in_data (EContactEditorDynTable *dyntable)
{
	guint pos = 0, col, row;
	EContactEditorDynTableClass *class;
	GtkGrid *grid;
	GtkTreeIter iter;
	GtkTreeModel *store;
	GtkWidget *w;
	gboolean valid;

	class = E_CONTACT_EDITOR_DYNTABLE_GET_CLASS (dyntable);
	grid = GTK_GRID (dyntable);
	store = GTK_TREE_MODEL (dyntable->priv->data_store);

	valid = gtk_tree_model_get_iter_first (store, &iter);
	while (valid) {
		gchar *str_data = NULL;
		gchar *sel_text = NULL;
		gint int_data;

		gtk_tree_model_get (store, &iter,
				DYNTABLE_STORE_COLUMN_ENTRY_STRING, &str_data,
				DYNTABLE_STORE_COLUMN_SELECTED_ITEM, &int_data,
				DYNTABLE_STORE_COLUMN_SELECTED_TEXT, &sel_text,
				-1);

		if (pos >= dyntable->priv->curr_entries)
			add_empty_entry (dyntable);

		position_to_grid (dyntable, pos++, &col, &row);
		w = gtk_grid_get_child_at (grid, col, row);
		set_combo_box_active (dyntable, GTK_COMBO_BOX(w), int_data);
		if (int_data < 0 && sel_text) {
			GtkWidget *child;

			child = gtk_bin_get_child (GTK_BIN (w));
			if (GTK_IS_ENTRY (child))
				gtk_entry_set_text (GTK_ENTRY (child), sel_text);
		}
		w = gtk_grid_get_child_at (grid, col + 1, row);
		class->widget_fill (dyntable, w, str_data);

		g_free (str_data);
		g_free (sel_text);

		valid = gtk_tree_model_iter_next (store, &iter);

		if (valid && pos >= dyntable->priv->max_entries) {
			g_warning ("dyntable is configured with max_entries = %i, ignoring the rest.", dyntable->priv->max_entries);
			break;
		}
	}

	/* fix visibility of added items */
	adjust_visibility_of_widgets (dyntable);
}

/* the model returned has 3 columns
 *
 * UINT: sort order
 * UINT: active combo item
 * STRING: data extracted with widget_extract()
 */
GtkListStore*
e_contact_editor_dyntable_extract_data (EContactEditorDynTable *dyntable)
{
	EContactEditorDynTableClass *class;
	GtkGrid *grid;
	GtkListStore *data_store;
	GtkWidget *w;
	guint pos, col, row;

	grid = GTK_GRID(dyntable);
	class = E_CONTACT_EDITOR_DYNTABLE_GET_CLASS(dyntable);
	data_store = dyntable->priv->data_store;

	gtk_list_store_clear (data_store);

	for (pos = 0; pos < dyntable->priv->curr_entries; pos++) {

		position_to_grid (dyntable, pos, &col, &row);
		w = gtk_grid_get_child_at (grid, col + 1, row);

		if (!class->widget_is_empty (dyntable, w)) {
			GtkTreeIter iter;
			gchar *dup;
			const gchar *combo_entry_text = NULL;
			gint combo_item;
			const gchar *data;

			data = class->widget_extract (dyntable, w);
			w = gtk_grid_get_child_at (grid, col, row);
			combo_item = gtk_combo_box_get_active (GTK_COMBO_BOX(w));
			if (dyntable->priv->combo_with_entry && combo_item < 0) {
				GtkWidget *child;

				child = gtk_bin_get_child (GTK_BIN (w));
				if (GTK_IS_ENTRY (child))
					combo_entry_text = gtk_entry_get_text (GTK_ENTRY (child));
			}

			dup = g_strdup (data);
			g_strstrip(dup);

			gtk_list_store_append (data_store, &iter);
			gtk_list_store_set (data_store, &iter,
			                    DYNTABLE_STORE_COLUMN_SORTORDER, pos,
			                    DYNTABLE_STORE_COLUMN_SELECTED_ITEM, combo_item,
					    DYNTABLE_STORE_COLUMN_SELECTED_TEXT, combo_entry_text,
			                    DYNTABLE_STORE_COLUMN_ENTRY_STRING, dup,
			                    -1);

			g_free (dup);
		}
	}

	return dyntable->priv->data_store;
}

/* the model returned has two columns
 *
 * STRING: bound to attribute "text"
 * BOOLEAN: bound to attribute "sensitive"
 */
GtkListStore*
e_contact_editor_dyntable_get_combo_store (EContactEditorDynTable *dyntable)
{
	return dyntable->priv->combo_store;
}

void
e_contact_editor_dyntable_set_combo_defaults (EContactEditorDynTable *dyntable,
                                              const gint *defaults,
                                              size_t defaults_n)
{
	dyntable->priv->combo_defaults = defaults;
	dyntable->priv->combo_defaults_n = defaults_n;
}

static void
dispose_impl (GObject *object)
{
	GtkListStore *store;
	EContactEditorDynTable *dyntable;

	dyntable = E_CONTACT_EDITOR_DYNTABLE(object);

	store = dyntable->priv->data_store;
	if (store) {
		gtk_list_store_clear (store);
		g_object_unref (store);
		dyntable->priv->data_store = NULL;
	}

	store = dyntable->priv->combo_store;
	if (store) {
		g_object_unref (store);
		dyntable->priv->combo_store = NULL;
	}

	G_OBJECT_CLASS(e_contact_editor_dyntable_parent_class)->dispose (object);
}

static GtkWidget*
default_impl_widget_create (EContactEditorDynTable *dyntable)
{
	return gtk_entry_new ();
}

static void
default_impl_widget_clear (EContactEditorDynTable *dyntable,
                           GtkWidget *w)
{
	GtkEntry *e;
	e = GTK_ENTRY(w);
	gtk_entry_set_text (e, "");
}

static const gchar*
default_impl_widget_extract (EContactEditorDynTable *dyntable,
                             GtkWidget *w)
{
	GtkEntry *e;

	e = GTK_ENTRY(w);
	return gtk_entry_get_text (e);
}

static void
default_impl_widget_fill (EContactEditorDynTable *dyntable,
                          GtkWidget *w,
                          const gchar *value)
{
	GtkEntry *e;

	e = GTK_ENTRY(w);
	gtk_entry_set_text (e, value);
}

static gboolean
default_impl_widget_is_empty (EContactEditorDynTable *dyntable,
                              GtkWidget *w)
{
	GtkEntry *e;

	e = GTK_ENTRY(w);
	if (0 == gtk_entry_get_text_length (e))
		return TRUE;

	return e_str_is_empty (gtk_entry_get_text (e));
}

static void
e_contact_editor_dyntable_init (EContactEditorDynTable *dyntable)
{
	GtkGrid *grid;

	dyntable->priv = e_contact_editor_dyntable_get_instance_private (dyntable);

	/* fill in defaults */
	dyntable->priv->max_entries = MAX_CAPACITY;
	dyntable->priv->curr_entries = 0;
	dyntable->priv->show_min_entries = 0;
	dyntable->priv->show_max_entries = dyntable->priv->max_entries;
	dyntable->priv->columns = 2;
	dyntable->priv->justified = FALSE;
	dyntable->priv->combo_defaults = NULL;

	dyntable->priv->combo_store = gtk_list_store_new (DYNTABLE_COBMO_COLUMN_NUM_COLUMNS,
			G_TYPE_STRING, G_TYPE_BOOLEAN);
	dyntable->priv->data_store = gtk_list_store_new (DYNTABLE_STORE_COLUMN_NUM_COLUMNS,
			G_TYPE_UINT, G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING);
	gtk_tree_sortable_set_sort_column_id (
			GTK_TREE_SORTABLE (dyntable->priv->data_store),
			DYNTABLE_STORE_COLUMN_SORTORDER,
			GTK_SORT_ASCENDING);

	dyntable->priv->add_button = gtk_button_new_with_label ("+");
	g_signal_connect_swapped (dyntable->priv->add_button, "clicked",
			G_CALLBACK (add_empty_entry), dyntable);
	g_signal_connect_swapped(dyntable->priv->add_button, "clicked",
			G_CALLBACK (emit_row_added), dyntable);

	grid = GTK_GRID (dyntable);

	gtk_grid_attach (grid, dyntable->priv->add_button, 0, 0, 1, 1);
	gtk_widget_set_valign (dyntable->priv->add_button, GTK_ALIGN_CENTER);
	gtk_widget_set_halign (dyntable->priv->add_button, GTK_ALIGN_START);
	gtk_widget_show (dyntable->priv->add_button);

	if (dyntable->priv->curr_entries < dyntable->priv->show_min_entries)
		add_empty_entry (dyntable);
}

static void
e_contact_editor_dyntable_class_init (EContactEditorDynTableClass *class)
{
	GObjectClass *object_class;

	dyntable_signals[CHANGED_SIGNAL] = g_signal_new ("changed",
			G_TYPE_FROM_CLASS(class), G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
			G_STRUCT_OFFSET(EContactEditorDynTableClass, changed),
			NULL, NULL,
			g_cclosure_marshal_VOID__VOID,
			G_TYPE_NONE, 0);
	dyntable_signals[ACTIVATE_SIGNAL] = g_signal_new ("activate",
			G_TYPE_FROM_CLASS(class), G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
			G_STRUCT_OFFSET(EContactEditorDynTableClass, activate),
			NULL, NULL,
			g_cclosure_marshal_VOID__VOID,
			G_TYPE_NONE, 0);
	dyntable_signals[ROW_ADDED_SIGNAL] = g_signal_new ("row-added",
			G_TYPE_FROM_CLASS(class), G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
			G_STRUCT_OFFSET(EContactEditorDynTableClass, row_added),
			NULL, NULL,
			g_cclosure_marshal_VOID__VOID,
			G_TYPE_NONE, 0);

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = dispose_impl;

	/* virtual functions */
	class->widget_create 	= default_impl_widget_create;
	class->widget_is_empty 	= default_impl_widget_is_empty;
	class->widget_clear 	= default_impl_widget_clear;
	class->widget_extract 	= default_impl_widget_extract;
	class->widget_fill 	= default_impl_widget_fill;
}
