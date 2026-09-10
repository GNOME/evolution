/*
 * SPDX-FileCopyrightText: (C) 2026 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include "e-misc-utils.h"
#include "e-virtual-tree.h"

#include "e-virtual-tree-customize-popover.h"

#define MAX_SORT_LEVELS 4

enum {
	FIELDS_COL_VISIBLE,
	FIELDS_COL_TITLE,
	FIELDS_COL_INDEX,
	FIELDS_N_COLUMNS
};

typedef struct {
	GtkWidget *hbox;
	GtkWidget *combo;
	GtkWidget *radio_asc;
	GtkWidget *radio_desc;
} SortLevelWidgets;

struct _EVirtualTreeCustomizePopover {
	GtkPopover parent;

	EVirtualTree *vtree;

	GtkWidget *notebook;

	GtkListStore *fields_store;
	GtkTreeView *fields_tree_view;

	SortLevelWidgets sort_levels[MAX_SORT_LEVELS];

	SortLevelWidgets group_levels[MAX_SORT_LEVELS];
	GtkWidget *group_page;

	gboolean has_changes;
	gboolean rebuilding_combos;
};

G_DEFINE_TYPE (EVirtualTreeCustomizePopover, e_virtual_tree_customize_popover, GTK_TYPE_POPOVER)

static void
on_any_widget_changed (EVirtualTreeCustomizePopover *self)
{
	self->has_changes = TRUE;
}

static gint
combo_get_selected_column (GtkComboBoxText *combo)
{
	const gchar *active_id;

	active_id = gtk_combo_box_get_active_id (GTK_COMBO_BOX (combo));
	if (!active_id || g_strcmp0 (active_id, "none") == 0)
		return -1;

	return (gint) g_ascii_strtoll (active_id, NULL, 10);
}

static gint
compare_columns_by_title (gconstpointer aa,
			  gconstpointer bb,
			  gpointer user_data)
{
	EVirtualTreeCustomizePopover *self = user_data;
	guint col_a = *(const guint *) aa;
	guint col_b = *(const guint *) bb;
	const gchar *title_a = e_virtual_tree_get_column_title (self->vtree, col_a);
	const gchar *title_b = e_virtual_tree_get_column_title (self->vtree, col_b);

	return g_utf8_collate (title_a ? title_a : "", title_b ? title_b : "");
}

static GArray *
get_columns_sorted_by_title (EVirtualTreeCustomizePopover *self,
			     guint n_columns)
{
	GArray *order;
	guint ii;

	order = g_array_new (FALSE, FALSE, sizeof (guint));

	for (ii = 0; ii < n_columns; ii++)
		g_array_append_val (order, ii);

	g_array_sort_with_data (order, compare_columns_by_title, self);

	return order;
}

static void
rebuild_combos_for_levels (EVirtualTreeCustomizePopover *self,
			   SortLevelWidgets *levels,
			   gboolean is_group,
			   gint changed_level)
{
	guint n_columns, ii, jj;
	gint selected_cols[MAX_SORT_LEVELS];
	gboolean prev_is_none;
	GArray *col_order;

	if (self->rebuilding_combos)
		return;

	self->rebuilding_combos = TRUE;

	n_columns = e_virtual_tree_get_n_columns (self->vtree);
	col_order = get_columns_sorted_by_title (self, n_columns);

	for (ii = 0; ii < MAX_SORT_LEVELS; ii++) {
		selected_cols[ii] = combo_get_selected_column (GTK_COMBO_BOX_TEXT (levels[ii].combo));
	}

	prev_is_none = FALSE;

	for (ii = 0; ii < MAX_SORT_LEVELS; ii++) {
		GHashTable *used_by_preceding;
		gchar id_buf[32];
		gboolean found_current;
		guint n_available;

		if (ii > 0 && prev_is_none) {
			selected_cols[ii] = -1;
			gtk_combo_box_set_active_id (GTK_COMBO_BOX (levels[ii].combo), "none");
			gtk_widget_set_sensitive (levels[ii].hbox, FALSE);
			continue;
		}

		if ((gint) ii <= changed_level && changed_level >= 0) {
			prev_is_none = selected_cols[ii] < 0;
			continue;
		}

		used_by_preceding = g_hash_table_new (g_direct_hash, g_direct_equal);
		for (jj = 0; jj < ii; jj++) {
			if (selected_cols[jj] >= 0)
				g_hash_table_add (used_by_preceding, GINT_TO_POINTER (selected_cols[jj]));
		}

		gtk_combo_box_text_remove_all (GTK_COMBO_BOX_TEXT (levels[ii].combo));
		gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (levels[ii].combo), "none", _("(None)"));

		found_current = FALSE;
		n_available = 0;

		for (jj = 0; jj < n_columns; jj++) {
			const gchar *title;
			guint col_idx = g_array_index (col_order, guint, jj);

			if (is_group && !e_virtual_tree_get_column_groupable (self->vtree, col_idx))
				continue;

			if (g_hash_table_contains (used_by_preceding, GINT_TO_POINTER ((gint) col_idx)))
				continue;

			title = e_virtual_tree_get_column_title (self->vtree, col_idx);
			if (!title || !*title)
				continue;

			g_snprintf (id_buf, sizeof (id_buf), "%u", col_idx);
			gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (levels[ii].combo), id_buf, title);
			n_available++;

			if (selected_cols[ii] == (gint) col_idx)
				found_current = TRUE;
		}

		if (ii > 0 && n_available == 0) {
			selected_cols[ii] = -1;
			gtk_combo_box_set_active_id (GTK_COMBO_BOX (levels[ii].combo), "none");
			gtk_widget_set_sensitive (levels[ii].hbox, FALSE);
			prev_is_none = TRUE;
			g_hash_table_destroy (used_by_preceding);
			continue;
		}

		gtk_widget_set_sensitive (levels[ii].hbox, TRUE);

		if (found_current) {
			g_snprintf (id_buf, sizeof (id_buf), "%u", (guint) selected_cols[ii]);
			gtk_combo_box_set_active_id (GTK_COMBO_BOX (levels[ii].combo), id_buf);
		} else {
			selected_cols[ii] = -1;
			gtk_combo_box_set_active_id (GTK_COMBO_BOX (levels[ii].combo), "none");
		}

		gtk_widget_set_sensitive (levels[ii].radio_asc, selected_cols[ii] >= 0);
		gtk_widget_set_sensitive (levels[ii].radio_desc, selected_cols[ii] >= 0);

		prev_is_none = selected_cols[ii] < 0;

		g_hash_table_destroy (used_by_preceding);
	}

	g_array_unref (col_order);

	self->rebuilding_combos = FALSE;
}

static void
on_sort_combo_changed (GtkComboBoxText *combo,
		       gpointer user_data)
{
	EVirtualTreeCustomizePopover *self = user_data;
	gboolean has_column;
	gint level;

	if (self->rebuilding_combos)
		return;

	on_any_widget_changed (self);

	has_column = g_strcmp0 (gtk_combo_box_get_active_id (GTK_COMBO_BOX (combo)), "none") != 0;

	level = -1;
	for (guint ii = 0; ii < MAX_SORT_LEVELS; ii++) {
		if (self->sort_levels[ii].combo == GTK_WIDGET (combo)) {
			gtk_widget_set_sensitive (self->sort_levels[ii].radio_asc, has_column);
			gtk_widget_set_sensitive (self->sort_levels[ii].radio_desc, has_column);
			level = (gint) ii;
			break;
		}
	}
	if (level >= 0) {
		rebuild_combos_for_levels (self, self->sort_levels, FALSE, level);
		return;
	}

	for (guint ii = 0; ii < MAX_SORT_LEVELS; ii++) {
		if (self->group_levels[ii].combo == GTK_WIDGET (combo)) {
			gtk_widget_set_sensitive (self->group_levels[ii].radio_asc, has_column);
			gtk_widget_set_sensitive (self->group_levels[ii].radio_desc, has_column);
			level = (gint) ii;
			break;
		}
	}
	if (level >= 0)
		rebuild_combos_for_levels (self, self->group_levels, TRUE, level);
}

static void
populate_sort_combos (EVirtualTreeCustomizePopover *self,
		      SortLevelWidgets *levels,
		      gboolean is_group)
{
	guint n_columns, ii, jj;
	gint sorted_cols[MAX_SORT_LEVELS];
	gint sorted_prios[MAX_SORT_LEVELS];
	GtkSortType sorted_dirs[MAX_SORT_LEVELS];
	guint n_sorted;
	GArray *col_order;

	n_columns = e_virtual_tree_get_n_columns (self->vtree);
	col_order = get_columns_sorted_by_title (self, n_columns);

	self->rebuilding_combos = TRUE;

	n_sorted = 0;

	for (ii = 0; ii < n_columns && n_sorted < MAX_SORT_LEVELS; ii++) {
		GtkSortType sort_order;
		gint priority;
		gboolean has;

		if (is_group)
			has = e_virtual_tree_get_column_group (self->vtree, ii, &sort_order, &priority);
		else
			has = e_virtual_tree_get_column_sort (self->vtree, ii, &sort_order, &priority);

		if (has) {
			sorted_cols[n_sorted] = (gint) ii;
			sorted_dirs[n_sorted] = sort_order;
			sorted_prios[n_sorted] = priority;
			n_sorted++;
		}
	}

	for (ii = 0; ii < n_sorted; ii++) {
		for (jj = ii + 1; jj < n_sorted; jj++) {
			if (sorted_prios[jj] < sorted_prios[ii]) {
				gint tmp_col = sorted_cols[ii];
				gint tmp_prio = sorted_prios[ii];
				GtkSortType tmp_dir = sorted_dirs[ii];

				sorted_cols[ii] = sorted_cols[jj];
				sorted_prios[ii] = sorted_prios[jj];
				sorted_dirs[ii] = sorted_dirs[jj];

				sorted_cols[jj] = tmp_col;
				sorted_prios[jj] = tmp_prio;
				sorted_dirs[jj] = tmp_dir;
			}
		}
	}

	for (ii = 0; ii < MAX_SORT_LEVELS; ii++) {
		GHashTable *used_by_preceding;
		gchar id_buf[32];
		guint n_available;
		gboolean prev_is_none;

		prev_is_none = ii > 0 && (ii > n_sorted || sorted_cols[ii - 1] < 0);

		if (prev_is_none) {
			gtk_combo_box_set_active_id (GTK_COMBO_BOX (levels[ii].combo), "none");
			gtk_widget_set_sensitive (levels[ii].hbox, FALSE);
			continue;
		}

		used_by_preceding = g_hash_table_new (g_direct_hash, g_direct_equal);
		for (jj = 0; jj < ii; jj++) {
			if (jj < n_sorted)
				g_hash_table_add (used_by_preceding, GINT_TO_POINTER (sorted_cols[jj]));
		}

		gtk_combo_box_text_remove_all (GTK_COMBO_BOX_TEXT (levels[ii].combo));
		gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (levels[ii].combo), "none", _("(None)"));

		n_available = 0;
		for (jj = 0; jj < n_columns; jj++) {
			const gchar *title;
			guint col_idx = g_array_index (col_order, guint, jj);

			if (is_group && !e_virtual_tree_get_column_groupable (self->vtree, col_idx))
				continue;

			if (g_hash_table_contains (used_by_preceding, GINT_TO_POINTER ((gint) col_idx)))
				continue;

			title = e_virtual_tree_get_column_title (self->vtree, col_idx);
			if (!title || !*title)
				continue;

			g_snprintf (id_buf, sizeof (id_buf), "%u", col_idx);
			gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (levels[ii].combo), id_buf, title);
			n_available++;
		}

		if (ii > 0 && n_available == 0) {
			gtk_combo_box_set_active_id (GTK_COMBO_BOX (levels[ii].combo), "none");
			gtk_widget_set_sensitive (levels[ii].hbox, FALSE);
			g_hash_table_destroy (used_by_preceding);
			continue;
		}

		gtk_widget_set_sensitive (levels[ii].hbox, TRUE);

		if (ii < n_sorted) {
			g_snprintf (id_buf, sizeof (id_buf), "%u", (guint) sorted_cols[ii]);
			gtk_combo_box_set_active_id (GTK_COMBO_BOX (levels[ii].combo), id_buf);
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (
				sorted_dirs[ii] == GTK_SORT_ASCENDING ? levels[ii].radio_asc : levels[ii].radio_desc),
				TRUE);
			gtk_widget_set_sensitive (levels[ii].radio_asc, TRUE);
			gtk_widget_set_sensitive (levels[ii].radio_desc, TRUE);
		} else {
			gtk_combo_box_set_active_id (GTK_COMBO_BOX (levels[ii].combo), "none");
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (levels[ii].radio_asc), TRUE);
			gtk_widget_set_sensitive (levels[ii].radio_asc, FALSE);
			gtk_widget_set_sensitive (levels[ii].radio_desc, FALSE);
		}

		g_hash_table_destroy (used_by_preceding);
	}

	g_array_unref (col_order);

	self->rebuilding_combos = FALSE;
}

static void
populate_fields_list (EVirtualTreeCustomizePopover *self)
{
	GtkTreeView *vtree_view;
	GList *display_columns, *link;
	GHashTable *added;
	guint n_columns, ii;

	gtk_list_store_clear (self->fields_store);

	vtree_view = e_virtual_tree_get_tree_view (self->vtree);
	display_columns = gtk_tree_view_get_columns (vtree_view);
	n_columns = e_virtual_tree_get_n_columns (self->vtree);

	added = g_hash_table_new (NULL, NULL);

	for (link = display_columns; link; link = link->next) {
		GtkTreeViewColumn *tvc = link->data;
		gboolean visible = gtk_tree_view_column_get_visible (tvc);

		for (ii = 0; ii < n_columns; ii++) {
			GtkTreeViewColumn *col = e_virtual_tree_get_column (self->vtree, ii);

			if (col == tvc) {
				const gchar *title = e_virtual_tree_get_column_title (self->vtree, ii);
				GtkTreeIter iter;

				if (visible) {
					gtk_list_store_append (self->fields_store, &iter);
					gtk_list_store_set (self->fields_store, &iter,
						FIELDS_COL_VISIBLE, TRUE,
						FIELDS_COL_TITLE, title && *title ? title : e_virtual_tree_get_column_id (self->vtree, ii),
						FIELDS_COL_INDEX, ii,
						-1);
				}

				g_hash_table_add (added, GUINT_TO_POINTER (ii));
				break;
			}
		}
	}

	for (ii = 0; ii < n_columns; ii++) {
		if (!g_hash_table_contains (added, GUINT_TO_POINTER (ii))) {
			const gchar *title = e_virtual_tree_get_column_title (self->vtree, ii);
			GtkTreeIter iter;

			gtk_list_store_append (self->fields_store, &iter);
			gtk_list_store_set (self->fields_store, &iter,
				FIELDS_COL_VISIBLE, FALSE,
				FIELDS_COL_TITLE, title && *title ? title : e_virtual_tree_get_column_id (self->vtree, ii),
				FIELDS_COL_INDEX, ii,
				-1);
		}
	}

	for (link = display_columns; link; link = link->next) {
		GtkTreeViewColumn *tvc = link->data;
		gboolean visible = gtk_tree_view_column_get_visible (tvc);

		if (visible)
			continue;

		for (ii = 0; ii < n_columns; ii++) {
			GtkTreeViewColumn *col = e_virtual_tree_get_column (self->vtree, ii);

			if (col == tvc && g_hash_table_contains (added, GUINT_TO_POINTER (ii))) {
				const gchar *title = e_virtual_tree_get_column_title (self->vtree, ii);
				GtkTreeIter iter;

				gtk_list_store_append (self->fields_store, &iter);
				gtk_list_store_set (self->fields_store, &iter,
					FIELDS_COL_VISIBLE, FALSE,
					FIELDS_COL_TITLE, title && *title ? title : e_virtual_tree_get_column_id (self->vtree, ii),
					FIELDS_COL_INDEX, ii,
					-1);
				break;
			}
		}
	}

	g_hash_table_destroy (added);
	g_list_free (display_columns);
}

static void
on_fields_visible_toggled (GtkCellRendererToggle *renderer,
			   gchar *path_str,
			   gpointer user_data)
{
	EVirtualTreeCustomizePopover *self = user_data;
	GtkTreePath *path;
	GtkTreeIter iter;
	gboolean visible;

	path = gtk_tree_path_new_from_string (path_str);

	if (gtk_tree_model_get_iter (GTK_TREE_MODEL (self->fields_store), &iter, path)) {
		gtk_tree_model_get (GTK_TREE_MODEL (self->fields_store), &iter, FIELDS_COL_VISIBLE, &visible, -1);
		gtk_list_store_set (self->fields_store, &iter, FIELDS_COL_VISIBLE, !visible, -1);
		on_any_widget_changed (self);
	}

	gtk_tree_path_free (path);
}

static void
fields_move_selected (EVirtualTreeCustomizePopover *self,
		      gint direction)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter, target;

	selection = gtk_tree_view_get_selection (self->fields_tree_view);
	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return;

	target = iter;

	if (direction < -1) {
		GtkTreeIter first;

		if (gtk_tree_model_get_iter_first (model, &first))
			gtk_list_store_move_before (self->fields_store, &iter, &first);
	} else if (direction == -1) {
		GtkTreePath *path;

		path = gtk_tree_model_get_path (model, &target);
		if (gtk_tree_path_prev (path) && gtk_tree_model_get_iter (model, &target, path))
			gtk_list_store_move_before (self->fields_store, &iter, &target);
		gtk_tree_path_free (path);
	} else if (direction == 1) {
		if (gtk_tree_model_iter_next (model, &target))
			gtk_list_store_move_after (self->fields_store, &iter, &target);
	} else {
		gtk_list_store_move_before (self->fields_store, &iter, NULL);
	}
}

static void
on_fields_move_top (GtkButton *button, gpointer user_data)
{
	fields_move_selected (user_data, -2);
}

static void
on_fields_move_up (GtkButton *button, gpointer user_data)
{
	fields_move_selected (user_data, -1);
}

static void
on_fields_move_down (GtkButton *button, gpointer user_data)
{
	fields_move_selected (user_data, 1);
}

static void
on_fields_move_bottom (GtkButton *button, gpointer user_data)
{
	fields_move_selected (user_data, 2);
}

static GtkWidget *
create_move_button (const gchar *icon_name,
		    const gchar *tooltip,
		    GCallback callback,
		    gpointer user_data)
{
	GtkWidget *button, *image;

	button = gtk_button_new ();
	image = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_SMALL_TOOLBAR);
	gtk_button_set_image (GTK_BUTTON (button), image);
	gtk_widget_set_tooltip_text (button, tooltip);
	g_signal_connect (button, "clicked", callback, user_data);

	return button;
}

static GtkWidget *
create_fields_tab (EVirtualTreeCustomizePopover *self)
{
	GtkWidget *vbox, *scrolled, *button_box;
	GtkTreeView *tree_view;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	self->fields_store = gtk_list_store_new (FIELDS_N_COLUMNS,
		G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_UINT);

	tree_view = GTK_TREE_VIEW (gtk_tree_view_new_with_model (GTK_TREE_MODEL (self->fields_store)));
	self->fields_tree_view = tree_view;
	gtk_tree_view_set_headers_visible (tree_view, FALSE);

	renderer = gtk_cell_renderer_toggle_new ();
	g_signal_connect (renderer, "toggled",
		G_CALLBACK (on_fields_visible_toggled), self);
	column = gtk_tree_view_column_new_with_attributes ("", renderer, "active", FIELDS_COL_VISIBLE, NULL);
	gtk_tree_view_append_column (tree_view, column);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ("", renderer, "text", FIELDS_COL_TITLE, NULL);
	gtk_tree_view_append_column (tree_view, column);

	scrolled = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled), GTK_SHADOW_IN);
	gtk_container_add (GTK_CONTAINER (scrolled), GTK_WIDGET (tree_view));
	gtk_widget_set_size_request (scrolled, -1, 200);

	button_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_box_pack_start (GTK_BOX (button_box),
		create_move_button ("go-top-symbolic", _("Move to Top"), G_CALLBACK (on_fields_move_top), self), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (button_box),
		create_move_button ("go-up-symbolic", _("Move Up"), G_CALLBACK (on_fields_move_up), self), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (button_box),
		create_move_button ("go-down-symbolic", _("Move Down"), G_CALLBACK (on_fields_move_down), self), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (button_box),
		create_move_button ("go-bottom-symbolic", _("Move to Bottom"), G_CALLBACK (on_fields_move_bottom), self), FALSE, FALSE, 0);

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
	gtk_box_pack_start (GTK_BOX (vbox), scrolled, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), button_box, FALSE, FALSE, 0);

	g_signal_connect_swapped (self->fields_store, "row-changed",
		G_CALLBACK (on_any_widget_changed), self);
	g_signal_connect_swapped (self->fields_store, "row-deleted",
		G_CALLBACK (on_any_widget_changed), self);
	g_signal_connect_swapped (self->fields_store, "row-inserted",
		G_CALLBACK (on_any_widget_changed), self);

	return vbox;
}

static void
on_clear_all_clicked (GtkButton *button,
		      gpointer user_data)
{
	SortLevelWidgets *levels = user_data;
	guint ii;

	for (ii = 0; ii < MAX_SORT_LEVELS; ii++) {
		gtk_combo_box_set_active_id (GTK_COMBO_BOX (levels[ii].combo), "none");
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (levels[ii].radio_asc), TRUE);
		gtk_widget_set_sensitive (levels[ii].radio_asc, FALSE);
		gtk_widget_set_sensitive (levels[ii].radio_desc, FALSE);
		if (ii > 0)
			gtk_widget_set_sensitive (levels[ii].hbox, FALSE);
	}
}

static GtkWidget *
create_sort_group_tab (EVirtualTreeCustomizePopover *self,
		       SortLevelWidgets *levels,
		       const gchar *first_label)
{
	GtkWidget *vbox, *hbox, *label, *button;
	guint ii;

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_set_margin_start (vbox, 6);
	gtk_widget_set_margin_end (vbox, 6);
	gtk_widget_set_margin_top (vbox, 6);
	gtk_widget_set_margin_bottom (vbox, 6);

	for (ii = 0; ii < MAX_SORT_LEVELS; ii++) {
		hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
		gtk_widget_set_margin_start (hbox, ii * 12);

		if (ii == 0)
			label = gtk_label_new (first_label);
		else
			label = gtk_label_new (_("Then by"));

		gtk_widget_set_size_request (label, 80, -1);
		gtk_label_set_xalign (GTK_LABEL (label), 0.0);
		gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

		levels[ii].combo = gtk_combo_box_text_new ();
		gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (levels[ii].combo), "none", _("(None)"));
		gtk_widget_set_hexpand (levels[ii].combo, TRUE);
		gtk_box_pack_start (GTK_BOX (hbox), levels[ii].combo, TRUE, TRUE, 0);

		levels[ii].radio_asc = gtk_radio_button_new_with_label (NULL, _("Ascending"));
		gtk_box_pack_start (GTK_BOX (hbox), levels[ii].radio_asc, FALSE, FALSE, 0);

		levels[ii].radio_desc = gtk_radio_button_new_with_label_from_widget (
			GTK_RADIO_BUTTON (levels[ii].radio_asc), _("Descending"));
		gtk_box_pack_start (GTK_BOX (hbox), levels[ii].radio_desc, FALSE, FALSE, 0);

		levels[ii].hbox = hbox;

		gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

		g_signal_connect (levels[ii].combo, "changed",
			G_CALLBACK (on_sort_combo_changed), self);
		g_signal_connect_swapped (levels[ii].radio_asc, "toggled",
			G_CALLBACK (on_any_widget_changed), self);
	}

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	button = gtk_button_new_with_mnemonic (_("C_lear All"));
	g_signal_connect (button, "clicked",
		G_CALLBACK (on_clear_all_clicked), levels);
	g_signal_connect_swapped (button, "clicked",
		G_CALLBACK (on_any_widget_changed), self);
	gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	return vbox;
}

static void
apply_sort_levels_to_key_file (EVirtualTreeCustomizePopover *self,
			       SortLevelWidgets *levels,
			       GKeyFile *key_file,
			       gboolean is_group)
{
	guint ii;

	for (ii = 0; ii < MAX_SORT_LEVELS; ii++) {
		gint col_idx;
		const gchar *column_id;
		gchar *group;
		gboolean descending;
		const gchar *order_key, *dir_key;

		if (!gtk_widget_get_sensitive (levels[ii].hbox))
			continue;

		col_idx = combo_get_selected_column (GTK_COMBO_BOX_TEXT (levels[ii].combo));
		if (col_idx < 0)
			continue;

		column_id = e_virtual_tree_get_column_id (self->vtree, (guint) col_idx);
		if (!column_id)
			continue;

		group = g_strdup_printf ("Column-%s", column_id);

		descending = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (levels[ii].radio_desc));

		if (is_group) {
			order_key = "group-order";
			dir_key = "group-dir";
		} else {
			order_key = "sort-order";
			dir_key = "sort-dir";
		}

		g_key_file_set_integer (key_file, group, order_key, (gint) ii);
		g_key_file_set_string (key_file, group, dir_key,
			descending ? "descending" : "ascending");

		g_free (group);
	}
}

static void
on_ok_clicked (GtkButton *button,
	       gpointer user_data)
{
	EVirtualTreeCustomizePopover *self = user_data;
	GKeyFile *key_file;
	GtkTreeIter iter;
	guint order;
	gboolean valid;

	key_file = g_key_file_new ();

	order = 0;
	valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self->fields_store), &iter);
	while (valid) {
		gboolean visible;
		guint col_idx;
		const gchar *column_id;
		gchar *group;

		gtk_tree_model_get (GTK_TREE_MODEL (self->fields_store), &iter,
			FIELDS_COL_VISIBLE, &visible,
			FIELDS_COL_INDEX, &col_idx,
			-1);

		column_id = e_virtual_tree_get_column_id (self->vtree, col_idx);
		if (column_id) {
			group = g_strdup_printf ("Column-%s", column_id);

			g_key_file_set_integer (key_file, group, "order", (gint) order);
			g_key_file_set_boolean (key_file, group, "visible", visible);

			if (visible) {
				GtkTreeViewColumn *tvc = e_virtual_tree_get_column (self->vtree, col_idx);
				gint width = gtk_tree_view_column_get_width (tvc);

				if (width > 0)
					g_key_file_set_integer (key_file, group, "width", width);
			}

			g_free (group);
		}

		order++;
		valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (self->fields_store), &iter);
	}

	apply_sort_levels_to_key_file (self, self->sort_levels, key_file, FALSE);

	if (self->group_page)
		apply_sort_levels_to_key_file (self, self->group_levels, key_file, TRUE);

	e_virtual_tree_load_column_state_from_key_file (self->vtree, key_file);

	g_key_file_unref (key_file);

	self->has_changes = FALSE;
	gtk_popover_popdown (GTK_POPOVER (self));
}

static void
on_cancel_clicked (GtkButton *button,
		   gpointer user_data)
{
	EVirtualTreeCustomizePopover *self = user_data;

	self->has_changes = FALSE;
	gtk_popover_popdown (GTK_POPOVER (self));
}

static void
e_virtual_tree_customize_popover_hide (GtkWidget *widget)
{
	EVirtualTreeCustomizePopover *self = E_VIRTUAL_TREE_CUSTOMIZE_POPOVER (widget);

	if (self->has_changes)
		return;

	GTK_WIDGET_CLASS (e_virtual_tree_customize_popover_parent_class)->hide (widget);
}

static gboolean
e_virtual_tree_customize_popover_button_release (GtkWidget *widget,
						  GdkEventButton *event)
{
	EVirtualTreeCustomizePopover *self = E_VIRTUAL_TREE_CUSTOMIZE_POPOVER (widget);

	if (self->has_changes) {
		GtkWidget *child, *event_widget;

		child = gtk_bin_get_child (GTK_BIN (widget));
		event_widget = gtk_get_event_widget ((GdkEvent *) event);

		if (child && event->window == gtk_widget_get_window (widget)) {
			GtkAllocation child_alloc;

			gtk_widget_get_allocation (child, &child_alloc);

			if (event->x < child_alloc.x ||
			    event->x > child_alloc.x + child_alloc.width ||
			    event->y < child_alloc.y ||
			    event->y > child_alloc.y + child_alloc.height)
				return GDK_EVENT_STOP;
		} else if (!event_widget || !gtk_widget_is_ancestor (event_widget, widget)) {
			return GDK_EVENT_STOP;
		}
	}

	return GTK_WIDGET_CLASS (e_virtual_tree_customize_popover_parent_class)->button_release_event (widget, event);
}

static gboolean
e_virtual_tree_customize_popover_key_press (GtkWidget *widget,
					     GdkEventKey *event)
{
	EVirtualTreeCustomizePopover *self = E_VIRTUAL_TREE_CUSTOMIZE_POPOVER (widget);

	if (event->keyval == GDK_KEY_Escape && self->has_changes)
		return GDK_EVENT_STOP;

	return GTK_WIDGET_CLASS (e_virtual_tree_customize_popover_parent_class)->key_press_event (widget, event);
}

static void
e_virtual_tree_customize_popover_show (GtkWidget *widget)
{
	EVirtualTreeCustomizePopover *self = E_VIRTUAL_TREE_CUSTOMIZE_POPOVER (widget);

	self->has_changes = FALSE;

	populate_fields_list (self);
	populate_sort_combos (self, self->sort_levels, FALSE);

	if (self->group_page)
		populate_sort_combos (self, self->group_levels, TRUE);

	self->has_changes = FALSE;

	GTK_WIDGET_CLASS (e_virtual_tree_customize_popover_parent_class)->show (widget);
}

static void
e_virtual_tree_customize_popover_dispose (GObject *object)
{
	EVirtualTreeCustomizePopover *self = E_VIRTUAL_TREE_CUSTOMIZE_POPOVER (object);

	g_clear_object (&self->fields_store);

	G_OBJECT_CLASS (e_virtual_tree_customize_popover_parent_class)->dispose (object);
}

static void
e_virtual_tree_customize_popover_class_init (EVirtualTreeCustomizePopoverClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->dispose = e_virtual_tree_customize_popover_dispose;

	widget_class->show = e_virtual_tree_customize_popover_show;
	widget_class->hide = e_virtual_tree_customize_popover_hide;
	widget_class->button_release_event = e_virtual_tree_customize_popover_button_release;
	widget_class->key_press_event = e_virtual_tree_customize_popover_key_press;
}

static void
e_virtual_tree_customize_popover_init (EVirtualTreeCustomizePopover *self)
{
}

GtkWidget *
e_virtual_tree_customize_popover_new (EVirtualTree *vtree)
{
	EVirtualTreeCustomizePopover *self;
	GtkWidget *vbox, *button_box, *button, *fields_tab, *sort_tab;

	g_return_val_if_fail (E_IS_VIRTUAL_TREE (vtree), NULL);

	self = g_object_new (E_TYPE_VIRTUAL_TREE_CUSTOMIZE_POPOVER,
		"modal", TRUE,
		NULL);

	self->vtree = vtree;

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_set_margin_start (vbox, 12);
	gtk_widget_set_margin_end (vbox, 12);
	gtk_widget_set_margin_top (vbox, 12);
	gtk_widget_set_margin_bottom (vbox, 12);

	self->notebook = gtk_notebook_new ();
	gtk_widget_set_size_request (self->notebook, 500, -1);

	fields_tab = create_fields_tab (self);
	gtk_notebook_append_page (GTK_NOTEBOOK (self->notebook), fields_tab,
		gtk_label_new_with_mnemonic (_("_Fields")));

	sort_tab = create_sort_group_tab (self, self->sort_levels, _("Sort by"));
	gtk_notebook_append_page (GTK_NOTEBOOK (self->notebook), sort_tab,
		gtk_label_new_with_mnemonic (_("_Sorting")));

	if (e_virtual_tree_get_allow_grouping (vtree)) {
		self->group_page = create_sort_group_tab (self, self->group_levels, _("Group by"));
		gtk_notebook_append_page (GTK_NOTEBOOK (self->notebook), self->group_page,
			gtk_label_new_with_mnemonic (_("_Grouping")));
	}

	gtk_box_pack_start (GTK_BOX (vbox), self->notebook, TRUE, TRUE, 0);

	button_box = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (button_box), GTK_BUTTONBOX_END);
	gtk_box_set_spacing (GTK_BOX (button_box), 6);

	button = gtk_button_new_with_mnemonic (_("_Cancel"));
	g_signal_connect (button, "clicked",
		G_CALLBACK (on_cancel_clicked), self);
	gtk_container_add (GTK_CONTAINER (button_box), button);

	button = gtk_button_new_with_mnemonic (_("_OK"));
	gtk_style_context_add_class (gtk_widget_get_style_context (button), "suggested-action");
	g_signal_connect (button, "clicked",
		G_CALLBACK (on_ok_clicked), self);
	gtk_container_add (GTK_CONTAINER (button_box), button);

	gtk_box_pack_start (GTK_BOX (vbox), button_box, FALSE, FALSE, 0);

	gtk_container_add (GTK_CONTAINER (self), vbox);
	gtk_widget_show_all (vbox);

	return GTK_WIDGET (self);
}
