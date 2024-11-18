/*
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
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
 */

#include "evolution-config.h"

#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n-lib.h>

#include <libedataserver/libedataserver.h>

#include "e-categories-editor.h"
#include "e-categories-selector.h"
#include "e-category-completion.h"
#include "e-category-editor.h"
#include "e-dialog-widgets.h"
#include "e-misc-utils.h"

struct _ECategoriesEditorPrivate {
	ECategoriesSelector *categories_list;
	GtkWidget *categories_entry;
	GtkWidget *categories_entry_label;
	GtkWidget *new_button;
	GtkWidget *edit_button;
	GtkWidget *delete_button;

	guint ignore_category_changes : 1;
	gulong category_checked_handler_id;
	gulong entry_changed_handler_id;
};

enum {
	COLUMN_ACTIVE,
	COLUMN_ICON,
	COLUMN_CATEGORY,
	N_COLUMNS
};

enum {
	PROP_0,
	PROP_ENTRY_VISIBLE
};

enum {
	ENTRY_CHANGED,
	LAST_SIGNAL
};

static gint signals[LAST_SIGNAL] = {0};

G_DEFINE_TYPE_WITH_PRIVATE (ECategoriesEditor, e_categories_editor, GTK_TYPE_GRID)

static void
entry_changed_cb (GtkEntry *entry,
                  ECategoriesEditor *editor)
{
	g_signal_handler_block (editor->priv->categories_list, editor->priv->category_checked_handler_id);
	e_categories_selector_set_checked (editor->priv->categories_list, gtk_entry_get_text (entry));
	g_signal_handler_unblock (editor->priv->categories_list, editor->priv->category_checked_handler_id);

	g_signal_emit (editor, signals[ENTRY_CHANGED], 0);
}

static void
categories_editor_selection_changed_cb (ECategoriesEditor *editor,
                                        GtkTreeSelection *selection)
{
	GtkWidget *widget;
	gint n_rows;

	n_rows = gtk_tree_selection_count_selected_rows (selection);

	widget = editor->priv->edit_button;
	gtk_widget_set_sensitive (widget, n_rows == 1);

	widget = editor->priv->delete_button;
	gtk_widget_set_sensitive (widget, n_rows >= 1);
}

static void
categories_editor_update_entry (ECategoriesEditor *editor)
{
	GtkEntry *entry;
	gchar *categories;

	entry = GTK_ENTRY (editor->priv->categories_entry);
	categories = e_categories_selector_get_checked (editor->priv->categories_list);

	g_signal_handler_block (entry, editor->priv->entry_changed_handler_id);
	gtk_entry_set_text (entry, categories);
	g_signal_handler_unblock (entry, editor->priv->entry_changed_handler_id);

	g_free (categories);
}

static void
categories_editor_category_checked_cb (ECategoriesEditor *editor)
{
	categories_editor_update_entry (editor);
	g_signal_emit (editor, signals[ENTRY_CHANGED], 0);
}

static void
new_button_clicked_cb (GtkButton *button,
                       ECategoriesEditor *editor)
{
	GtkWidget *toplevel, *parent;
	ECategoryEditor *cat_editor;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (editor));
	if (GTK_IS_WINDOW (toplevel))
		parent = toplevel;
	else
		parent = NULL;

	cat_editor = g_object_new (E_TYPE_CATEGORY_EDITOR,
		"transient-for", parent,
		NULL);

	e_category_editor_create_category (cat_editor);

	gtk_widget_destroy (GTK_WIDGET (cat_editor));
}

static void
edit_button_clicked_cb (GtkButton *button,
                        ECategoriesEditor *editor)
{
	GtkWidget *toplevel, *parent;
	ECategoryEditor *cat_editor;
	gchar *category;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (editor));
	if (GTK_IS_WINDOW (toplevel))
		parent = toplevel;
	else
		parent = NULL;

	cat_editor = g_object_new (E_TYPE_CATEGORY_EDITOR,
		"transient-for", parent,
		"use-header-bar", e_util_get_use_header_bar (),
		NULL);

	category = e_categories_selector_get_selected (
		editor->priv->categories_list);

	e_category_editor_edit_category (cat_editor, category);

	gtk_widget_destroy (GTK_WIDGET (cat_editor));
	g_free (category);
}

static void
delete_button_clicked_cb (GtkButton *button,
			  ECategoriesEditor *editor)
{
	e_categories_selector_delete_selection (editor->priv->categories_list);
	categories_editor_update_entry (editor);
}

