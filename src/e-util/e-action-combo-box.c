/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-action-combo-box.c
 *
 * Copyright (C) 2008 Novell, Inc.
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

#include "e-action-combo-box.h"
#include "e-misc-utils.h"

#include <glib/gi18n.h>

#define E_ACTION_COMBO_BOX_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_ACTION_COMBO_BOX, EActionComboBoxPrivate))

enum {
	COLUMN_ACTION,
	COLUMN_SORT
};

enum {
	PROP_0,
	PROP_ACTION
};

struct _EActionComboBoxPrivate {
	GtkRadioAction *action;
	GtkActionGroup *action_group;
	GHashTable *index;
	guint changed_handler_id;		/* action::changed */
	guint group_sensitive_handler_id;	/* action-group::sensitive */
	guint group_visible_handler_id;		/* action-group::visible */
	gboolean group_has_icons : 1;
};

G_DEFINE_TYPE (
	EActionComboBox,
	e_action_combo_box,
	GTK_TYPE_COMBO_BOX)

static void
action_combo_box_action_changed_cb (GtkRadioAction *action,
                                    GtkRadioAction *current,
                                    EActionComboBox *combo_box)
{
	GtkTreeRowReference *reference;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	gboolean valid;

	reference = g_hash_table_lookup (
		combo_box->priv->index, GINT_TO_POINTER (
		gtk_radio_action_get_current_value (current)));
	g_return_if_fail (reference != NULL);

	model = gtk_tree_row_reference_get_model (reference);
	path = gtk_tree_row_reference_get_path (reference);
	valid = gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_path_free (path);
	g_return_if_fail (valid);

	gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combo_box), &iter);
}

static void
action_combo_box_action_group_notify_cb (GtkActionGroup *action_group,
                                         GParamSpec *pspec,
                                         EActionComboBox *combo_box)
{
	g_object_set (
		combo_box, "sensitive",
		gtk_action_group_get_sensitive (action_group), "visible",
		gtk_action_group_get_visible (action_group), NULL);
}

static void
action_combo_box_render_pixbuf (GtkCellLayout *layout,
                                GtkCellRenderer *renderer,
                                GtkTreeModel *model,
                                GtkTreeIter *iter,
                                EActionComboBox *combo_box)
{
	GtkRadioAction *action;
	gchar *icon_name;
	gchar *stock_id;
	gboolean sensitive;
	gboolean visible;
	gint width;

	gtk_tree_model_get (model, iter, COLUMN_ACTION, &action, -1);

	/* A NULL action means the row is a separator. */
	if (action == NULL)
		return;

	g_object_get (
		G_OBJECT (action),
		"icon-name", &icon_name,
		"sensitive", &sensitive,
		"stock-id", &stock_id,
		"visible", &visible,
		NULL);

	/* If some action has an icon */
	if (combo_box->priv->group_has_icons)
		/* Keep the pixbuf renderer a fixed size for proper alignment. */
		gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &width, NULL);
	else
		width = 0;

	/* We can't set both "icon-name" and "stock-id" because setting
	 * one unsets the other.  So pick the one that has a non-NULL
	 * value.  If both are non-NULL, "stock-id" wins. */

	if (stock_id != NULL)
		g_object_set (
			G_OBJECT (renderer),
			"sensitive", sensitive,
			"icon-name", NULL,
			"stock-id", stock_id,
			"stock-size", GTK_ICON_SIZE_MENU,
			"visible", visible,
			"width", width,
			NULL);
	else
		g_object_set (
			G_OBJECT (renderer),
			"sensitive", sensitive,
			"icon-name", icon_name,
			"stock-id", NULL,
			"stock-size", GTK_ICON_SIZE_MENU,
			"visible", visible,
			"width", width,
			NULL);

	g_object_unref (action);
	g_free (icon_name);
	g_free (stock_id);
}

