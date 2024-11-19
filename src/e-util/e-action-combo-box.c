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

#include <glib/gi18n.h>

#include "e-ui-action.h"
#include "e-ui-action-group.h"
#include "e-misc-utils.h"

#include "e-action-combo-box.h"

enum {
	COLUMN_ACTION,
	COLUMN_SORT
};

enum {
	PROP_0,
	PROP_ACTION,
	PROP_CURRENT_VALUE
};

struct _EActionComboBoxPrivate {
	EUIAction *action;
	EUIActionGroup *action_group;
	GHashTable *index;
	guint notify_state_handler_id;		/* action::notify::state */
	guint group_sensitive_handler_id;	/* action-group::sensitive */
	guint group_visible_handler_id;		/* action-group::visible */
	gboolean group_has_icons;
	gboolean ellipsize_enabled;
};

G_DEFINE_TYPE_WITH_PRIVATE (EActionComboBox, e_action_combo_box, GTK_TYPE_COMBO_BOX)

static void
action_combo_box_action_notify_state_cb (EUIAction *action,
					 GParamSpec *param,
					 EActionComboBox *combo_box)
{
	GtkTreeRowReference *reference = NULL;
	GtkTreeModel *model;
	GtkTreePath *path;
	GVariant *state;
	GtkTreeIter iter;
	gboolean valid;

	state = g_action_get_state (G_ACTION (action));
	if (state && g_variant_is_of_type (state, G_VARIANT_TYPE_INT32)) {
		reference = g_hash_table_lookup (combo_box->priv->index, GINT_TO_POINTER (g_variant_get_int32 (state)));
		g_return_if_fail (reference != NULL);
	} else {
		g_warn_if_reached ();
	}

	g_clear_pointer (&state, g_variant_unref);

	if (!reference)
		return;

	model = gtk_tree_row_reference_get_model (reference);
	path = gtk_tree_row_reference_get_path (reference);
	valid = gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_path_free (path);
	g_return_if_fail (valid);

	gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combo_box), &iter);
}

static void
action_combo_box_action_group_notify_cb (EUIActionGroup *action_group,
                                         GParamSpec *pspec,
                                         EActionComboBox *combo_box)
{
	g_object_set (combo_box,
		"sensitive", e_ui_action_group_get_sensitive (action_group),
		"visible", e_ui_action_group_get_visible (action_group),
		NULL);
}

static void
action_combo_box_render_pixbuf (GtkCellLayout *layout,
                                GtkCellRenderer *renderer,
                                GtkTreeModel *model,
                                GtkTreeIter *iter,
                                EActionComboBox *combo_box)
{
	EUIAction *action;
	gint width;

	gtk_tree_model_get (model, iter, COLUMN_ACTION, &action, -1);

	/* A NULL action means the row is a separator. */
	if (action == NULL)
		return;

	/* If some action has an icon */
	if (combo_box->priv->group_has_icons)
		/* Keep the pixbuf renderer a fixed size for proper alignment. */
		gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &width, NULL);
	else
		width = 0;

	g_object_set (
		G_OBJECT (renderer),
		"sensitive", e_ui_action_get_sensitive (action),
		"icon-name", e_ui_action_get_icon_name (action),
		"stock-size", GTK_ICON_SIZE_MENU,
		"visible", e_ui_action_is_visible (action),
		"width", width,
		NULL);

	g_object_unref (action);
}