static void
categories_editor_set_property (GObject *object,
                                guint property_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ENTRY_VISIBLE:
			e_categories_editor_set_entry_visible (
				E_CATEGORIES_EDITOR (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
categories_editor_get_property (GObject *object,
                                guint property_id,
                                GValue *value,
                                GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ENTRY_VISIBLE:
			g_value_set_boolean (
				value, e_categories_editor_get_entry_visible (
				E_CATEGORIES_EDITOR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_categories_editor_class_init (ECategoriesEditorClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = categories_editor_set_property;
	object_class->get_property = categories_editor_get_property;

	g_object_class_install_property (
		object_class,
		PROP_ENTRY_VISIBLE,
		g_param_spec_boolean (
			"entry-visible",
			NULL,
			NULL,
			TRUE,
			G_PARAM_READWRITE));

	signals[ENTRY_CHANGED] = g_signal_new (
		"entry-changed",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (ECategoriesEditorClass, entry_changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
e_categories_editor_init (ECategoriesEditor *editor)
{
	GtkEntryCompletion *completion;
	GtkGrid *grid;
	GtkWidget *entry_categories;
	GtkWidget *label_header;
	GtkWidget *label2;
	GtkWidget *scrolledwindow1;
	GtkWidget *categories_list;
	GtkWidget *hbuttonbox1;
	GtkWidget *button_new;
	GtkWidget *button_edit;
	GtkWidget *button_delete;

	editor->priv = e_categories_editor_get_instance_private (editor);

	gtk_widget_set_size_request (GTK_WIDGET (editor), -1, 400);

	grid = GTK_GRID (editor);

	gtk_grid_set_row_spacing (grid, 6);
	gtk_grid_set_column_spacing (grid, 6);

	label_header = gtk_label_new_with_mnemonic (
		_("Currently _used categories:"));
	gtk_widget_set_halign (label_header, GTK_ALIGN_FILL);
	gtk_grid_attach (grid, label_header, 0, 0, 1, 1);
	gtk_label_set_justify (GTK_LABEL (label_header), GTK_JUSTIFY_CENTER);
	gtk_label_set_xalign (GTK_LABEL (label_header), 0);

	entry_categories = gtk_entry_new ();
	gtk_widget_set_hexpand (entry_categories, TRUE);
	gtk_widget_set_halign (entry_categories, GTK_ALIGN_FILL);
	gtk_grid_attach (grid, entry_categories, 0, 1, 1, 1);

	label2 = gtk_label_new_with_mnemonic (_("_Available Categories:"));
	gtk_widget_set_halign (label2, GTK_ALIGN_FILL);
	gtk_grid_attach (grid, label2, 0, 2, 1, 1);
	gtk_label_set_justify (GTK_LABEL (label2), GTK_JUSTIFY_CENTER);
	gtk_label_set_xalign (GTK_LABEL (label2), 0);

	scrolledwindow1 = gtk_scrolled_window_new (NULL, NULL);
	g_object_set (
		G_OBJECT (scrolledwindow1),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		NULL);
	gtk_grid_attach (grid, scrolledwindow1, 0, 3, 1, 1);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (scrolledwindow1),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (scrolledwindow1), GTK_SHADOW_IN);

	categories_list = GTK_WIDGET (e_categories_selector_new ());
	gtk_container_add (GTK_CONTAINER (scrolledwindow1), categories_list);
	gtk_widget_set_size_request (categories_list, -1, 350);
	gtk_tree_view_set_headers_visible (
		GTK_TREE_VIEW (categories_list), FALSE);
	gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (categories_list), TRUE);
	editor->priv->category_checked_handler_id = g_signal_connect_swapped (
		G_OBJECT (categories_list), "category-checked",
		G_CALLBACK (categories_editor_category_checked_cb), editor);

	hbuttonbox1 = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
	g_object_set (
		G_OBJECT (hbuttonbox1),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		NULL);
	gtk_grid_attach (grid, hbuttonbox1, 0, 4, 1, 1);
	gtk_box_set_spacing (GTK_BOX (hbuttonbox1), 6);

	button_new = e_dialog_button_new_with_icon ("document-new", C_("category", "_New"));
	gtk_container_add (GTK_CONTAINER (hbuttonbox1), button_new);
	gtk_widget_set_can_default (button_new, TRUE);

	button_edit = gtk_button_new_with_mnemonic (C_("category", "_Edit"));
	gtk_container_add (GTK_CONTAINER (hbuttonbox1), button_edit);
	gtk_widget_set_can_default (button_edit, TRUE);

	button_delete = e_dialog_button_new_with_icon ("edit-delete", C_("category", "_Delete"));
	gtk_container_add (GTK_CONTAINER (hbuttonbox1), button_delete);
	gtk_widget_set_can_default (button_delete, TRUE);

	gtk_label_set_mnemonic_widget (
		GTK_LABEL (label_header), entry_categories);
	gtk_label_set_mnemonic_widget (
		GTK_LABEL (label2), categories_list);

	editor->priv->categories_list = E_CATEGORIES_SELECTOR (categories_list);
	editor->priv->categories_entry = entry_categories;
	editor->priv->categories_entry_label = label_header;

	g_signal_connect_swapped (
		editor->priv->categories_list, "selection-changed",
		G_CALLBACK (categories_editor_selection_changed_cb), editor);

	completion = e_category_completion_new ();
	gtk_entry_set_completion (
		GTK_ENTRY (editor->priv->categories_entry), completion);
	g_object_unref (completion);

	editor->priv->new_button = button_new;
	g_signal_connect (
		editor->priv->new_button, "clicked",
		G_CALLBACK (new_button_clicked_cb), editor);

	editor->priv->edit_button = button_edit;
	g_signal_connect (
		editor->priv->edit_button, "clicked",
		G_CALLBACK (edit_button_clicked_cb), editor);

	editor->priv->delete_button = button_delete;
	g_signal_connect (
		editor->priv->delete_button, "clicked",
		G_CALLBACK (delete_button_clicked_cb),
		editor);

	editor->priv->entry_changed_handler_id = g_signal_connect (
		editor->priv->categories_entry, "changed",
		G_CALLBACK (entry_changed_cb), editor);

	gtk_widget_show_all (GTK_WIDGET (editor));
}

/**
 * e_categories_editor_new:
 *
 * Creates a new #ECategoriesEditor widget.
 *
 * Returns: a new #ECategoriesEditor
 *
 * Since: 3.2
 **/
GtkWidget *
e_categories_editor_new (void)
{
	return g_object_new (E_TYPE_CATEGORIES_EDITOR, NULL);
}

/**
 * e_categories_editor_get_categories:
 * @editor: an #ECategoriesEditor
 *
 * Gets a comma-separated list of the categories currently selected
 * in the editor.
 *
 * Returns: a comma-separated list of categories. Free returned
 * pointer with g_free().
 *
 * Since: 3.2
 **/
gchar *
e_categories_editor_get_categories (ECategoriesEditor *editor)
{
	g_return_val_if_fail (E_IS_CATEGORIES_EDITOR (editor), NULL);

	if (e_categories_editor_get_entry_visible (editor)) {
		GString *categories;
		gchar **split;
		gint ii;

		categories = g_string_new ("");

		split = g_strsplit (gtk_entry_get_text (GTK_ENTRY (editor->priv->categories_entry)), ",", 0);

		if (split) {
			GHashTable *known;
			GSList *items = NULL, *link;

			known = g_hash_table_new (g_str_hash, g_str_equal);

			for (ii = 0; split[ii] != NULL; ii++) {
				gchar *value = g_strstrip (split[ii]);

				if (*value && g_hash_table_insert (known, value, GINT_TO_POINTER (1)))
					items = g_slist_prepend (items, value);
			}

			items = g_slist_sort (items, e_collate_compare);

			for (link = items; link; link = g_slist_next (link)) {
				if (categories->len)
					g_string_append_c (categories, ',');
				g_string_append (categories, link->data);
			}

			g_hash_table_destroy (known);
			g_slist_free (items);
			g_strfreev (split);
		}

		return g_string_free (categories, FALSE);
	}

	return e_categories_selector_get_checked (editor->priv->categories_list);
}

/**
 * e_categories_editor_set_categories:
 * @editor: an #ECategoriesEditor
 * @categories: comma-separated list of categories
 *
 * Sets the list of categories selected on the editor.
 *
 * Since: 3.2
 **/
void
e_categories_editor_set_categories (ECategoriesEditor *editor,
                                    const gchar *categories)
{
	g_return_if_fail (E_IS_CATEGORIES_EDITOR (editor));

	e_categories_selector_set_checked (editor->priv->categories_list, categories);
	categories_editor_update_entry (editor);
}

/**
 * e_categories_editor_get_entry_visible:
 * @editor: an #ECategoriesEditor
 *
 * Return the visibility of the category input entry.
 *
 * Returns: whether the entry is visible
 *
 * Since: 3.2
 **/
gboolean
e_categories_editor_get_entry_visible (ECategoriesEditor *editor)
{
	g_return_val_if_fail (E_IS_CATEGORIES_EDITOR (editor), TRUE);

	return gtk_widget_get_visible (editor->priv->categories_entry);
}

/**
 * e_categories_editor_set_entry_visible:
 * @editor: an #ECategoriesEditor
 * @entry_visible: whether to make the entry visible
 *
 * Sets the visibility of the category input entry.
 *
 * Since: 3.2
 **/
void
e_categories_editor_set_entry_visible (ECategoriesEditor *editor,
                                       gboolean entry_visible)
{
	g_return_if_fail (E_IS_CATEGORIES_EDITOR (editor));

	if ((gtk_widget_get_visible (editor->priv->categories_entry) ? 1 : 0) ==
	    (entry_visible ? 1 : 0))
		return;

	gtk_widget_set_visible (
		editor->priv->categories_entry, entry_visible);
	gtk_widget_set_visible (
		editor->priv->categories_entry_label, entry_visible);
	e_categories_selector_set_items_checkable (
		editor->priv->categories_list, entry_visible);

	g_object_notify (G_OBJECT (editor), "entry-visible");
}