static void
action_combo_box_render_text (GtkCellLayout *layout,
                              GtkCellRenderer *renderer,
                              GtkTreeModel *model,
                              GtkTreeIter *iter,
                              EActionComboBox *combo_box)
{
	GtkRadioAction *action;
	gchar **strv;
	gchar *label;
	gboolean sensitive;
	gboolean visible;
	gint xpad;

	gtk_tree_model_get (model, iter, COLUMN_ACTION, &action, -1);

	/* A NULL action means the row is a separator. */
	if (action == NULL)
		return;

	g_object_get (
		G_OBJECT (action),
		"label", &label,
		"sensitive", &sensitive,
		"visible", &visible,
		NULL);

	/* Strip out underscores. */
	strv = g_strsplit (label, "_", -1);
	g_free (label);
	label = g_strjoinv (NULL, strv);
	g_strfreev (strv);

	xpad = combo_box->priv->group_has_icons ? 3 : 0;

	g_object_set (
		G_OBJECT (renderer),
		"sensitive", sensitive,
		"text", label,
		"visible", visible,
		"xpad", xpad,
		NULL);

	g_object_unref (action);
	g_free (label);
}

static gboolean
action_combo_box_is_row_separator (GtkTreeModel *model,
                                   GtkTreeIter *iter)
{
	GtkAction *action;
	gboolean separator;

	/* NULL actions are rendered as separators. */
	gtk_tree_model_get (model, iter, COLUMN_ACTION, &action, -1);
	separator = (action == NULL);
	if (action != NULL)
		g_object_unref (action);

	return separator;
}

static void
action_combo_box_update_model (EActionComboBox *combo_box)
{
	GtkListStore *list_store;
	GSList *list;

	g_hash_table_remove_all (combo_box->priv->index);

	if (combo_box->priv->action == NULL) {
		gtk_combo_box_set_model (GTK_COMBO_BOX (combo_box), NULL);
		return;
	}

	/* We store values in the sort column as floats so that we can
	 * insert separators in between consecutive integer values and
	 * still maintain the proper ordering. */
	list_store = gtk_list_store_new (
		2, GTK_TYPE_RADIO_ACTION, G_TYPE_FLOAT);

	list = gtk_radio_action_get_group (combo_box->priv->action);
	combo_box->priv->group_has_icons = FALSE;

	while (list != NULL) {
		GtkTreeRowReference *reference;
		GtkRadioAction *action = list->data;
		GtkTreePath *path;
		GtkTreeIter iter;
		gchar *icon_name = NULL;
		gchar *stock_id = NULL;
		gboolean visible = FALSE;
		gint value;

		g_object_get (action,
			"icon-name", &icon_name,
			"stock-id", &stock_id,
			"visible", &visible,
			NULL);

		if (!visible) {
			g_free (icon_name);
			g_free (stock_id);

			list = g_slist_next (list);
			continue;
		}

		combo_box->priv->group_has_icons |=
			(icon_name != NULL || stock_id != NULL);
		g_free (icon_name);
		g_free (stock_id);

		gtk_list_store_append (list_store, &iter);
		g_object_get (action, "value", &value, NULL);
		gtk_list_store_set (
			list_store, &iter, COLUMN_ACTION,
			list->data, COLUMN_SORT, (gfloat) value, -1);

		path = gtk_tree_model_get_path (
			GTK_TREE_MODEL (list_store), &iter);
		reference = gtk_tree_row_reference_new (
			GTK_TREE_MODEL (list_store), path);
		g_hash_table_insert (
			combo_box->priv->index,
			GINT_TO_POINTER (value), reference);
		gtk_tree_path_free (path);

		list = g_slist_next (list);
	}

	gtk_tree_sortable_set_sort_column_id (
		GTK_TREE_SORTABLE (list_store),
		COLUMN_SORT, GTK_SORT_ASCENDING);
	gtk_combo_box_set_model (
		GTK_COMBO_BOX (combo_box), GTK_TREE_MODEL (list_store));
	g_object_unref (list_store);

	action_combo_box_action_changed_cb (
		combo_box->priv->action,
		combo_box->priv->action,
		combo_box);
}