static void
action_combo_box_render_text (GtkCellLayout *layout,
                              GtkCellRenderer *renderer,
                              GtkTreeModel *model,
                              GtkTreeIter *iter,
                              EActionComboBox *combo_box)
{
	EUIAction *action;
	const gchar *const_label;
	gchar *label = NULL;
	gint xpad;

	gtk_tree_model_get (model, iter, COLUMN_ACTION, &action, -1);

	/* A NULL action means the row is a separator. */
	if (action == NULL)
		return;

	const_label = e_ui_action_get_label (action);
	if (const_label && strchr (const_label, '_')) {
		guint ii, wr = 0;

		/* Strip out underscores. */
		label = g_strdup (const_label);
		for (ii = 0; label[ii]; ii++) {
			if (label[ii] != '_') {
				if (wr != ii)
					label[wr] = label[ii];
				wr++;
			}
		}
		if (ii != wr)
			label[wr] = '\0';
	}

	xpad = combo_box->priv->group_has_icons ? 3 : 0;

	g_object_set (
		G_OBJECT (renderer),
		"sensitive", e_ui_action_get_sensitive (action),
		"text", label ? label : const_label,
		"visible", e_ui_action_is_visible (action),
		"xpad", xpad,
		NULL);

	g_object_unref (action);
	g_free (label);
}

static gboolean
action_combo_box_is_row_separator (GtkTreeModel *model,
                                   GtkTreeIter *iter)
{
	EUIAction *action;
	gboolean separator;

	/* NULL actions are rendered as separators. */
	gtk_tree_model_get (model, iter, COLUMN_ACTION, &action, -1);
	separator = (action == NULL);
	g_clear_object (&action);

	return separator;
}

static void
action_combo_box_update_model (EActionComboBox *combo_box)
{
	GtkListStore *list_store;
	GPtrArray *radio_group;
	guint ii;

	g_hash_table_remove_all (combo_box->priv->index);

	if (combo_box->priv->action == NULL) {
		gtk_combo_box_set_model (GTK_COMBO_BOX (combo_box), NULL);
		return;
	}

	/* We store values in the sort column as floats so that we can
	 * insert separators in between consecutive integer values and
	 * still maintain the proper ordering. */
	list_store = gtk_list_store_new (
		2, E_TYPE_UI_ACTION, G_TYPE_FLOAT);

	radio_group = e_ui_action_get_radio_group (combo_box->priv->action);
	combo_box->priv->group_has_icons = FALSE;

	for (ii = 0; radio_group && ii < radio_group->len; ii++) {
		EUIAction *action = g_ptr_array_index (radio_group, ii);
		GtkTreeRowReference *reference;
		GtkTreePath *path;
		GtkTreeIter iter;
		GVariant *target;
		gint value = G_MININT32;

		if (!e_ui_action_get_visible (action))
			continue;

		combo_box->priv->group_has_icons |= e_ui_action_get_icon_name (action) != NULL;

		target = e_ui_action_ref_target (action);
		if (target && g_variant_is_of_type (target, G_VARIANT_TYPE_INT32))
			value = g_variant_get_int32 (target);
		g_clear_pointer (&target, g_variant_unref);

		gtk_list_store_append (list_store, &iter);
		gtk_list_store_set (list_store, &iter,
			COLUMN_ACTION, action,
			COLUMN_SORT, (gfloat) value,
			-1);

		path = gtk_tree_model_get_path (
			GTK_TREE_MODEL (list_store), &iter);
		reference = gtk_tree_row_reference_new (
			GTK_TREE_MODEL (list_store), path);
		g_hash_table_insert (
			combo_box->priv->index,
			GINT_TO_POINTER (value), reference);
		gtk_tree_path_free (path);
	}

	gtk_tree_sortable_set_sort_column_id (
		GTK_TREE_SORTABLE (list_store),
		COLUMN_SORT, GTK_SORT_ASCENDING);
	gtk_combo_box_set_model (
		GTK_COMBO_BOX (combo_box), GTK_TREE_MODEL (list_store));
	g_object_unref (list_store);

	action_combo_box_action_notify_state_cb (combo_box->priv->action, NULL, combo_box);
}

