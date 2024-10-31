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

#include <glib/gi18n-lib.h>

#include <libedataserver/libedataserver.h>

#include "e-categories-config.h"
#include "e-categories-selector.h"

struct _ECategoriesSelectorPrivate {
	gboolean checkable;
	gboolean use_inconsistent;
	GHashTable *selected_categories;

	gboolean ignore_category_changes;
};

enum {
	PROP_0,
	PROP_ITEMS_CHECKABLE,
	PROP_USE_INCONSISTENT
};

enum {
	CATEGORY_CHECKED,
	SELECTION_CHANGED,
	LAST_SIGNAL
};

enum {
	COLUMN_ACTIVE,
	COLUMN_ICON,
	COLUMN_CATEGORY,
	COLUMN_INCONSISTENT,
	N_COLUMNS
};

static gint signals[LAST_SIGNAL] = {0};

G_DEFINE_TYPE_WITH_PRIVATE (ECategoriesSelector, e_categories_selector, GTK_TYPE_TREE_VIEW)

static void
categories_selector_build_model (ECategoriesSelector *selector)
{
	GtkListStore *store;
	GList *list, *link;

	store = gtk_list_store_new (
		N_COLUMNS, G_TYPE_BOOLEAN, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_BOOLEAN);

	gtk_tree_sortable_set_sort_column_id (
		GTK_TREE_SORTABLE (store),
		COLUMN_CATEGORY, GTK_SORT_ASCENDING);

	list = e_categories_dup_list ();
	for (link = list; link != NULL; link = g_list_next (link)) {
		const gchar *category_name = link->data;
		GdkPixbuf *pixbuf = NULL;
		GtkTreeIter iter;
		gboolean active;

		/* Only add user-visible categories. */
		if (!e_categories_is_searchable (category_name))
			continue;

		active = (g_hash_table_lookup (
				selector->priv->selected_categories,
				category_name) != NULL);

		if (!e_categories_config_get_icon_for (category_name, &pixbuf))
			pixbuf = NULL;

		gtk_list_store_append (store, &iter);

		gtk_list_store_set (
			store, &iter,
			COLUMN_ACTIVE, active,
			COLUMN_ICON, pixbuf,
			COLUMN_CATEGORY, category_name,
			COLUMN_INCONSISTENT, selector->priv->use_inconsistent,
			-1);

		if (pixbuf != NULL)
			g_object_unref (pixbuf);
	}

	gtk_tree_view_set_model (
		GTK_TREE_VIEW (selector), GTK_TREE_MODEL (store));

	/* This has to be reset everytime we install a new model */
	gtk_tree_view_set_search_column (
		GTK_TREE_VIEW (selector), COLUMN_CATEGORY);

	g_list_free_full (list, g_free);
	g_object_unref (store);
}

static void
category_toggled_cb (GtkCellRenderer *renderer,
                     const gchar *path,
                     ECategoriesSelector *selector)
{
	GtkTreeModel *model;
	GtkTreePath *tree_path;
	GtkTreeIter iter;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (selector));
	g_return_if_fail (model);

	tree_path = gtk_tree_path_new_from_string (path);
	g_return_if_fail (tree_path);

	if (gtk_tree_model_get_iter (model, &iter, tree_path)) {
		gchar *category;
		gboolean inconsistent;
		gboolean active;

		gtk_tree_model_get (
			model, &iter,
			COLUMN_ACTIVE, &active,
			COLUMN_CATEGORY, &category,
			COLUMN_INCONSISTENT, &inconsistent,
			-1);

		if (selector->priv->use_inconsistent) {
			if (!active && !inconsistent) {
				active = TRUE;
				inconsistent = TRUE;
			} else if (inconsistent) {
				inconsistent = FALSE;
			} else {
				active = !active;
			}

			gtk_list_store_set (
				GTK_LIST_STORE (model), &iter,
				COLUMN_ACTIVE, active,
				COLUMN_INCONSISTENT, inconsistent,
				-1);
		} else {
			active = !active;

			gtk_list_store_set (
				GTK_LIST_STORE (model), &iter,
				COLUMN_ACTIVE, active, -1);
		}

		if (active) {
			g_hash_table_insert (
				selector->priv->selected_categories,
				g_strdup (category), g_strdup (category));
		} else {
			g_hash_table_remove (
				selector->priv->selected_categories, category);
		}

		g_signal_emit (
			selector, signals[CATEGORY_CHECKED], 0,
			category, active);

		g_free (category);
	}

	gtk_tree_path_free (tree_path);
}

