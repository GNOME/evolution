/*
 * SPDX-FileCopyrightText: (C) 2024 Red Hat (www.redhat.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <gio/gio.h>

#include "e-ui-action.h"
#include "e-ui-manager.h"

#include "e-ui-menu.h"

/**
 * SECTION: e-ui-menu
 * @include: e-util/e-util.h
 * @short_description: a UI menu
 *
 * #EUIMenu is a #GMenuModel descendant, which takes care of an #EUIAction
 * visibility and regenerates its content when any of the actions hides/shows
 * itself or when the associated #EUIManager changes.
 *
 * The object is not thread-safe, it's meant to be used only from
 * the main/GUI thread.
 *
 * Since: 3.56
 **/

struct _EUIMenu {
	GMenuModel parent;

	GHashTable *tracked_actions; /* EUIAction * ~> NULL */
	GMenu *real_menu;
	EUIManager *manager;
	gchar *id;

	guint frozen;
	gboolean changed_while_frozen;
};

G_DEFINE_TYPE (EUIMenu, e_ui_menu, G_TYPE_MENU_MODEL)

enum {
	PROP_0,
	PROP_MANAGER,
	PROP_ID,
	N_PROPS
};

static GParamSpec *properties[N_PROPS] = { NULL, };

static void
e_ui_menu_thaw_internal (EUIMenu *self)
{
	g_return_if_fail (E_IS_UI_MENU (self));
	g_return_if_fail (self->frozen > 0);

	self->frozen--;

	if (!self->frozen && self->changed_while_frozen) {
		self->changed_while_frozen = FALSE;

		e_ui_menu_rebuild (self);
	}
}

static void
e_ui_menu_manager_freeze_cb (EUIManager *manager,
			     gpointer user_data)
{
	EUIMenu *self = user_data;

	e_ui_menu_freeze (self);
}

static void
e_ui_menu_manager_thaw_cb (EUIManager *manager,
			   gboolean changed_while_frozen,
			   gpointer user_data)
{
	EUIMenu *self = user_data;

	self->changed_while_frozen = self->changed_while_frozen || changed_while_frozen;

	e_ui_menu_thaw_internal (self);
}

static void
e_ui_menu_items_changed_cb (GMenuModel *parent,
			    gint position,
			    gint removed,
			    gint added,
			    gpointer user_data)
{
	EUIMenu *self = user_data;

	g_menu_model_items_changed (G_MENU_MODEL (self), position, removed, added);
}

static gboolean
e_ui_menu_is_mutable (GMenuModel *self)
{
	return TRUE;
}

static gint
e_ui_menu_get_n_items (GMenuModel *model)
{
	EUIMenu *self = E_UI_MENU (model);

	return g_menu_model_get_n_items (G_MENU_MODEL (self->real_menu));
}

static void
e_ui_menu_get_item_attributes (GMenuModel *model,
			       gint position,
			       GHashTable **table)
{
	EUIMenu *self = E_UI_MENU (model);
	GMenuModelClass *klass;

	klass = G_MENU_MODEL_GET_CLASS (self->real_menu);
	g_return_if_fail (klass != NULL);
	g_return_if_fail (klass->get_item_attributes != NULL);

	klass->get_item_attributes (G_MENU_MODEL (self->real_menu), position, table);
}

static void
e_ui_menu_get_item_links (GMenuModel *model,
			  gint position,
                          GHashTable **table)
{
	EUIMenu *self = E_UI_MENU (model);
	GMenuModelClass *klass;

	klass = G_MENU_MODEL_GET_CLASS (self->real_menu);
	g_return_if_fail (klass != NULL);
	g_return_if_fail (klass->get_item_links != NULL);

	klass->get_item_links (G_MENU_MODEL (self->real_menu), position, table);
}

