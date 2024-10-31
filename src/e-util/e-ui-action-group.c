/*
 * SPDX-FileCopyrightText: (C) 2024 Red Hat (www.redhat.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <gio/gio.h>
#include <libedataserver/libedataserver.h>

#include "e-ui-action-group.h"
#include "e-ui-action.h"

/**
 * SECTION: e-ui-action-group
 * @include: e-util/e-util.h
 * @short_description: Group of UI actions
 *
 * The #EUIActionGroup is a named group of #EUIAction actions. The group
 * allows to sensitize or influence visibility of all the actions in it
 * by setting sensitive or visible property of the group itself.
 *
 * The #EUIActionGroup can serve also as a #GActionMap.
 *
 * The object is not thread-safe, it's meant to be used only from
 * the main/GUI thread.
 *
 * Since: 3.56
 **/

struct _EUIActionGroup {
	GSimpleActionGroup parent;

	gchar *name;
	GHashTable *items; /* gchar *name ~> EUIAction * (owned) */
	gboolean sensitive;
	gboolean visible;
};

G_DEFINE_TYPE (EUIActionGroup, e_ui_action_group, G_TYPE_SIMPLE_ACTION_GROUP)

enum {
	PROP_0,
	PROP_NAME,
	PROP_SENSITIVE,
	PROP_VISIBLE,
	N_PROPS
};

enum {
	SIGNAL_ADDED,
	SIGNAL_REMOVED,
	SIGNAL_ACCEL_ADDED,
	SIGNAL_ACCEL_REMOVED,
	N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };
static GParamSpec *properties[N_PROPS] = { NULL, };