static void
categories_selector_listener_cb (gpointer useless_pointer,
                                 ECategoriesSelector *selector)
{
	if (!selector->priv->ignore_category_changes)
		categories_selector_build_model (selector);
}

static gboolean
categories_selector_key_press_event (ECategoriesSelector *selector,
                                     GdkEventKey *event)
{
	if (event->keyval == GDK_KEY_Delete) {
		e_categories_selector_delete_selection (selector);
		return TRUE;
	}

	return FALSE;
}

static void
categories_selector_selection_changed (GtkTreeSelection *selection,
                                       ECategoriesSelector *selector)
{
	g_signal_emit (selector, signals[SELECTION_CHANGED], 0, selection);
}

static void
categories_selector_get_property (GObject *object,
                                  guint property_id,
                                  GValue *value,
                                  GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ITEMS_CHECKABLE:
			g_value_set_boolean (
				value,
				e_categories_selector_get_items_checkable (
				E_CATEGORIES_SELECTOR (object)));
			return;
		case PROP_USE_INCONSISTENT:
			g_value_set_boolean (
				value,
				e_categories_selector_get_use_inconsistent (
				E_CATEGORIES_SELECTOR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
categories_selector_set_property (GObject *object,
                                  guint property_id,
                                  const GValue *value,
                                  GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ITEMS_CHECKABLE:
			e_categories_selector_set_items_checkable (
				E_CATEGORIES_SELECTOR (object),
				g_value_get_boolean (value));
			return;
		case PROP_USE_INCONSISTENT:
			e_categories_selector_set_use_inconsistent (
				E_CATEGORIES_SELECTOR (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
categories_selector_dispose (GObject *object)
{
	ECategoriesSelector *self = E_CATEGORIES_SELECTOR (object);

	g_clear_pointer (&self->priv->selected_categories, g_hash_table_destroy);

	/* Chain up to parent's dispose() method.*/
	G_OBJECT_CLASS (e_categories_selector_parent_class)->dispose (object);
}

static void
categories_selector_finalize (GObject *object)
{
	e_categories_unregister_change_listener (
		G_CALLBACK (categories_selector_listener_cb), object);
}

static void
e_categories_selector_class_init (ECategoriesSelectorClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = categories_selector_set_property;
	object_class->get_property = categories_selector_get_property;
	object_class->dispose = categories_selector_dispose;
	object_class->finalize = categories_selector_finalize;

	g_object_class_install_property (
		object_class,
		PROP_ITEMS_CHECKABLE,
		g_param_spec_boolean (
			"items-checkable",
			NULL,
			NULL,
			TRUE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_USE_INCONSISTENT,
		g_param_spec_boolean (
			"use-inconsistent",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

	signals[CATEGORY_CHECKED] = g_signal_new (
		"category-checked",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (ECategoriesSelectorClass, category_checked),
		NULL, NULL, NULL,
		G_TYPE_NONE, 2,
		G_TYPE_STRING,
		G_TYPE_BOOLEAN);

	signals[SELECTION_CHANGED] = g_signal_new (
		"selection-changed",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (ECategoriesSelectorClass, selection_changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		GTK_TYPE_TREE_SELECTION);
}

static void
e_categories_selector_init (ECategoriesSelector *selector)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;

	selector->priv = e_categories_selector_get_instance_private (selector);

	selector->priv->checkable = TRUE;
	selector->priv->selected_categories = g_hash_table_new_full (
		(GHashFunc) g_str_hash,
		(GEqualFunc) g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_free);
	selector->priv->ignore_category_changes = FALSE;

	renderer = gtk_cell_renderer_toggle_new ();
	column = gtk_tree_view_column_new_with_attributes (
		"?", renderer, "active", COLUMN_ACTIVE, "inconsistent", COLUMN_INCONSISTENT, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (selector), column);

	g_signal_connect (
		renderer, "toggled",
		G_CALLBACK (category_toggled_cb), selector);

	renderer = gtk_cell_renderer_pixbuf_new ();
	column = gtk_tree_view_column_new_with_attributes (
		_("Icon"), renderer, "pixbuf", COLUMN_ICON, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (selector), column);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (
		_("Category"), renderer, "text", COLUMN_CATEGORY, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (selector), column);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (selector));
	g_signal_connect (
		selection, "changed",
		G_CALLBACK (categories_selector_selection_changed), selector);

	g_signal_connect (
		selector, "key-press-event",
		G_CALLBACK (categories_selector_key_press_event), NULL);

	e_categories_register_change_listener (
		G_CALLBACK (categories_selector_listener_cb), selector);

	categories_selector_build_model (selector);
}

/**
 * e_categories_selector_new:
 *
 * Since: 3.2
 **/
GtkWidget *
e_categories_selector_new (void)
{
	return g_object_new (
		E_TYPE_CATEGORIES_SELECTOR,
		"items-checkable", TRUE, NULL);
}

/**
 * e_categories_selector_get_items_checkable:
 *
 * Since: 3.2
 **/
gboolean
e_categories_selector_get_items_checkable (ECategoriesSelector *selector)
{
	g_return_val_if_fail (E_IS_CATEGORIES_SELECTOR (selector), TRUE);

	return selector->priv->checkable;
}

/**
 * e_categories_selector_set_items_checkable:
 *
 * Since: 3.2
 **/
void
e_categories_selector_set_items_checkable (ECategoriesSelector *selector,
                                           gboolean checkable)
{
	GtkTreeViewColumn *column;

	g_return_if_fail (E_IS_CATEGORIES_SELECTOR (selector));

	if ((selector->priv->checkable ? 1 : 0) == (checkable ? 1 : 0))
		return;

	selector->priv->checkable = checkable;

	column = gtk_tree_view_get_column (
		GTK_TREE_VIEW (selector), COLUMN_ACTIVE);
	gtk_tree_view_column_set_visible (column, checkable);

	g_object_notify (G_OBJECT (selector), "items-checkable");
}

/**
 * e_categories_selector_get_checked:
 *
 * Free returned pointer with g_free().
 *
 * Since: 3.2
 **/
gchar *
e_categories_selector_get_checked (ECategoriesSelector *selector)
{
	GString *str;
	GList *list, *category;

	g_return_val_if_fail (E_IS_CATEGORIES_SELECTOR (selector), NULL);

	str = g_string_new ("");
	list = g_hash_table_get_values (selector->priv->selected_categories);

	/* to get them always in the same order */
	list = g_list_sort (list, (GCompareFunc) g_utf8_collate);

	for (category = list; category != NULL; category = category->next) {
		if (str->len > 0)
			g_string_append_printf (
				str, ",%s", (gchar *) category->data);
		else
			g_string_append (str, (gchar *) category->data);
	}

	g_list_free (list);

	return g_string_free (str, FALSE);
}

/**
 * e_categories_selector_set_checked:
 *
 * Since: 3.2
 **/
void
e_categories_selector_set_checked (ECategoriesSelector *selector,
                                   const gchar *categories)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar **arr;
	gint i;

	g_return_if_fail (E_IS_CATEGORIES_SELECTOR (selector));

	/* Clean up table of selected categories. */
	g_hash_table_remove_all (selector->priv->selected_categories);

	arr = g_strsplit (categories, ",", 0);
	if (arr) {
		for (i = 0; arr[i] != NULL; i++) {
			g_strstrip (arr[i]);
			g_hash_table_insert (
				selector->priv->selected_categories,
				g_strdup (arr[i]), g_strdup (arr[i]));
		}
		g_strfreev (arr);
	}

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (selector));
	if (gtk_tree_model_get_iter_first (model, &iter)) {
		do {
			gchar *category_name;
			gboolean found;

			gtk_tree_model_get (
				model, &iter,
				COLUMN_CATEGORY, &category_name,
				-1);
			found = (g_hash_table_lookup (
				selector->priv->selected_categories,
				category_name) != NULL);
			gtk_list_store_set (
				GTK_LIST_STORE (model), &iter,
				COLUMN_ACTIVE, found, -1);

			g_free (category_name);
		} while (gtk_tree_model_iter_next (model, &iter));
	}
}

/**
 * e_categories_selector_delete_selection:
 *
 * Since: 3.2
 **/
void
e_categories_selector_delete_selection (ECategoriesSelector *selector)
{
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GList *selected, *item;

	g_return_if_fail (E_IS_CATEGORIES_SELECTOR (selector));

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (selector));
	g_return_if_fail (model != NULL);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (selector));
	selected = gtk_tree_selection_get_selected_rows (selection, &model);

	/* Remove categories in reverse order to avoid invalidating
	 * tree paths as we iterate over the list. Note, the list is
	 * probably already sorted but we sort again just to be safe. */
	selected = g_list_reverse (g_list_sort (
		selected, (GCompareFunc) gtk_tree_path_compare));

	/* Prevent the model from being rebuilt every time we
	 * remove a category, since we're already modifying it. */
	selector->priv->ignore_category_changes = TRUE;

	for (item = selected; item != NULL; item = item->next) {
		GtkTreePath *path = item->data;
		GtkTreeIter iter;
		gchar *category;

		gtk_tree_model_get_iter (model, &iter, path);
		gtk_tree_model_get (
			model, &iter,
			COLUMN_CATEGORY, &category, -1);
		gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
		e_categories_remove (category);

		if (g_hash_table_remove (selector->priv->selected_categories, category)) {
			g_signal_emit (
				selector, signals[CATEGORY_CHECKED], 0,
				category, FALSE);
		}

		g_free (category);
	}

	selector->priv->ignore_category_changes = FALSE;

	/* If we only remove one category, try to select another */
	if (selected && selected->data && !selected->next) {
		GtkTreePath *path = selected->data;

		gtk_tree_selection_select_path (selection, path);
		if (!gtk_tree_selection_path_is_selected (selection, path))
			if (gtk_tree_path_prev (path))
				gtk_tree_selection_select_path (selection, path);
	}

	g_list_foreach (selected, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (selected);
}

/**
 * e_categories_selector_get_selected:
 *
 * Free returned pointer with g_free().
 *
 * Since: 3.2
 **/
gchar *
e_categories_selector_get_selected (ECategoriesSelector *selector)
{
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GList *selected, *item;
	GString *str = g_string_new ("");

	g_return_val_if_fail (E_IS_CATEGORIES_SELECTOR (selector), NULL);

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (selector));
	g_return_val_if_fail (model != NULL, NULL);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (selector));
	selected = gtk_tree_selection_get_selected_rows (selection, &model);

	for (item = selected; item != NULL; item = item->next) {
		GtkTreePath *path = item->data;
		GtkTreeIter iter;
		gchar *category;

		gtk_tree_model_get_iter (model, &iter, path);
		gtk_tree_model_get (
			model, &iter,
			COLUMN_CATEGORY, &category, -1);
		if (str->len == 0)
			g_string_assign (str, category);
		else
			g_string_append_printf (str, ",%s", category);

		g_free (category);
	}

	g_list_foreach (selected, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (selected);

	return g_string_free (str, FALSE);
}

gboolean
e_categories_selector_get_use_inconsistent (ECategoriesSelector *selector)
{
	g_return_val_if_fail (E_IS_CATEGORIES_SELECTOR (selector), FALSE);

	return selector->priv->use_inconsistent;
}

void
e_categories_selector_set_use_inconsistent (ECategoriesSelector *selector,
					    gboolean use_inconsistent)
{
	g_return_if_fail (E_IS_CATEGORIES_SELECTOR (selector));

	if ((selector->priv->use_inconsistent ? 1 : 0) != (use_inconsistent ? 1 : 0)) {
		selector->priv->use_inconsistent = use_inconsistent;

		g_object_notify (G_OBJECT (selector), "use-inconsistent");

		categories_selector_build_model (selector);
	}
}

void
e_categories_selector_get_changes (ECategoriesSelector *selector,
				   GHashTable **out_checked, /* gchar * ~> NULL */
				   GHashTable **out_unchecked) /* gchar * ~> NULL */
{
	GtkTreeModel *model;
	GtkTreeIter iter;

	g_return_if_fail (E_IS_CATEGORIES_SELECTOR (selector));
	g_return_if_fail (out_checked != NULL);
	g_return_if_fail (out_unchecked != NULL);

	*out_checked = NULL;
	*out_unchecked = NULL;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (selector));
	g_return_if_fail (model != NULL);

	if (gtk_tree_model_get_iter_first (model, &iter)) {
		do {
			gboolean inconsistent = FALSE;

			gtk_tree_model_get (model, &iter, COLUMN_INCONSISTENT, &inconsistent, -1);

			if (!inconsistent) {
				gboolean active = FALSE;
				gchar *category = NULL;

				gtk_tree_model_get (model, &iter,
					COLUMN_ACTIVE, &active,
					COLUMN_CATEGORY, &category,
					-1);

				if (category) {
					GHashTable **out_hash = active ? out_checked : out_unchecked;

					if (!*out_hash)
						*out_hash = g_hash_table_new_full (camel_strcase_hash, camel_strcase_equal, g_free, NULL);

					g_hash_table_add (*out_hash, category);
				}
			}
		} while (gtk_tree_model_iter_next (model, &iter));
	}
}

gchar *
e_categories_selector_util_apply_changes (const gchar *in_categories,
					  GHashTable *checked,
					  GHashTable *unchecked)
{
	GSList *new_value = NULL;
	gchar **value_split;
	gchar *res = NULL;
	guint ii;

	if (!checked && !unchecked)
		return g_strdup (in_categories);

	value_split = in_categories ? g_strsplit (in_categories, ",", -1) : NULL;

	for (ii = 0; value_split && value_split[ii]; ii++) {
		if (unchecked && g_hash_table_contains (unchecked, value_split[ii])) {
			/* skip it */
		} else if (!checked || !g_hash_table_contains (checked, value_split[ii])) {
			new_value = g_slist_prepend (new_value, value_split[ii]);
		}
	}

	if (checked) {
		gpointer key;
		GHashTableIter iter;

		g_hash_table_iter_init (&iter, checked);
		while (g_hash_table_iter_next (&iter, &key, NULL)) {
			const gchar *category = key;

			new_value = g_slist_prepend (new_value, (gpointer) category);
		}
	}

	if (new_value) {
		GSList *new_value_link;
		GString *str = g_string_new (NULL);

		new_value = g_slist_sort (new_value, (GCompareFunc) g_utf8_collate);

		for (new_value_link = new_value; new_value_link; new_value_link = g_slist_next (new_value_link)) {
			const gchar *category = new_value_link->data;

			if (str->len)
				g_string_append_c (str, ',');

			g_string_append (str, category);
		}

		res = g_string_free (str, FALSE);
	}

	g_slist_free (new_value);
	g_strfreev (value_split);

	return res;
}