static void
e_ui_menu_set_property (GObject *object,
			guint prop_id,
			const GValue *value,
			GParamSpec *pspec)
{
	EUIMenu *self = E_UI_MENU (object);

	switch (prop_id) {
	case PROP_MANAGER:
		g_clear_object (&self->manager);
		self->manager = g_value_dup_object (value);
		break;
	case PROP_ID:
		g_free (self->id);
		self->id = g_value_dup_string (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
e_ui_menu_get_property (GObject *object,
			guint prop_id,
			GValue *value,
			GParamSpec *pspec)
{
	EUIMenu *self = E_UI_MENU (object);

	switch (prop_id) {
	case PROP_MANAGER:
		g_value_set_object (value, e_ui_menu_get_manager (self));
		break;
	case PROP_ID:
		g_value_set_string (value, e_ui_menu_get_id (self));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
e_ui_menu_constructed (GObject *object)
{
	EUIMenu *self = E_UI_MENU (object);

	G_OBJECT_CLASS (e_ui_menu_parent_class)->constructed (object);

	g_return_if_fail (self->manager != NULL);
	g_return_if_fail (self->id != NULL);

	e_ui_menu_rebuild (self);

	g_signal_connect_object (self->manager, "changed",
		G_CALLBACK (e_ui_menu_rebuild), self, G_CONNECT_SWAPPED);
	g_signal_connect_object (self->manager, "freeze",
		G_CALLBACK (e_ui_menu_manager_freeze_cb), self, 0);
	g_signal_connect_object (self->manager, "thaw",
		G_CALLBACK (e_ui_menu_manager_thaw_cb), self, 0);
}

static void
e_ui_menu_finalize (GObject *object)
{
	EUIMenu *self = E_UI_MENU (object);

	e_ui_menu_remove_all (self);

	g_clear_pointer (&self->tracked_actions, g_hash_table_unref);
	g_clear_pointer (&self->id, g_free);
	g_clear_object (&self->real_menu);
	g_clear_object (&self->manager);

	G_OBJECT_CLASS (e_ui_menu_parent_class)->finalize (object);
}

static void
e_ui_menu_class_init (EUIMenuClass *klass)
{
	GObjectClass *object_class;
	GMenuModelClass *model_class;

	model_class = G_MENU_MODEL_CLASS (klass);
	model_class->is_mutable = e_ui_menu_is_mutable;
	model_class->get_n_items = e_ui_menu_get_n_items;
	model_class->get_item_attributes = e_ui_menu_get_item_attributes;
	model_class->get_item_links = e_ui_menu_get_item_links;

	object_class = G_OBJECT_CLASS (klass);
	object_class->set_property = e_ui_menu_set_property;
	object_class->get_property = e_ui_menu_get_property;
	object_class->constructed = e_ui_menu_constructed;
	object_class->finalize = e_ui_menu_finalize;

	/**
	 * EUIMenu:manager:
	 *
	 * An #EUIManager associated with the menu.
	 *
	 * Since: 3.56
	 **/
	properties[PROP_MANAGER] = g_param_spec_object ("manager", NULL, NULL,
		E_TYPE_UI_MANAGER,
		G_PARAM_READWRITE |
		G_PARAM_CONSTRUCT_ONLY |
		G_PARAM_STATIC_STRINGS |
		G_PARAM_EXPLICIT_NOTIFY);

	/**
	 * EUIMenu:id:
	 *
	 * Identifier of the menu to be read from the #EUIManager.
	 *
	 * Since: 3.56
	 **/
	properties[PROP_ID] = g_param_spec_string ("id", NULL, NULL, NULL,
		G_PARAM_READWRITE |
		G_PARAM_CONSTRUCT_ONLY |
		G_PARAM_STATIC_STRINGS |
		G_PARAM_EXPLICIT_NOTIFY);

	g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);
}

static void
e_ui_menu_init (EUIMenu *self)
{
	self->tracked_actions = g_hash_table_new_full (g_direct_hash, g_direct_equal, g_object_unref, NULL);
	self->real_menu = g_menu_new ();

	g_signal_connect_object (self->real_menu, "items-changed",
		G_CALLBACK (e_ui_menu_items_changed_cb), self, 0);
}

/**
 * e_ui_menu_new:
 * @manager: an *EUIManager
 * @id: a menu identifier to use the created #EUIMenu with
 *
 * Creates a new #EUIMenu, which will use the @manager to get
 * its content from under identifier @id.
 *
 * Returns: (transfer full): a new #EUIMenu
 *
 * Since: 3.56
 **/
EUIMenu *
e_ui_menu_new (EUIManager *manager,
	       const gchar *id)
{
	return g_object_new (E_TYPE_UI_MENU,
		"manager", manager,
		"id", id,
		NULL);
}

/**
 * e_ui_menu_get_manager:
 * @self: an #EUIMenu
 *
 * Gets an associated #EUIManager.
 *
 * Returns: (transfer none): an associated #EUIManager
 *
 * Since: 3.56
 **/
EUIManager *
e_ui_menu_get_manager (EUIMenu *self)
{
	g_return_val_if_fail (E_IS_UI_MENU (self), NULL);

	return self->manager;
}

/**
 * e_ui_menu_get_id:
 * @self: an #EUIMenu
 *
 * Gets an identifier of the menu to populate the @self with.
 *
 * Returns: a menu identifier
 *
 * Since: 3.56
 **/
const gchar *
e_ui_menu_get_id (EUIMenu *self)
{
	g_return_val_if_fail (E_IS_UI_MENU (self), NULL);

	return self->id;
}

/**
 * e_ui_menu_append_item:
 * @self: an #EUIMenu
 * @action: (nullable): an #EUIAction, or %NULL
 * @item: a #GMenuItem to append
 *
 * Appends a #GMenuItem descendant @item into the menu, which can
 * be related to the @action. When the @action is not %NULL, its
 * state is tracked and the menu is regenerated whenever the state
 * changed (and when needed).
 *
 * Since: 3.56
 **/
void
e_ui_menu_append_item (EUIMenu *self,
		       EUIAction *action,
		       GMenuItem *item)
{
	g_return_if_fail (E_IS_UI_MENU (self));
	g_return_if_fail (G_IS_MENU_ITEM (item));

	if (action)
		e_ui_menu_track_action (self, action);

	g_menu_append_item (self->real_menu, item);
}

/**
 * e_ui_menu_append_section:
 * @self: an #EUIMenu
 * @section: a #GMenuModel
 *
 * Appends a #GMenuModel as a new section in the @self.
 *
 * Since: 3.56
 **/
void
e_ui_menu_append_section (EUIMenu *self,
			  GMenuModel *section)
{
	g_return_if_fail (E_IS_UI_MENU (self));
	g_return_if_fail (G_IS_MENU_MODEL (section));

	g_menu_append_section (self->real_menu, NULL, section);
}

/**
 * e_ui_menu_track_action:
 * @self: an #EUIMenu
 * @action: (not nullable): an #EUIAction
 *
 * Tracks a state change of the @action and regenerates the menu
 * content when needed.
 *
 * Since: 3.56
 **/
void
e_ui_menu_track_action (EUIMenu *self,
			EUIAction *action)
{
	g_return_if_fail (E_IS_UI_MENU (self));
	g_return_if_fail (E_IS_UI_ACTION (action));

	if (!g_hash_table_contains (self->tracked_actions, action)) {
		g_signal_connect_swapped (action, "notify::is-visible", G_CALLBACK (e_ui_menu_rebuild), self);
		g_signal_connect_swapped (action, "changed", G_CALLBACK (e_ui_menu_rebuild), self);
		g_hash_table_add (self->tracked_actions, g_object_ref (action));
	}
}

/**
 * e_ui_menu_freeze:
 * @self: an #EUIMenu
 *
 * Freezes rebuild of the menu content. Useful when filling the content.
 * The function can be called multiple times, only each call needs
 * a pair call of the e_ui_menu_thaw() to revert the effect of this function.
 *
 * Since: 3.56
 **/
void
e_ui_menu_freeze (EUIMenu *self)
{
	g_return_if_fail (E_IS_UI_MENU (self));
	g_return_if_fail (self->frozen + 1 > self->frozen);

	self->frozen++;
}

/**
 * e_ui_menu_thaw:
 * @self: an #EUIMenu
 *
 * Pair function for the e_ui_menu_freeze(). It's a programming
 * error to thaw a menu, which is not frozen.
 *
 * Since: 3.56
 **/
void
e_ui_menu_thaw (EUIMenu *self)
{
	g_return_if_fail (E_IS_UI_MENU (self));
	g_return_if_fail (self->frozen > 0);

	e_ui_menu_thaw_internal (self);
}

/**
 * e_ui_menu_is_frozen:
 * @self: an #EUIMenu
 *
 * Gets whether the @self is frozen for rebuild. It can be frozen with
 * the e_ui_menu_freeze() and unfrozen with the e_ui_menu_thaw().
 *
 * Returns: whether rebuild of the @self is frozen
 *
 * Since: 3.56
 **/
gboolean
e_ui_menu_is_frozen (EUIMenu *self)
{
	g_return_val_if_fail (E_IS_UI_MENU (self), FALSE);

	return self->frozen > 0;
}

/**
 * e_ui_menu_rebuild:
 * @self: an #EUIMenu
 *
 * Rebuilds the @self content. If the rebuild is frozen (see e_ui_menu_freeze()),
 * the rebuild is postponed until the rebuild is allowed again.
 *
 * Since: 3.56
 **/
void
e_ui_menu_rebuild (EUIMenu *self)
{
	g_return_if_fail (E_IS_UI_MENU (self));

	if (self->frozen) {
		self->changed_while_frozen = TRUE;
		return;
	}

	e_ui_menu_remove_all (self);
	e_ui_manager_fill_menu (self->manager, self->id, self);
}

/**
 * e_ui_menu_remove_all:
 * @self: an #EUIMenu
 *
 * Cleans up content of the @self, including tracked actions
 * and all the menu items.
 *
 * Since: 3.56
 **/
void
e_ui_menu_remove_all (EUIMenu *self)
{
	GHashTableIter iter;
	gpointer key;

	g_return_if_fail (E_IS_UI_MENU (self));

	g_menu_remove_all (self->real_menu);

	g_hash_table_iter_init (&iter, self->tracked_actions);
	while (g_hash_table_iter_next (&iter, &key, NULL)) {
		EUIAction *action = key;

		g_signal_handlers_disconnect_by_func (action, G_CALLBACK (e_ui_menu_rebuild), self);
	}

	g_hash_table_remove_all (self->tracked_actions);
}