static void
e_action_combo_box_get_preferred_width (GtkWidget *widget,
					gint *minimum_width,
					gint *natural_width)
{
	GTK_WIDGET_CLASS (e_action_combo_box_parent_class)->get_preferred_width (widget, minimum_width, natural_width);

	if (e_action_combo_box_get_ellipsize_enabled (E_ACTION_COMBO_BOX (widget)) &&
	    natural_width && *natural_width > 250)
		*natural_width = 225;
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
		case PROP_CURRENT_VALUE:
			e_action_combo_box_set_current_value (
				E_ACTION_COMBO_BOX (object),
				g_value_get_int (value));
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
		case PROP_CURRENT_VALUE:
			g_value_set_int (
				value, e_action_combo_box_get_current_value (
				E_ACTION_COMBO_BOX (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
action_combo_box_dispose (GObject *object)
{
	EActionComboBox *self = E_ACTION_COMBO_BOX (object);

	e_action_combo_box_set_action (self, NULL);

	g_clear_object (&self->priv->action);
	g_clear_object (&self->priv->action_group);
	g_hash_table_remove_all (self->priv->index);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_action_combo_box_parent_class)->dispose (object);
}

static void
action_combo_box_finalize (GObject *object)
{
	EActionComboBox *self = E_ACTION_COMBO_BOX (object);

	g_hash_table_destroy (self->priv->index);

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
	EUIAction *action;
	GtkTreeModel *model;
	GtkTreeIter iter;

	/* This method is virtual, so no need to chain up. */

	if (!gtk_combo_box_get_active_iter (combo_box, &iter))
		return;

	model = gtk_combo_box_get_model (combo_box);
	gtk_tree_model_get (model, &iter, COLUMN_ACTION, &action, -1);
	if (action != NULL) {
		e_ui_action_set_active (action, TRUE);
		g_object_notify (G_OBJECT (combo_box), "current-value");
	}
	g_object_unref (action);
}

static void
e_action_combo_box_class_init (EActionComboBoxClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;
	GtkComboBoxClass *combo_box_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = action_combo_box_set_property;
	object_class->get_property = action_combo_box_get_property;
	object_class->dispose = action_combo_box_dispose;
	object_class->finalize = action_combo_box_finalize;
	object_class->constructed = action_combo_box_constructed;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->get_preferred_width = e_action_combo_box_get_preferred_width;

	combo_box_class = GTK_COMBO_BOX_CLASS (class);
	combo_box_class->changed = action_combo_box_changed;

	g_object_class_install_property (
		object_class,
		PROP_ACTION,
		g_param_spec_object (
			"action",
			"Action",
			"An EUIAction",
			E_TYPE_UI_ACTION,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_CURRENT_VALUE,
		g_param_spec_int (
			"current-value", NULL, NULL,
			G_MININT32, G_MAXINT32, 0,
			G_PARAM_READWRITE));
}

static void
e_action_combo_box_init (EActionComboBox *combo_box)
{
	combo_box->priv = e_action_combo_box_get_instance_private (combo_box);

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
e_action_combo_box_new_with_action (EUIAction *action)
{
	return g_object_new (E_TYPE_ACTION_COMBO_BOX, "action", action, NULL);
}

EUIAction *
e_action_combo_box_get_action (EActionComboBox *combo_box)
{
	g_return_val_if_fail (E_IS_ACTION_COMBO_BOX (combo_box), NULL);

	return combo_box->priv->action;
}

void
e_action_combo_box_set_action (EActionComboBox *combo_box,
			       EUIAction *action)
{
	gboolean last_state_set = FALSE;
	gint last_state;

	g_return_if_fail (E_IS_ACTION_COMBO_BOX (combo_box));

	if (action != NULL)
		g_return_if_fail (E_IS_UI_ACTION (action));

	if (action == combo_box->priv->action)
		return;

	if (combo_box->priv->action) {
		last_state_set = TRUE;
		last_state = e_action_combo_box_get_current_value (combo_box);
	}

	if (combo_box->priv->action_group) {
		g_signal_handler_disconnect (
			combo_box->priv->action_group,
			combo_box->priv->group_sensitive_handler_id);
		g_signal_handler_disconnect (
			combo_box->priv->action_group,
			combo_box->priv->group_visible_handler_id);
		g_clear_object (&combo_box->priv->action_group);
		combo_box->priv->group_visible_handler_id = 0;
		combo_box->priv->group_sensitive_handler_id = 0;
	}

	if (action != NULL) {
		g_object_ref (action);
		combo_box->priv->action_group = e_ui_action_get_action_group (action);

		if (combo_box->priv->action_group)
			g_object_ref (combo_box->priv->action_group);
	}

	if (combo_box->priv->action != NULL) {
		g_signal_handler_disconnect (
			combo_box->priv->action,
			combo_box->priv->notify_state_handler_id);
		g_clear_object (&combo_box->priv->action);
		combo_box->priv->notify_state_handler_id = 0;
	}

	combo_box->priv->action = action;
	action_combo_box_update_model (combo_box);

	if (combo_box->priv->action != NULL) {
		combo_box->priv->notify_state_handler_id = g_signal_connect (
			combo_box->priv->action, "notify::state",
			G_CALLBACK (action_combo_box_action_notify_state_cb), combo_box);
	}

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

	if (action && last_state_set && g_hash_table_contains (combo_box->priv->index, GINT_TO_POINTER (last_state)))
		e_action_combo_box_set_current_value (combo_box, last_state);

	g_object_notify (G_OBJECT (combo_box), "action");
}

gint
e_action_combo_box_get_current_value (EActionComboBox *combo_box)
{
	GVariant *state;
	gint value = 0;

	g_return_val_if_fail (E_IS_ACTION_COMBO_BOX (combo_box), 0);
	g_return_val_if_fail (combo_box->priv->action != NULL, 0);

	state = g_action_get_state (G_ACTION (combo_box->priv->action));
	if (state && g_variant_is_of_type (state, G_VARIANT_TYPE_INT32)) {
		value = g_variant_get_int32 (state);
	} else if (state) {
		g_warning ("%s: Action '%s' does not hold int32 state", G_STRFUNC, g_action_get_name (G_ACTION (combo_box->priv->action)));
	} else {
		g_warning ("%s: Action '%s' does not have state", G_STRFUNC, g_action_get_name (G_ACTION (combo_box->priv->action)));
	}

	g_clear_pointer (&state, g_variant_unref);

	return value;
}

void
e_action_combo_box_set_current_value (EActionComboBox *combo_box,
                                      gint current_value)
{
	g_return_if_fail (E_IS_ACTION_COMBO_BOX (combo_box));
	g_return_if_fail (combo_box->priv->action != NULL);

	if (current_value == e_action_combo_box_get_current_value (combo_box))
		return;

	e_ui_action_set_state (combo_box->priv->action, g_variant_new_int32 (current_value));

	g_object_notify (G_OBJECT (combo_box), "current-value");
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

gboolean
e_action_combo_box_get_ellipsize_enabled (EActionComboBox *combo_box)
{
	g_return_val_if_fail (E_IS_ACTION_COMBO_BOX (combo_box), FALSE);

	return combo_box->priv->ellipsize_enabled;
}

void
e_action_combo_box_set_ellipsize_enabled (EActionComboBox *combo_box,
					  gboolean enabled)
{
	g_return_if_fail (E_IS_ACTION_COMBO_BOX (combo_box));

	if ((enabled ? 1 : 0) != (combo_box->priv->ellipsize_enabled ? 1 : 0)) {
		GList *cells, *link;

		combo_box->priv->ellipsize_enabled = enabled;

		cells = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (combo_box));

		for (link = cells; link; link = g_list_next (link)) {
			if (GTK_IS_CELL_RENDERER_TEXT (link->data)) {
				g_object_set (link->data,
					"ellipsize", enabled ? PANGO_ELLIPSIZE_END : PANGO_ELLIPSIZE_NONE,
					NULL);
			}
		}

		g_list_free (cells);
	}
}