static void
e_ui_action_group_set_property (GObject *object,
				guint prop_id,
				const GValue *value,
				GParamSpec *pspec)
{
	EUIActionGroup *self = E_UI_ACTION_GROUP (object);

	switch (prop_id) {
	case PROP_NAME:
		g_free (self->name);
		self->name = g_value_dup_string (value);
		break;
	case PROP_SENSITIVE:
		e_ui_action_group_set_sensitive (self, g_value_get_boolean (value));
		break;
	case PROP_VISIBLE:
		e_ui_action_group_set_visible (self, g_value_get_boolean (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
e_ui_action_group_get_property (GObject *object,
				guint prop_id,
				GValue *value,
				GParamSpec *pspec)
{
	EUIActionGroup *self = E_UI_ACTION_GROUP (object);

	switch (prop_id) {
	case PROP_NAME:
		g_value_set_string (value, e_ui_action_group_get_name (self));
		break;
	case PROP_SENSITIVE:
		g_value_set_boolean (value, e_ui_action_group_get_sensitive (self));
		break;
	case PROP_VISIBLE:
		g_value_set_boolean (value, e_ui_action_group_get_visible (self));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
e_ui_action_group_dispose (GObject *object)
{
	EUIActionGroup *self = E_UI_ACTION_GROUP (object);

	e_ui_action_group_remove_all (self);

	G_OBJECT_CLASS (e_ui_action_group_parent_class)->dispose (object);
}

static void
e_ui_action_group_finalize (GObject *object)
{
	EUIActionGroup *self = E_UI_ACTION_GROUP (object);

	g_clear_pointer (&self->name, g_free);
	g_clear_pointer (&self->items, g_hash_table_unref);

	G_OBJECT_CLASS (e_ui_action_group_parent_class)->finalize (object);
}

static void
e_ui_action_group_class_init (EUIActionGroupClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->set_property = e_ui_action_group_set_property;
	object_class->get_property = e_ui_action_group_get_property;
	object_class->dispose = e_ui_action_group_dispose;
	object_class->finalize = e_ui_action_group_finalize;

	/**
	 * EUIActionGroup:name:
	 *
	 * Name of the action group.
	 *
	 * Since: 3.56
	 **/
	properties[PROP_NAME] = g_param_spec_string ("name", NULL, NULL, NULL,
		G_PARAM_READWRITE |
		G_PARAM_CONSTRUCT_ONLY |
		G_PARAM_STATIC_STRINGS |
		G_PARAM_EXPLICIT_NOTIFY);

	/**
	 * EUIActionGroup:sensitive:
	 *
	 * Get or set whether the action group is sensitive, which influences
	 * all the actions in the group.
	 *
	 * Since: 3.56
	 **/
	properties[PROP_SENSITIVE] = g_param_spec_boolean ("sensitive", NULL, NULL, TRUE,
		G_PARAM_READWRITE |
		G_PARAM_STATIC_STRINGS |
		G_PARAM_EXPLICIT_NOTIFY);


	/**
	 * EUIActionGroup:visible:
	 *
	 * Get or set whether the action group is visible, which influences
	 * all the actions in the group.
	 *
	 * Since: 3.56
	 **/
	properties[PROP_VISIBLE] = g_param_spec_boolean ("visible", NULL, NULL, TRUE,
		G_PARAM_READWRITE |
		G_PARAM_STATIC_STRINGS |
		G_PARAM_EXPLICIT_NOTIFY);

	g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);

	/* void		added		(EUIActionGroup *action_group,
					 EUIAction *action); */
	/**
	 * EUIActionGroup::added:
	 * @action_group: an #EUIActionGroup
	 * @action: an #EUIAction, which had been just added
	 *
	 * A signal emitted when the @action_group added @action into itself.
	 *
	 * Since: 3.56
	 **/
	signals[SIGNAL_ADDED] = g_signal_new ("added",
		E_TYPE_UI_ACTION_GROUP,
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		0,
		NULL, NULL,
		NULL,
		G_TYPE_NONE, 1,
		E_TYPE_UI_ACTION);

	/* void		removed		(EUIActionGroup *action_group,
					 EUIAction *action); */
	/**
	 * EUIActionGroup::removed:
	 * @action_group: an #EUIActionGroup
	 * @action: an #EUIAction, which had been just removed
	 *
	 * A signal emitted when the @action_group removed @action from itself.
	 *
	 * Since: 3.56
	 **/
	signals[SIGNAL_REMOVED] = g_signal_new ("removed",
		E_TYPE_UI_ACTION_GROUP,
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		0,
		NULL, NULL,
		NULL,
		G_TYPE_NONE, 1,
		E_TYPE_UI_ACTION);

	/* void		accel_added	(EUIActionGroup *action_group,
					 EUIAction *action,
					 const gchar *accel); */
	/**
	 * EUIActionGroup::accel-added:
	 * @action_group: an #EUIActionGroup
	 * @action: an #EUIAction, whose accel was added
	 * @accel: the added accel
	 *
	 * A signal emitted when one of the @action_group @action has added
	 * an accelerator string.
	 *
	 * Since: 3.56
	 **/
	signals[SIGNAL_ACCEL_ADDED] = g_signal_new ("accel-added",
		E_TYPE_UI_ACTION_GROUP,
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		0,
		NULL, NULL,
		NULL,
		G_TYPE_NONE, 2,
		E_TYPE_UI_ACTION,
		G_TYPE_STRING);

	/* void		accel_removed	(EUIActionGroup *action_group,
					 EUIAction *action,
					 const gchar *accel); */
	/**
	 * EUIActionGroup::accel-removed:
	 * @action_group: an #EUIActionGroup
	 * @action: an #EUIAction, whose accel was removed
	 * @accel: the removed accel
	 *
	 * A signal emitted when one of the @action_group @action has removed
	 * an accelerator string.
	 *
	 * Since: 3.56
	 **/
	signals[SIGNAL_ACCEL_REMOVED] = g_signal_new ("accel-removed",
		E_TYPE_UI_ACTION_GROUP,
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		0,
		NULL, NULL,
		NULL,
		G_TYPE_NONE, 2,
		E_TYPE_UI_ACTION,
		G_TYPE_STRING);
}

static void
e_ui_action_group_init (EUIActionGroup *self)
{
	self->items = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_object_unref);
	self->sensitive = TRUE;
	self->visible = TRUE;
}

/**
 * e_ui_action_group_new:
 * @name: (not nullable): a group name
 *
 * Creates a new #EUIActionGroup named @name.
 *
 * Returns: (transfer full): a new #EUIActionGroup
 *
 * Since: 3.56
 **/
EUIActionGroup *
e_ui_action_group_new (const gchar *name)
{
	g_return_val_if_fail (name != NULL, NULL);

	return g_object_new (E_TYPE_UI_ACTION_GROUP,
		"name", name,
		NULL);
}

/**
 * e_ui_action_group_get_name:
 * @self: an #EUIActionGroup
 *
 * Get a name of the @self.
 *
 * Returns: a name of the @self
 *
 * Since: 3.56
 **/
const gchar *
e_ui_action_group_get_name (EUIActionGroup *self)
{
	g_return_val_if_fail (E_IS_UI_ACTION_GROUP (self), NULL);

	return self->name;
}

static void
e_ui_action_group_action_accel_added_cb (EUIAction *action,
					 const gchar *accel,
					 gpointer user_data)
{
	EUIActionGroup *self = user_data;

	g_signal_emit (self, signals[SIGNAL_ACCEL_ADDED], 0, action, accel, NULL);
}

static void
e_ui_action_group_action_accel_removed_cb (EUIAction *action,
					   const gchar *accel,
					   gpointer user_data)
{
	EUIActionGroup *self = user_data;

	g_signal_emit (self, signals[SIGNAL_ACCEL_REMOVED], 0, action, accel, NULL);
}

/**
 * e_ui_action_group_add:
 * @self: an #EUIActionGroup
 * @action: (transfer none): an #EUIAction
 *
 * Adds an action into the @self. When the @action is already in this group,
 * it does nothing. When the @action is part of another group, it's removed
 * from it first. In other words, the @action can be part of a single group
 * only.
 *
 * Adding a new action of the same name as another action already in the group
 * is considered as a programming error.
 *
 * Since: 3.56
 **/
void
e_ui_action_group_add (EUIActionGroup *self,
		       EUIAction *action)
{
	EUIAction *adept;
	const gchar *name;

	g_return_if_fail (E_IS_UI_ACTION_GROUP (self));
	g_return_if_fail (E_IS_UI_ACTION (action));

	name = g_action_get_name (G_ACTION (action));
	adept = g_hash_table_lookup (self->items, name);

	if (adept == action)
		return;

	if (adept) {
		g_warning ("%s: Other action of the name '%s' is in the group, skipping", G_STRFUNC, name);
		return;
	}

	if (!e_ui_action_get_label (action))
		g_warning ("%s: Action '%s' does not have set label", G_STRFUNC, name);

	g_hash_table_insert (self->items, (gpointer) name, g_object_ref (action));
	g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (action));
	e_ui_action_set_action_group (action, self);

	g_signal_connect_object (action, "accel-added",
		G_CALLBACK (e_ui_action_group_action_accel_added_cb), self, 0);
	g_signal_connect_object (action, "accel-removed",
		G_CALLBACK (e_ui_action_group_action_accel_removed_cb), self, 0);

	g_signal_emit (self, signals[SIGNAL_ADDED], 0, action, NULL);
}

/**
 * e_ui_action_group_remove:
 * @self: an #EUIActionGroup
 * @action: an #EUIAction
 *
 * Removes @action from the @self. It does nothing when the action
 * does not exist in the group.
 *
 * Trying to remove an action of the same name as another action already
 * in the group is considered as a programming error.
 *
 * See e_ui_action_group_remove_by_name().
 *
 * Since: 3.56
 **/
void
e_ui_action_group_remove (EUIActionGroup *self,
			  EUIAction *action)
{
	EUIAction *adept;
	const gchar *name;

	g_return_if_fail (E_IS_UI_ACTION_GROUP (self));
	g_return_if_fail (E_IS_UI_ACTION (action));

	name = g_action_get_name (G_ACTION (action));
	adept = g_hash_table_lookup (self->items, name);

	if (adept != action) {
		if (adept)
			g_warning ("%s: Other action of the name '%s' is in the group, skipping", G_STRFUNC, name);
		return;
	}

	g_object_ref (action);

	g_hash_table_remove (self->items, (gpointer) name);
	e_ui_action_set_action_group (action, NULL);
	g_action_map_remove_action (G_ACTION_MAP (self), name);

	g_signal_handlers_disconnect_by_func (action, G_CALLBACK (e_ui_action_group_action_accel_added_cb), self);
	g_signal_handlers_disconnect_by_func (action, G_CALLBACK (e_ui_action_group_action_accel_removed_cb), self);

	g_signal_emit (self, signals[SIGNAL_REMOVED], 0, action, NULL);

	g_object_unref (action);
}

/**
 * e_ui_action_group_remove_by_name:
 * @self: an #EUIActionGroup
 * @action_name: (not nullable): an action name
 *
 * Removes an action from the @self identified by the @action_name.
 * It does nothing, when the group does not contain any such
 * named action.
 *
 * See e_ui_action_group_remove_by_name().
 *
 * Since: 3.56
 **/
void
e_ui_action_group_remove_by_name (EUIActionGroup *self,
				  const gchar *action_name)
{
	EUIAction *action;

	g_return_if_fail (E_IS_UI_ACTION_GROUP (self));
	g_return_if_fail (action_name != NULL);

	action = g_hash_table_lookup (self->items, action_name);
	if (!action)
		return;

	e_ui_action_group_remove (self, action);
}

/**
 * e_ui_action_group_remove_all:
 * @self: an #EUIActionGroup
 *
 * Removes all actions from the @self.
 *
 * Since: 3.56
 **/
void
e_ui_action_group_remove_all (EUIActionGroup *self)
{
	GPtrArray *actions;
	GActionMap *action_map;
	GHashTableIter iter;
	gpointer value = NULL;
	guint ii;

	g_return_if_fail (E_IS_UI_ACTION_GROUP (self));

	if (!g_hash_table_size (self->items))
		return;

	actions = g_ptr_array_new_full (g_hash_table_size (self->items), g_object_unref);

	g_hash_table_iter_init (&iter, self->items);
	while (g_hash_table_iter_next (&iter, NULL, &value)) {
		EUIAction *action = value;
		g_ptr_array_add (actions, g_object_ref (action));
	}

	g_hash_table_remove_all (self->items);

	action_map = G_ACTION_MAP (self);

	for (ii = 0; ii < actions->len; ii++) {
		EUIAction *action = g_ptr_array_index (actions, ii);
		e_ui_action_set_action_group (action, NULL);
		g_action_map_remove_action (action_map, g_action_get_name (G_ACTION (action)));
		g_signal_handlers_disconnect_by_func (action, G_CALLBACK (e_ui_action_group_action_accel_added_cb), self);
		g_signal_handlers_disconnect_by_func (action, G_CALLBACK (e_ui_action_group_action_accel_removed_cb), self);
		g_signal_emit (self, signals[SIGNAL_REMOVED], 0, action, NULL);
	}

	g_ptr_array_unref (actions);
}

/**
 * e_ui_action_group_get_action:
 * @self: an #EUIActionGroup
 * @action_name: (not nullable): an action name
 *
 * Looks up an action by its name in the @self.
 *
 * Returns: (transfer none) (nullable): an #EUIAction of the name @action_name,
 *    or %NULL, when no such named action exists in the group
 *
 * Since: 3.56
 **/
EUIAction *
e_ui_action_group_get_action (EUIActionGroup *self,
			      const gchar *action_name)
{
	g_return_val_if_fail (E_IS_UI_ACTION_GROUP (self), NULL);
	g_return_val_if_fail (action_name != NULL, NULL);

	return g_hash_table_lookup (self->items, action_name);
}

/**
 * e_ui_action_group_list_actions:
 * @self: an #EUIActionGroup
 *
 * List all the actions in the @self. Free the returned #GPtrArray
 * with g_ptr_array_unref(), when no longer needed.
 *
 * Returns: (transfer container) (element-type EUIAction): all actions in the @self
 *
 * Since: 3.56
 **/
GPtrArray *
e_ui_action_group_list_actions (EUIActionGroup *self)
{
	GPtrArray *actions;
	GHashTableIter iter;
	gpointer value = NULL;

	g_return_val_if_fail (E_IS_UI_ACTION_GROUP (self), NULL);

	actions = g_ptr_array_new_full (g_hash_table_size (self->items), g_object_unref);

	g_hash_table_iter_init (&iter, self->items);
	while (g_hash_table_iter_next (&iter, NULL, &value)) {
		EUIAction *action = value;
		g_ptr_array_add (actions, g_object_ref (action));
	}

	return actions;
}

/**
 * e_ui_action_group_get_visible:
 * @self: an #EUIActionGroup
 *
 * Checks whether the group, and also all the actions in the group, can be visible.
 *
 * Returns: whether the group and all its actions can be visible
 *
 * Since: 3.56
 **/
gboolean
e_ui_action_group_get_visible (EUIActionGroup *self)
{
	g_return_val_if_fail (E_IS_UI_ACTION_GROUP (self), FALSE);

	return self->visible;
}

/**
 * e_ui_action_group_set_visible:
 * @self: an #EUIActionGroup
 * @value: value to set
 *
 * Sets whether the group, and also all the actions in the group, can be visible.
 *
 * Since: 3.56
 **/
void
e_ui_action_group_set_visible (EUIActionGroup *self,
			       gboolean value)
{
	GHashTableIter iter;
	gpointer ht_value = NULL;

	g_return_if_fail (E_IS_UI_ACTION_GROUP (self));

	if ((!self->visible) == (!value))
		return;

	self->visible = value;

	g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_VISIBLE]);

	g_hash_table_iter_init (&iter, self->items);
	while (g_hash_table_iter_next (&iter, NULL, &ht_value)) {
		GObject *action = ht_value;
		g_object_notify (action, "is-visible");
	}
}

/**
 * e_ui_action_group_get_sensitive:
 * @self: an #EUIActionGroup
 *
 * Checks whether the group, and also all the actions in the group, can be sensitive.
 *
 * Returns: whether the group and all its actions can be sensitive
 *
 * Since: 3.56
 **/
gboolean
e_ui_action_group_get_sensitive (EUIActionGroup *self)
{
	g_return_val_if_fail (E_IS_UI_ACTION_GROUP (self), FALSE);

	return self->sensitive;
}

/**
 * e_ui_action_group_set_sensitive:
 * @self: an #EUIActionGroup
 * @value: a value to set
 *
 * Sets whether the group, and also all the actions in the group, can be visible.
 *
 * Since: 3.56
 **/
void
e_ui_action_group_set_sensitive (EUIActionGroup *self,
				 gboolean value)
{
	GHashTableIter iter;
	gpointer ht_value = NULL;

	g_return_if_fail (E_IS_UI_ACTION_GROUP (self));

	if ((!self->sensitive) == (!value))
		return;

	self->sensitive = value;

	g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SENSITIVE]);

	g_hash_table_iter_init (&iter, self->items);
	while (g_hash_table_iter_next (&iter, NULL, &ht_value)) {
		GObject *action = ht_value;
		g_object_notify (action, "enabled");
	}
}