static void
action_combo_box_set_property (GObject *object,
                               guint property_id,
                               const GValue *value,
                               GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ACTION:
			e_action_combo_box_set_action (
				E_ACTION_COMBO_BOX (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
action_combo_box_get_property (GObject *object,
                               guint property_id,
                               GValue *value,
                               GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ACTION:
			g_value_set_object (
				value, e_action_combo_box_get_action (
				E_ACTION_COMBO_BOX (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
action_combo_box_dispose (GObject *object)
{
	EActionComboBoxPrivate *priv = E_ACTION_COMBO_BOX_GET_PRIVATE (object);

	if (priv->action != NULL) {
		g_object_unref (priv->action);
		priv->action = NULL;
	}

	if (priv->action_group != NULL) {
		g_object_unref (priv->action_group);
		priv->action_group = NULL;
	}

	g_hash_table_remove_all (priv->index);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_action_combo_box_parent_class)->dispose (object);
}

static void
action_combo_box_finalize (GObject *object)
{
	EActionComboBoxPrivate *priv = E_ACTION_COMBO_BOX_GET_PRIVATE (object);

	g_hash_table_destroy (priv->index);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_action_combo_box_parent_class)->finalize (object);
}

static void
action_combo_box_constructed (GObject *object)
{
	GtkComboBox *combo_box;
	GtkCellRenderer *renderer;

	combo_box = GTK_COMBO_BOX (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_action_combo_box_parent_class)->constructed (object);

	/* This needs to happen after constructor properties are set
	 * so that GtkCellLayout.get_area() returns something valid. */

	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_cell_layout_pack_start (
		GTK_CELL_LAYOUT (combo_box), renderer, FALSE);
	gtk_cell_layout_set_cell_data_func (
		GTK_CELL_LAYOUT (combo_box), renderer,
		(GtkCellLayoutDataFunc) action_combo_box_render_pixbuf,
		combo_box, NULL);

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (
		GTK_CELL_LAYOUT (combo_box), renderer, TRUE);
	gtk_cell_layout_set_cell_data_func (
		GTK_CELL_LAYOUT (combo_box), renderer,
		(GtkCellLayoutDataFunc) action_combo_box_render_text,
		combo_box, NULL);

	gtk_combo_box_set_row_separator_func (
		combo_box, (GtkTreeViewRowSeparatorFunc)
		action_combo_box_is_row_separator, NULL, NULL);
}

static void
action_combo_box_changed (GtkComboBox *combo_box)
{
	GtkRadioAction *action;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gint value;

	/* This method is virtual, so no need to chain up. */

	if (!gtk_combo_box_get_active_iter (combo_box, &iter))
		return;

	model = gtk_combo_box_get_model (combo_box);
	gtk_tree_model_get (model, &iter, COLUMN_ACTION, &action, -1);
	g_object_get (action, "value", &value, NULL);
	gtk_radio_action_set_current_value (action, value);
	g_object_unref (action);
}

static void
e_action_combo_box_class_init (EActionComboBoxClass *class)
{
	GObjectClass *object_class;
	GtkComboBoxClass *combo_box_class;

	g_type_class_add_private (class, sizeof (EActionComboBoxPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = action_combo_box_set_property;
	object_class->get_property = action_combo_box_get_property;
	object_class->dispose = action_combo_box_dispose;
	object_class->finalize = action_combo_box_finalize;
	object_class->constructed = action_combo_box_constructed;

	combo_box_class = GTK_COMBO_BOX_CLASS (class);
	combo_box_class->changed = action_combo_box_changed;

	g_object_class_install_property (
		object_class,
		PROP_ACTION,
		g_param_spec_object (
			"action",
			"Action",
			"A GtkRadioAction",
			GTK_TYPE_RADIO_ACTION,
			G_PARAM_READWRITE));
}

static void
e_action_combo_box_init (EActionComboBox *combo_box)
{
	combo_box->priv = E_ACTION_COMBO_BOX_GET_PRIVATE (combo_box);

	combo_box->priv->index = g_hash_table_new_full (
		g_direct_hash, g_direct_equal,
		(GDestroyNotify) NULL,
		(GDestroyNotify) gtk_tree_row_reference_free);
}

GtkWidget *
e_action_combo_box_new (void)
{
	return e_action_combo_box_new_with_action (NULL);
}

GtkWidget *
e_action_combo_box_new_with_action (GtkRadioAction *action)
{
	return g_object_new (E_TYPE_ACTION_COMBO_BOX, "action", action, NULL);
}

GtkRadioAction *
e_action_combo_box_get_action (EActionComboBox *combo_box)
{
	g_return_val_if_fail (E_IS_ACTION_COMBO_BOX (combo_box), NULL);

	return combo_box->priv->action;
}

void
e_action_combo_box_set_action (EActionComboBox *combo_box,
                               GtkRadioAction *action)
{
	g_return_if_fail (E_IS_ACTION_COMBO_BOX (combo_box));

	if (action != NULL)
		g_return_if_fail (GTK_IS_RADIO_ACTION (action));

	if (combo_box->priv->action != NULL) {
		g_signal_handler_disconnect (
			combo_box->priv->action,
			combo_box->priv->changed_handler_id);
		g_object_unref (combo_box->priv->action);
	}

	if (combo_box->priv->action_group != NULL) {
		g_signal_handler_disconnect (
			combo_box->priv->action_group,
			combo_box->priv->group_sensitive_handler_id);
		g_signal_handler_disconnect (
			combo_box->priv->action_group,
			combo_box->priv->group_visible_handler_id);
		g_object_unref (combo_box->priv->action_group);
		combo_box->priv->action_group = NULL;
	}

	if (action != NULL) {
		/* This also adds a reference to the combo_box->priv->action_group */
		g_object_get (
			g_object_ref (action), "action-group",
			&combo_box->priv->action_group, NULL);
	}

	combo_box->priv->action = action;
	action_combo_box_update_model (combo_box);

	if (combo_box->priv->action != NULL)
		combo_box->priv->changed_handler_id = g_signal_connect (
			combo_box->priv->action, "changed",
			G_CALLBACK (action_combo_box_action_changed_cb),
			combo_box);

	if (combo_box->priv->action_group != NULL) {
		combo_box->priv->group_sensitive_handler_id =
			e_signal_connect_notify (
				combo_box->priv->action_group,
				"notify::sensitive", G_CALLBACK (
				action_combo_box_action_group_notify_cb),
				combo_box);
		combo_box->priv->group_visible_handler_id =
			e_signal_connect_notify (
				combo_box->priv->action_group,
				"notify::visible", G_CALLBACK (
				action_combo_box_action_group_notify_cb),
				combo_box);
	}

	g_object_notify (G_OBJECT (combo_box), "action");
}

gint
e_action_combo_box_get_current_value (EActionComboBox *combo_box)
{
	g_return_val_if_fail (E_IS_ACTION_COMBO_BOX (combo_box), 0);
	g_return_val_if_fail (combo_box->priv->action != NULL, 0);

	return gtk_radio_action_get_current_value (combo_box->priv->action);
}

void
e_action_combo_box_set_current_value (EActionComboBox *combo_box,
                                      gint current_value)
{
	g_return_if_fail (E_IS_ACTION_COMBO_BOX (combo_box));
	g_return_if_fail (combo_box->priv->action != NULL);

	gtk_radio_action_set_current_value (
		combo_box->priv->action, current_value);
}

void
e_action_combo_box_add_separator_before (EActionComboBox *combo_box,
                                         gint action_value)
{
	GtkTreeModel *model;
	GtkTreeIter iter;

	g_return_if_fail (E_IS_ACTION_COMBO_BOX (combo_box));

	/* NULL actions are rendered as separators. */
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo_box));
	gtk_list_store_append (GTK_LIST_STORE (model), &iter);
	gtk_list_store_set (
		GTK_LIST_STORE (model), &iter, COLUMN_ACTION,
		NULL, COLUMN_SORT, (gfloat) action_value - 0.5, -1);
}

void
e_action_combo_box_add_separator_after (EActionComboBox *combo_box,
                                        gint action_value)
{
	GtkTreeModel *model;
	GtkTreeIter iter;

	g_return_if_fail (E_IS_ACTION_COMBO_BOX (combo_box));

	/* NULL actions are rendered as separators. */
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo_box));
	gtk_list_store_append (GTK_LIST_STORE (model), &iter);
	gtk_list_store_set (
		GTK_LIST_STORE (model), &iter, COLUMN_ACTION,
		NULL, COLUMN_SORT, (gfloat) action_value + 0.5, -1);
}

void
e_action_combo_box_update_model (EActionComboBox *combo_box)
{
	g_return_if_fail (E_IS_ACTION_COMBO_BOX (combo_box));

	action_combo_box_update_model (combo_box);
}
