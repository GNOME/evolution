/*
 * SPDX-FileCopyrightText: (C) 2024 Red Hat (www.redhat.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <gtk/gtk.h>

#include <libedataserver/libedataserver.h>

#include "e-headerbar.h"
#include "e-headerbar-button.h"
#include "e-misc-utils.h"
#include "e-ui-action.h"
#include "e-ui-customizer.h"
#include "e-ui-menu.h"
#include "e-ui-parser.h"
#include "e-util-enumtypes.h"

#include "e-ui-manager.h"

/**
 * SECTION: e-ui-manager
 * @include: e-util/e-util.h
 * @short_description: a UI manager
 *
 * The #EUIManager is a central point of managing headerbars, toolbars, menubars
 * and context menus in the application.
 * The #EUIParser is used to read the .eui definition from an XML file or data.
 * Each item in the definition corresponds to an #EUIAction, which should be added
 * with the e_ui_manager_add_actions() or e_ui_manager_add_actions_enum() before
 * the corresponding items are created with the e_ui_manager_create_item().
 *
 * The e_ui_manager_set_action_groups_widget() is used to assign a widget to
 * which the action groups should be inserted (for example a #GtkWindow, but
 * can be other as well).
 *
 * The #GtkAccelGroup returned from the e_ui_manager_get_accel_group() needs
 * to be added to the #GtkWindow with gtk_window_add_accel_group().
 *
 * The object is not thread-safe, it's meant to be used only from
 * the main/GUI thread.
 *
 * Since: 3.56
 **/

struct _EUIManager {
	GObject parent;

	EUIParser *parser;
	EUICustomizer *customizer;
	GtkAccelGroup *accel_group;
	GHashTable *action_groups; /* gchar * ~> EUIActionGroup * */
	GHashTable *gicons; /* gchar * ~> GIcon * */
	GHashTable *groups; /* gchar * ~> GPtrArray * { EUIAction }; used for radio groups */

	CamelWeakRefGroup *self_weak_ref_group; /* self, aka EUIManager */
	GHashTable *shortcut_actions; /* EUIManagerShortcutDef ~> GPtrArray { EUIAction } */

	GWeakRef action_groups_widget;

	guint frozen;
	gboolean changed_while_frozen;

	guint in_accel_activation;
};

enum {
	PROP_0,
	PROP_CUSTOMIZER_FILENAME,
	N_PROPS
};

enum {
	SIGNAL_CHANGED,
	SIGNAL_FREEZE,
	SIGNAL_THAW,
	SIGNAL_CREATE_ITEM,
	SIGNAL_CREATE_GICON,
	SIGNAL_IGNORE_ACCEL,
	N_SIGNALS
};

static GParamSpec *properties[N_PROPS] = { NULL, };
static guint signals[N_SIGNALS] = { 0, };

G_DEFINE_TYPE (EUIManager, e_ui_manager, G_TYPE_OBJECT)

static void
e_ui_manager_gather_groups_recr (EUIManager *self,
				 EUIElement *elem)
{
	if (!elem)
		return;

	if (e_ui_element_get_kind (elem) == E_UI_ELEMENT_KIND_ITEM) {
		const gchar *group = e_ui_element_item_get_group (elem);

		if (group && *group && e_ui_element_item_get_action (elem)) {
			EUIAction *action;

			action = e_ui_manager_get_action (self, e_ui_element_item_get_action (elem));
			if (action) {
				GPtrArray *group_array;

				group_array = g_hash_table_lookup (self->groups, group);

				if (!group_array) {
					group_array = g_ptr_array_new ();
					g_hash_table_insert (self->groups, g_strdup (group), group_array);
				}

				e_ui_action_set_radio_group (action, group_array);
			} else {
				g_warning ("%s: Action '%s' for group '%s' not found", G_STRFUNC, e_ui_element_item_get_action (elem), group);
			}
		}
	} else if (e_ui_element_get_n_children (elem) > 0) {
		guint ii, sz = e_ui_element_get_n_children (elem);
		for (ii = 0; ii < sz; ii++) {
			EUIElement *child = e_ui_element_get_child (elem, ii);
			e_ui_manager_gather_groups_recr (self, child);
		}
	}
}

static void
e_ui_manager_gather_groups (EUIManager *self)
{
	GHashTableIter iter;
	gpointer value;

	g_hash_table_iter_init (&iter, self->groups);

	while (g_hash_table_iter_next (&iter, NULL, &value)) {
		GPtrArray *group_array = value;
		guint ii;

		/* go from the back, the unset modifies content of the group_array */
		for (ii = group_array->len; ii-- > 0;) {
			EUIAction *action = g_ptr_array_index (group_array, ii);
			e_ui_action_set_radio_group (action, NULL);
		}
	}

	g_hash_table_remove_all (self->groups);

	/* the groups are gathered from the app data, not from the user customizations,
	   because the user custommizations can have missing this information */
	e_ui_manager_gather_groups_recr (self, e_ui_parser_get_root (self->parser));

	/* update the state, thus there's only one (or none) group item selected */
	g_hash_table_iter_init (&iter, self->groups);

	while (g_hash_table_iter_next (&iter, NULL, &value)) {
		GPtrArray *group_array = value;

		if (group_array->len) {
			EUIAction *action = g_ptr_array_index (group_array, 0);
			GVariant *state;

			state = g_action_get_state (G_ACTION (action));
			if (state) {
				/* while setting the same value, the function will update other group members with it */
				e_ui_action_set_state (action, state);
				g_variant_unref (state);
			}
		}
	}
}

static void
e_ui_manager_parser_changed_cb (EUIParser *parser,
				gpointer user_data)
{
	EUIManager *self = user_data;

	e_ui_manager_changed (self);
}

static void
e_ui_manager_connect_accel_cb (EUIManager *self,
			       EUIAction *action,
			       const gchar *accel,
			       gpointer user_data);

static void
e_ui_manager_disconnect_accel_cb (EUIManager *self,
				  EUIAction *action,
				  const gchar *accel,
				  gpointer user_data);

static void
e_ui_manager_customizer_accels_changed_cb (EUICustomizer *customizer,
					   const gchar *action_name,
					   GPtrArray *old_accels,
					   GPtrArray *new_accels,
					   gpointer user_data)
{
	EUIManager *self = user_data;
	EUIAction *action;
	const gchar *accel;
	guint ii;

	action = e_ui_manager_get_action (self, action_name);
	if (!action)
		return;

	if (old_accels) {
		for (ii = 0; ii < old_accels->len; ii++) {
			accel = g_ptr_array_index (old_accels, ii);
			if (accel && *accel)
				e_ui_manager_disconnect_accel_cb (self, action, accel, NULL);
		}
	} else {
		GPtrArray *secondary_accels;

		accel = e_ui_action_get_accel (action);
		if (accel && *accel)
			e_ui_manager_disconnect_accel_cb (self, action, accel, NULL);

		secondary_accels = e_ui_action_get_secondary_accels (action);
		for (ii = 0; secondary_accels && ii < secondary_accels->len; ii++) {
			accel = g_ptr_array_index (secondary_accels, ii);
			if (accel && *accel)
				e_ui_manager_disconnect_accel_cb (self, action, accel, NULL);
		}
	}

	if (new_accels) {
		for (ii = 0; ii < new_accels->len; ii++) {
			accel = g_ptr_array_index (new_accels, ii);
			if (accel && *accel)
				e_ui_manager_connect_accel_cb (self, action, accel, NULL);
		}
	} else {
		GPtrArray *secondary_accels;

		accel = e_ui_action_get_accel (action);
		if (accel && *accel)
			e_ui_manager_connect_accel_cb (self, action, accel, NULL);

		secondary_accels = e_ui_action_get_secondary_accels (action);
		for (ii = 0; secondary_accels && ii < secondary_accels->len; ii++) {
			accel = g_ptr_array_index (secondary_accels, ii);
			if (accel && *accel)
				e_ui_manager_connect_accel_cb (self, action, accel, NULL);
		}
	}
}

static void
e_ui_manager_set_property (GObject *object,
			   guint prop_id,
			   const GValue *value,
			   GParamSpec *pspec)
{
	EUIManager *self = E_UI_MANAGER (object);

	switch (prop_id) {
	case PROP_CUSTOMIZER_FILENAME:
		g_clear_object (&self->customizer);
		self->customizer = g_object_new (E_TYPE_UI_CUSTOMIZER,
			"filename", g_value_get_string (value),
			"manager", self,
			NULL);
		g_signal_connect_object (self->customizer, "accels-changed",
			G_CALLBACK (e_ui_manager_customizer_accels_changed_cb), self, 0);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
e_ui_manager_get_property (GObject *object,
			   guint prop_id,
			   GValue *value,
			   GParamSpec *pspec)
{
	EUIManager *self = E_UI_MANAGER (object);

	switch (prop_id) {
	case PROP_CUSTOMIZER_FILENAME:
		g_value_set_string (value, self->customizer ? e_ui_customizer_get_filename (self->customizer) : NULL);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
e_ui_manager_dispose (GObject *object)
{
	EUIManager *self = E_UI_MANAGER (object);

	e_ui_manager_set_action_groups_widget (self, NULL);

	G_OBJECT_CLASS (e_ui_manager_parent_class)->dispose (object);
}

static void
e_ui_manager_finalize (GObject *object)
{
	EUIManager *self = E_UI_MANAGER (object);

	g_clear_object (&self->parser);
	g_clear_object (&self->customizer);
	g_clear_object (&self->accel_group);
	g_clear_pointer (&self->action_groups, g_hash_table_unref);
	g_clear_pointer (&self->gicons, g_hash_table_unref);
	g_clear_pointer (&self->groups, g_hash_table_unref);
	g_clear_pointer (&self->self_weak_ref_group, camel_weak_ref_group_unref);
	g_clear_pointer (&self->shortcut_actions, g_hash_table_unref);
	g_weak_ref_clear (&self->action_groups_widget);

	G_OBJECT_CLASS (e_ui_manager_parent_class)->finalize (object);
}

static void
e_ui_manager_class_init (EUIManagerClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->set_property = e_ui_manager_set_property;
	object_class->get_property = e_ui_manager_get_property;
	object_class->dispose = e_ui_manager_dispose;
	object_class->finalize = e_ui_manager_finalize;

	/**
	 * EUIManager:customizer-filename:
	 *
	 * A file name, into which the customizations are saved and loaded from.
	 * It can be %NULL, when the customizations are disabled.
	 *
	 * Since: 3.56
	 **/
	properties[PROP_CUSTOMIZER_FILENAME] = g_param_spec_string ("customizer-filename", NULL, NULL, NULL,
		G_PARAM_READWRITE |
		G_PARAM_CONSTRUCT_ONLY |
		G_PARAM_STATIC_STRINGS |
		G_PARAM_EXPLICIT_NOTIFY);

	g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);

	/* void		changed		(EUIManager *manager); */
	/**
	 * EUIManager::changed:
	 * @manager: an #EUIManager
	 *
	 * A signal called when the @manager content changed. It's a signal to
	 * regenerate the UI elements.
	 *
	 * Since: 3.56
	 **/
	signals[SIGNAL_CHANGED] = g_signal_new ("changed",
		E_TYPE_UI_MANAGER,
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		0,
		NULL, NULL,
		NULL,
		G_TYPE_NONE, 0,
		G_TYPE_NONE);

	/* void		freeze		(EUIManager *manager); */
	/**
	 * EUIManager::freeze:
	 * @manager: an #EUIManager
	 *
	 * A signal called when the @manager has called e_ui_manager_freeze().
	 *
	 * Since: 3.56
	 **/
	signals[SIGNAL_FREEZE] = g_signal_new ("freeze",
		E_TYPE_UI_MANAGER,
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		0,
		NULL, NULL,
		NULL,
		G_TYPE_NONE, 0,
		G_TYPE_NONE);

	/* void		thaw		(EUIManager *manager,
					 gboolean changed_while_frozen); */
	/**
	 * EUIManager::thaw:
	 * @manager: an #EUIManager
	 * @changed_while_frozen: whether changed while had been frozen
	 *
	 * A signal called when the @manager has called e_ui_manager_thaw().
	 * When the @changed_while_frozen is set to %TRUE, there will also
	 * be called the #EUIManager::changed signal once the @manager
	 * is completely unfrozen.
	 *
	 * Since: 3.56
	 **/
	signals[SIGNAL_THAW] = g_signal_new ("thaw",
		E_TYPE_UI_MANAGER,
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		0,
		NULL, NULL,
		NULL,
		G_TYPE_NONE, 1,
		G_TYPE_BOOLEAN);

	/* gboolean	create_item	(EUIManager *manager,
					 EUIElement *elem,
					 EUIAction *action,
					 EUIElementKind for_kind,
					 GObject **out_item); */
	/**
	 * EUIManager::create-item: (skip)
	 * @manager: an #EUIManager
	 * @elem: an #EUIElement to create the item for
	 * @action: an associated @EUIAction for the new item
	 * @for_kind: for which part of the UI the item should be created; it influences the type of the returned item
	 * @out_item: (out) (transfer full): a location to set the created item to
	 *
	 * Return %TRUE, when could create the item described by @elem with associated @action and
	 * sets it into the @out_item. The @for_kind can only be %E_UI_ELEMENT_KIND_HEADERBAR,
	 * %E_UI_ELEMENT_KIND_TOOLBAR or %E_UI_ELEMENT_KIND_MENU.
	 *
	 * For the %E_UI_ELEMENT_KIND_HEADERBAR any widget can be returned, though the mostly
	 * used might be an #EHeaderBarButton.
	 *
	 * For the %E_UI_ELEMENT_KIND_TOOLBAR only a #GtkToolItem descendant is expected.
	 *
	 * For the %E_UI_ELEMENT_KIND_MENU only a #GMenuItem descendant is expected.
	 *
	 * It's expected only one handler can create the item, thus the first
	 * which returns %TRUE will stop further signal emission.
	 *
	 * Returns: %TRUE when the item was created and set into the @out_item,
	 *   %FALSE when cannot create the item
	 *
	 * Since: 3.56
	 **/
	signals[SIGNAL_CREATE_ITEM] = g_signal_new ("create-item",
		E_TYPE_UI_MANAGER,
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		0,
		g_signal_accumulator_true_handled, NULL,
		NULL,
		G_TYPE_BOOLEAN, 4,
		G_TYPE_POINTER, /* EUIElement */
		E_TYPE_UI_ACTION,
		E_TYPE_UI_ELEMENT_KIND,
		G_TYPE_POINTER);

	/* gboolean	create_gicon	(EUIManager *manager,
					 const gchar *name,
					 GIcon **out_gicon); */
	/**
	 * EUIManager::create-gicon:
	 * @manager: an #EUIManager
	 * @name: a custom icon name
	 * @out_gicon: (out) (transfer full): an output location to store a created #GIcon to
	 *
	 * The actions can have defined custom icons as "gicon::name",
	 * aka with a "gicon::" prefix, which are looked for by the name (without
	 * the special prefix) using this callback.
	 *
	 * It's expected only one handler can create the #GIcon, thus the first
	 * which returns %TRUE will stop further signal emission.
	 *
	 * Returns: %TRUE when the item was created and set into the @out_gicon,
	 *   %FALSE when cannot create the item
	 *
	 * Since: 3.56
	 **/
	signals[SIGNAL_CREATE_GICON] = g_signal_new ("create-gicon",
		E_TYPE_UI_MANAGER,
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		0,
		g_signal_accumulator_true_handled, NULL,
		NULL,
		G_TYPE_BOOLEAN, 2,
		G_TYPE_STRING,
		G_TYPE_POINTER);

	/* gboolean	ignore_accel	(EUIManager *manager,
					 EUIAction *action); */
	/**
	 * EUIManager::ignore-accel:
	 * @manager: an #EUIManager
	 * @action: an #EUIAction whose accelerator is about to be activated
	 *
	 * The signal allows to ignore accelerators to be activated.
	 *
	 * Multiple signal handlers can be connected, the first which
	 * returns %TRUE will stop the signal emission and the accelerator
	 * will be ignored. When all handlers return %FALSE, the accelerator
	 * will be used and the @action will be activated.
	 *
	 * Returns: %TRUE, when the accel should be ignored, %FALSE to
	 *   allow the accel to be activated.
	 *
	 * Since:3.56
	 **/
	signals[SIGNAL_IGNORE_ACCEL] = g_signal_new ("ignore-accel",
		E_TYPE_UI_MANAGER,
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		0,
		g_signal_accumulator_true_handled, NULL,
		NULL,
		G_TYPE_BOOLEAN, 1,
		E_TYPE_UI_ACTION);
}

static void
e_ui_manager_init (EUIManager *self)
{
	g_weak_ref_init (&self->action_groups_widget, NULL);

	self->parser = e_ui_parser_new ();
	self->accel_group = gtk_accel_group_new ();
	self->action_groups = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_object_unref);
	self->gicons = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
	self->groups = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_ptr_array_unref);
	self->shortcut_actions = g_hash_table_new_full (e_ui_manager_shortcut_def_hash, e_ui_manager_shortcut_def_equal, g_free, (GDestroyNotify) g_ptr_array_unref);
	self->self_weak_ref_group = camel_weak_ref_group_new ();

	camel_weak_ref_group_set (self->self_weak_ref_group, self);

	g_signal_connect_object (self->parser, "changed",
		G_CALLBACK (e_ui_manager_parser_changed_cb), self, 0);
}

/**
 * e_ui_manager_new:
 * @customizer_filename: (nullable) (transfer full): an optional #EUICustomizer filename, or %NULL
 *
 * Creates a new #EUIManager. Use g_object_unref() to free it, when no longer needed.
 *
 * The @customizer_filename, if not %NULL, is a full path to a file where
 * UI customization are stored. If not set, the UI manager will not allow
 * the customizations. The function assumes ownership of the string.
 *
 * Returns: (transfer full): a new #EUIManager
 *
 * Since: 3.56
 **/
EUIManager *
e_ui_manager_new (gchar *customizer_filename)
{
	EUIManager *self;

	self = g_object_new (E_TYPE_UI_MANAGER,
		"customizer-filename", customizer_filename,
		NULL);

	g_free (customizer_filename);

	return self;
}

/**
 * e_ui_manager_get_parser:
 * @self: an #EUIManager
 *
 * Gets an #EUIParser associated with the @self. Any UI definitions
 * are loaded into it and the layouts are read from it.
 *
 * Returns: (transfer none): an #EUIParser
 *
 * Since: 3.56
 **/
EUIParser *
e_ui_manager_get_parser (EUIManager *self)
{
	g_return_val_if_fail (E_IS_UI_MANAGER (self), NULL);

	return self->parser;
}

/**
 * e_ui_manager_get_customizer:
 * @self: an #EUIManager
 *
 * Gets an #EUICustomizer for the @self.
 *
 * Returns: (transfer none) (nullable): an #EUICustomizer, or %NULL,
 *   when customizations are disabled
 *
 * Since: 3.56
 **/
EUICustomizer *
e_ui_manager_get_customizer (EUIManager *self)
{
	g_return_val_if_fail (E_IS_UI_MANAGER (self), NULL);

	return self->customizer;
}

/**
 * e_ui_manager_get_accel_group:
 * @self: an #EUIManager
 *
 * Gets a #GtkAccelGroup associated with the @self. Actions
 * with configured accelerators register themselves to this accel
 * group.
 *
 * It should be added to a #GtkWindow with gtk_window_add_accel_group().
 *
 * Returns: (transfer none): a #GtkAccelGroup
 *
 * Since: 3.56
 **/
GtkAccelGroup *
e_ui_manager_get_accel_group (EUIManager *self)
{
	g_return_val_if_fail (E_IS_UI_MANAGER (self), NULL);

	return self->accel_group;
}

/**
 * e_ui_manager_freeze:
 * @self: an #EUIManager
 *
 * Freezes change notifications on the @self. The function
 * can be called multiple times, only each call should be
 * followed by a corresponding e_ui_manager_thaw().
 *
 * Since: 3.56
 **/
void
e_ui_manager_freeze (EUIManager *self)
{
	g_return_if_fail (E_IS_UI_MANAGER (self));
	g_return_if_fail (self->frozen + 1 > self->frozen);

	self->frozen++;

	g_signal_emit (self, signals[SIGNAL_FREEZE], 0, NULL);
}

/**
 * e_ui_manager_thaw:
 * @self: an #EUIManager
 *
 * Reverts effect of one e_ui_manager_freeze() call.
 * It's a programming error to call this function when
 * the @self is not frozen.
 *
 * Since: 3.56
 **/
void
e_ui_manager_thaw (EUIManager *self)
{
	g_return_if_fail (E_IS_UI_MANAGER (self));
	g_return_if_fail (self->frozen > 0);

	self->frozen--;

	g_signal_emit (self, signals[SIGNAL_THAW], 0, self->changed_while_frozen, NULL);

	if (!self->frozen && self->changed_while_frozen) {
		self->changed_while_frozen = FALSE;

		e_ui_manager_changed (self);
	}
}

/**
 * e_ui_manager_is_frozen:
 * @self: an #EUIManager
 *
 * Check whether the change notifications are frozen
 * with e_ui_manager_freeze() call.
 *
 * Returns: whether change notifications are frozen
 *
 * Since: 3.56
 **/
gboolean
e_ui_manager_is_frozen (EUIManager *self)
{
	g_return_val_if_fail (E_IS_UI_MANAGER (self), FALSE);

	return self->frozen > 0;
}

/**
 * e_ui_manager_changed:
 * @self: an #EUIManager
 *
 * Let the @self know that something changed, indicating
 * the UI parts should be regenerated. In case the @self
 * is frozen, the regeneration is done once it's unfrozen.
 *
 * Since: 3.56
 **/
void
e_ui_manager_changed (EUIManager *self)
{
	g_return_if_fail (E_IS_UI_MANAGER (self));

	if (self->frozen) {
		self->changed_while_frozen = TRUE;
		return;
	}

	e_ui_manager_gather_groups (self);
	g_signal_emit (self, signals[SIGNAL_CHANGED], 0, NULL);
}

/*
 * e_ui_manager_set_in_accel_activation:
 * @self: an #EUIManager
 * @value: a value to set
 *
 * Sets a flag that the @self activates an action as part of the accelerator
 * activation, thus by a key press. It can be called multiple times, but
 * each call with %TRUE requires a counter part call with %FALSE.
 *
 * Since: 3.58
 */
static void
e_ui_manager_set_in_accel_activation (EUIManager *self,
				      gboolean value)
{
	if (value) {
		self->in_accel_activation++;
	} else {
		g_return_if_fail (self->in_accel_activation > 0);
		self->in_accel_activation--;
	}
}

/**
 * e_ui_manager_get_in_accel_activation:
 * @self: an #EUIManager
 *
 * Gets whether the @self is currently in an accelerator activation
 * process, thus whether an action is activated due to a key press.
 *
 * Returns: %TRUE when processing accelerator, %FALSE otherwise
 *
 * Since: 3.58
 **/
gboolean
e_ui_manager_get_in_accel_activation (EUIManager *self)
{
	g_return_val_if_fail (E_IS_UI_MANAGER (self), FALSE);

	return self->in_accel_activation > 0;
}

static gboolean
e_ui_manager_can_process_accel (EUIManager *self,
				EUIAction *action)
{
	gboolean ignore = FALSE;

	if (!e_ui_action_is_visible (action) ||
	    !g_action_get_enabled (G_ACTION (action))) {
		return FALSE;
	}

	g_signal_emit (self, signals[SIGNAL_IGNORE_ACCEL], 0, action, &ignore);

	return !ignore;
}

static gboolean
e_ui_manager_accel_activated_cb (GtkAccelGroup *accel_group,
				 GObject *acceleratable,
				 guint key,
				 GdkModifierType mods,
				 gpointer user_data)
{
	CamelWeakRefGroup *weak_ref_group = user_data;
	EUIManager *self = camel_weak_ref_group_get (weak_ref_group);
	EUIManagerShortcutDef sdef = { 0, 0 };
	GPtrArray *actions;
	gboolean handled = FALSE;

	if (!self)
		return FALSE;

	sdef.key = key;
	sdef.mods = mods;

	actions = g_hash_table_lookup (self->shortcut_actions, &sdef);

	if (actions) {
		guint ii;

		for (ii = 0; ii < actions->len; ii++) {
			GAction *action = g_ptr_array_index (actions, ii);

			if (e_ui_manager_can_process_accel (self, E_UI_ACTION (action))) {
				const GVariantType *param_type;

				e_ui_manager_set_in_accel_activation (self, TRUE);

				param_type = g_action_get_parameter_type (action);
				if (!param_type) {
					g_action_activate (action, NULL);
				} else if (g_variant_type_equal (param_type, G_VARIANT_TYPE_BOOLEAN)) {
					GVariant *current_value = g_action_get_state (action);
					GVariant *new_value;

					new_value = g_variant_new_boolean (current_value ? !g_variant_get_boolean (current_value) : TRUE);
					g_variant_ref_sink (new_value);

					g_action_activate (action, new_value);

					g_clear_pointer (&current_value, g_variant_unref);
					g_clear_pointer (&new_value, g_variant_unref);
				} else {
					GVariant *target;

					target = e_ui_action_ref_target (E_UI_ACTION (action));
					g_action_activate (action, target);
					g_clear_pointer (&target, g_variant_unref);
				}

				e_ui_manager_set_in_accel_activation (self, FALSE);
				handled = TRUE;
				break;
			}
		}
	} else {
		g_warning ("%s: No action found for key 0x%x and mods 0x%x", G_STRFUNC, key, mods);
	}

	g_clear_object (&self);

	return handled;
}

static void
e_ui_manager_connect_accel_cb (EUIManager *self,
			       EUIAction *action,
			       const gchar *accel,
			       gpointer user_data)
{
	EUIManagerShortcutDef sdef = { 0, 0 };

	g_return_if_fail (E_IS_UI_MANAGER (self));

	if (!self->accel_group || !accel || !*accel)
		return;

	gtk_accelerator_parse (accel, &sdef.key, &sdef.mods);

	if (sdef.key != 0) {
		GPtrArray *actions;
		guint ii;

		actions = g_hash_table_lookup (self->shortcut_actions, &sdef);

		if (!actions) {
			EUIManagerShortcutDef *sdef_ptr;
			GClosure *closure;

			sdef_ptr = g_new0 (EUIManagerShortcutDef, 1);
			sdef_ptr->key = sdef.key;
			sdef_ptr->mods = sdef.mods;

			actions = g_ptr_array_new_with_free_func (g_object_unref);
			g_hash_table_insert (self->shortcut_actions, sdef_ptr, actions);

			closure = g_cclosure_new (G_CALLBACK (e_ui_manager_accel_activated_cb),
				camel_weak_ref_group_ref (self->self_weak_ref_group),
				(GClosureNotify) camel_weak_ref_group_unref);

			gtk_accel_group_connect (self->accel_group, sdef.key, sdef.mods, GTK_ACCEL_LOCKED, closure);
		}

		for (ii = 0; ii < actions->len; ii++) {
			EUIAction *known_action = g_ptr_array_index (actions, ii);

			if (action == known_action)
				break;
		}

		/* not in the list yet */
		if (ii >= actions->len)
			g_ptr_array_add (actions, g_object_ref (action));
	} else {
		EUIActionGroup *action_group = e_ui_action_get_action_group (action);

		g_warning ("%s: Failed to parse accel '%s' on action '%s.%s'", G_STRFUNC, accel,
			action_group ? e_ui_action_group_get_name (action_group) : "no-group-set",
			g_action_get_name (G_ACTION (action)));
	}
}

static void
e_ui_manager_disconnect_accel_cb (EUIManager *self,
				  EUIAction *action,
				  const gchar *accel,
				  gpointer user_data)
{
	EUIManagerShortcutDef sdef = { 0, 0 };

	g_return_if_fail (E_IS_UI_MANAGER (self));

	if (!self->accel_group || !accel || !*accel)
		return;

	gtk_accelerator_parse (accel, &sdef.key, &sdef.mods);

	if (sdef.key != 0) {
		GPtrArray *actions;

		actions = g_hash_table_lookup (self->shortcut_actions, &sdef);

		if (actions) {
			guint ii;

			for (ii = 0; ii < actions->len; ii++) {
				EUIAction *known_action = g_ptr_array_index (actions, ii);

				if (known_action == action) {
					g_ptr_array_remove_index (actions, ii);
					if (!actions->len) {
						g_hash_table_remove (self->shortcut_actions, &sdef);
						gtk_accel_group_disconnect_key (self->accel_group, sdef.key, sdef.mods);
					}
					break;
				}
			}
		}
	}
}

static void
e_ui_manager_foreach_action_accel (EUIManager *self,
				   EUIAction *action,
				   void (*func) (EUIManager *self,
						 EUIAction *action,
						 const gchar *accel,
						 gpointer user_data),
				   gpointer user_data)
{
	GPtrArray *custom_accels, *secondary_accels;
	const gchar *accel;
	guint offset = 0;

	custom_accels = self->customizer ? e_ui_customizer_get_accels (self->customizer, g_action_get_name (G_ACTION (action))) : NULL;

	accel = custom_accels && custom_accels->len > 0 ? g_ptr_array_index (custom_accels, 0) : NULL;
	if (!accel && !custom_accels)
		accel = e_ui_action_get_accel (action);
	if (accel && *accel)
		func (self, action, accel, user_data);

	secondary_accels = custom_accels;
	if (secondary_accels)
		offset = 1;
	else
		secondary_accels = e_ui_action_get_secondary_accels (action);
	if (secondary_accels) {
		guint ii;

		for (ii = offset; ii < secondary_accels->len; ii++) {
			accel = g_ptr_array_index (secondary_accels, ii);

			if (accel && *accel)
				func (self, action, accel, user_data);
		}
	}
}

static void
e_ui_manager_add_action_accel (EUIManager *self,
			       EUIAction *action)
{
	e_ui_manager_foreach_action_accel (self, action, e_ui_manager_connect_accel_cb, NULL);
}

static void
e_ui_manager_add_action_internal (EUIManager *self,
				  EUIActionGroup *action_group,
				  EUIAction *action,
				  EUIActionFunc activate,
				  EUIActionFunc change_state,
				  gpointer user_data)
{
	if (activate)
		g_signal_connect (action, "activate", G_CALLBACK (activate), user_data);

	if (change_state)
		g_signal_connect (action, "change-state", G_CALLBACK (change_state), user_data);

	/* this calls e_ui_manager_add_action_accel() in the group's "added" signal handler */
	e_ui_action_group_add (action_group, action);
}

/**
 * e_ui_manager_add_actions:
 * @self: an #EUIManager
 * @group_name: (not nullable): a name of an action group to add the actions to
 * @translation_domain: (nullable): translation domain for the @entries' localized members, or %NULL
 * @entries: action entries to be added, as #EUIActionEntry array
 * @n_entries: how many items @entries has, or -1 when NULL-terminated
 * @user_data: user data to pass to action callbacks
 *
 * Just like g_action_map_add_action_entries(), only uses #EUIAction instead
 * of #GSimpleAction actions and adds the actions into a named action group.
 * When there is no action group of the @group_name, a new is added.
 *
 * The @translation_domain can be #NULL, in which case the Evolution's translation
 * domain is used. It's used also when the @translation_domain is an empty string.
 *
 * Since: 3.56
 **/
void
e_ui_manager_add_actions (EUIManager *self,
			  const gchar *group_name,
			  const gchar *translation_domain,
			  const EUIActionEntry *entries,
			  gint n_entries,
			  gpointer user_data)
{
	EUIActionGroup *action_group;
	const gchar *domain;
	guint ii;

	g_return_if_fail (E_IS_UI_MANAGER (self));
	g_return_if_fail (group_name != NULL);
	g_return_if_fail (entries != NULL || n_entries == 0);

	domain = translation_domain;
	if (!domain || !*domain)
		domain = GETTEXT_PACKAGE;

	action_group = e_ui_manager_get_action_group (self, group_name);

	for (ii = 0; n_entries < 0 ? entries[ii].name != NULL : ii < n_entries; ii++) {
		const EUIActionEntry *entry = &(entries[ii]);
		EUIAction *action;

		action = e_ui_action_new_from_entry (group_name, entry, domain);
		if (!action)
			continue;

		e_ui_manager_add_action_internal (self, action_group, action, entry->activate, entry->change_state, user_data);

		g_object_unref (action);
	}

	e_ui_manager_changed (self);
}

/**
 * e_ui_manager_add_actions_enum:
 * @self: an #EUIManager
 * @group_name: (not nullable): a name of an action group to add the actions to
 * @translation_domain: (nullable): translation domain for the @entries' localized members, or %NULL
 * @entries: action entries to be added, as #EUIActionEnumEntry array
 * @n_entries: how many items @entries has, or -1 when NULL-terminated
 * @user_data: user data to pass to action callbacks
 *
 * The same as e_ui_manager_add_actions(), only creates radio actions, whose
 * states correspond to certain enum value. If bindings between the action's
 * state and a corresponding enum #GObject property is needed, a utility function
 * e_ui_action_util_gvalue_to_enum_state() and e_ui_action_util_enum_state_to_gvalue()
 * can be used to transform the value from the #GObject to an action state or vice versa.
 *
 * The @translation_domain can be %NULL, in which case the Evolution's translation
 * domain is used. It's used also when the @translation_domain is an empty string.
 *
 * Since: 3.56
 **/
void
e_ui_manager_add_actions_enum (EUIManager *self,
			       const gchar *group_name,
			       const gchar *translation_domain,
			       const EUIActionEnumEntry *entries,
			       gint n_entries,
			       gpointer user_data)
{
	EUIActionGroup *action_group;
	const gchar *domain;
	guint ii;

	g_return_if_fail (E_IS_UI_MANAGER (self));
	g_return_if_fail (group_name != NULL);
	g_return_if_fail (entries != NULL || n_entries == 0);

	domain = translation_domain;
	if (!domain || !*domain)
		domain = GETTEXT_PACKAGE;

	action_group = e_ui_manager_get_action_group (self, group_name);

	for (ii = 0; n_entries < 0 ? entries[ii].name != NULL : ii < n_entries; ii++) {
		const EUIActionEnumEntry *entry = &(entries[ii]);
		EUIAction *action;

		action = e_ui_action_new_from_enum_entry (group_name, entry, domain);
		if (!action)
			continue;

		e_ui_manager_add_action_internal (self, action_group, action, entry->activate, (EUIActionFunc) e_ui_action_set_state, user_data);

		g_object_unref (action);
	}

	e_ui_manager_changed (self);
}

/**
 * e_ui_manager_add_action:
 * @self: an #EUIManager
 * @group_name: (not nullable): a name of an action group to add the action to
 * @action: (transfer none): the action to add
 * @activate: (nullable): an optional callback to call on "activate" signal, or %NULL
 * @change_state: (nullable): an optional callback to call on "change-state" signal, or %NULL
 * @user_data: user data to pass to action callbacks
 *
 * Adds a single @action to the @group_name. The actions group cannot contain
 * an action of the same name.
 *
 * Since: 3.56
 **/
void
e_ui_manager_add_action (EUIManager *self,
			 const gchar *group_name,
			 EUIAction *action,
			 EUIActionFunc activate,
			 EUIActionFunc change_state,
			 gpointer user_data)
{
	EUIActionGroup *action_group;

	g_return_if_fail (E_IS_UI_MANAGER (self));
	g_return_if_fail (group_name != NULL);
	g_return_if_fail (E_IS_UI_ACTION (action));

	action_group = e_ui_manager_get_action_group (self, group_name);
	e_ui_manager_add_action_internal (self, action_group, action, activate, change_state, user_data);

	e_ui_manager_changed (self);
}

/**
 * e_ui_manager_add_actions_with_eui_data:
 * @self: an #EUIManager
 * @group_name: (not nullable): a name of an action group to add the actions to
 * @translation_domain: (nullable): translation domain for the @entries' localized members, or %NULL
 * @entries: action entries to be added, as #EUIActionEntry array
 * @n_entries: how many items @entries has, or -1 when NULL-terminated
 * @user_data: user data to pass to action callbacks
 * @eui: Evolution UI definition
 *
 * Combines e_ui_manager_add_actions() with e_ui_parser_merge_data(), in this order,
 * printing any errors in the @eui into the terminal. This can be used to add built-in
 * UI definition with the related actions in a single call.
 *
 * Since: 3.56
 **/
void
e_ui_manager_add_actions_with_eui_data (EUIManager *self,
					const gchar *group_name,
					const gchar *translation_domain,
					const EUIActionEntry *entries,
					gint n_entries,
					gpointer user_data,
					const gchar *eui)
{
	GError *local_error = NULL;

	g_return_if_fail (E_IS_UI_MANAGER (self));
	g_return_if_fail (group_name != NULL);
	g_return_if_fail (entries != NULL || n_entries == 0);
	g_return_if_fail (eui != NULL);

	e_ui_manager_add_actions (self, group_name, translation_domain, entries, n_entries, user_data);

	if (!e_ui_parser_merge_data (e_ui_manager_get_parser (self), eui, -1, &local_error))
		g_critical ("%s: Failed to merge built-in UI definition: %s", G_STRFUNC, local_error ? local_error->message : "Unknown error");

	g_clear_error (&local_error);
}

/**
 * e_ui_manager_add_action_groups_to_widget:
 * @self: an #EUIManager
 * @widget: a #GtkWidget
 *
 * Adds all currently known action groups into the @widget with
 * gtk_widget_insert_action_group(). In contrast to e_ui_manager_set_action_groups_widget(),
 * this does not add any later added action groups into the @widget.
 * In other words, this is a one-time operation only.
 *
 * Since: 3.56
 **/
void
e_ui_manager_add_action_groups_to_widget (EUIManager *self,
					  GtkWidget *widget)
{
	GHashTableIter iter;
	gpointer key = NULL, value = NULL;

	g_return_if_fail (E_IS_UI_MANAGER (self));
	g_return_if_fail (GTK_IS_WIDGET (widget));

	g_hash_table_iter_init (&iter, self->action_groups);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		const gchar *name = key;
		EUIActionGroup *group = value;

		gtk_widget_insert_action_group (widget, name, G_ACTION_GROUP (group));
	}
}

/**
 * e_ui_manager_set_action_groups_widget:
 * @self: an #EUIManager
 * @widget: (nullable): a #GtkWidget, or %NULL
 *
 * Sets the @widget to be the one where all action groups will
 * be inserted. When the @self creates a new action group, it
 * is automatically added into the @widget.
 *
 * When the @widget is %NULL, any previous widget is unset.
 *
 * Overwriting any existing @widget will remove the groups
 * from the previous widget first.
 *
 * Since: 3.56
 **/
void
e_ui_manager_set_action_groups_widget (EUIManager *self,
				       GtkWidget *widget)
{
	GtkWidget *current;
	GHashTableIter iter;
	gpointer key = NULL, value = NULL;

	g_return_if_fail (E_IS_UI_MANAGER (self));
	if (widget)
		g_return_if_fail (GTK_IS_WIDGET (widget));

	current = g_weak_ref_get (&self->action_groups_widget);

	if (current == widget) {
		g_clear_object (&current);
		return;
	}

	g_hash_table_iter_init (&iter, self->action_groups);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		const gchar *name = key;
		EUIActionGroup *group = value;

		if (current)
			gtk_widget_insert_action_group (current, name, NULL);

		if (widget)
			gtk_widget_insert_action_group (widget, name, G_ACTION_GROUP (group));
	}

	g_weak_ref_set (&self->action_groups_widget, widget);

	g_clear_object (&current);
}

/**
 * e_ui_manager_ref_action_groups_widget:
 * @self: an #EUIManager
 *
 * References a #GtkWidget, which is used to add action groups to.
 *
 * The returned widget, if not %NULL, should be dereferenced
 * with g_object_unref(), when no longer needed.
 *
 * Returns: (transfer full) (nullable): a referenced #GtkWidget used
 *    to add action groups to, or %NULL, when none is set
 *
 * Since: 3.56
 **/
GtkWidget *
e_ui_manager_ref_action_groups_widget (EUIManager *self)
{
	g_return_val_if_fail (E_IS_UI_MANAGER (self), NULL);

	return g_weak_ref_get (&self->action_groups_widget);
}

/**
 * e_ui_manager_has_action_group:
 * @self: an #EUIManager
 * @name: an action group name
 *
 * Check whether an action group named @name already exists in @self.
 *
 * Returns: %TRUE, when the action group exists, %FALSE otherwise
 *
 * Since: 3.56
 **/
gboolean
e_ui_manager_has_action_group (EUIManager *self,
			       const gchar *name)
{
	g_return_val_if_fail (E_IS_UI_MANAGER (self), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);

	return g_hash_table_lookup (self->action_groups, name) != NULL;
}

static void
e_ui_manager_action_group_action_added_cb (EUIActionGroup *action_group,
					   EUIAction *action,
					   gpointer user_data)
{
	EUIManager *self = user_data;

	e_ui_manager_add_action_accel (self, action);
}

static void
e_ui_manager_action_group_action_removed_cb (EUIActionGroup *action_group,
					     EUIAction *action,
					     gpointer user_data)
{
	EUIManager *self = user_data;

	e_ui_manager_foreach_action_accel (self, action, e_ui_manager_disconnect_accel_cb, NULL);
}

static void
e_ui_manager_claim_new_action_group (EUIManager *self,
				     EUIActionGroup *action_group) /* (transfer full) */
{
	GtkWidget *action_groups_widget;
	GPtrArray *actions;
	const gchar *name;
	guint ii;

	name = e_ui_action_group_get_name (action_group);

	g_hash_table_insert (self->action_groups, (gpointer) name, action_group);

	actions = e_ui_action_group_list_actions (action_group);
	for (ii = 0; ii < actions->len; ii++) {
		EUIAction *action = g_ptr_array_index (actions, ii);

		e_ui_manager_add_action_accel (self, action);
	}

	g_clear_pointer (&actions, g_ptr_array_unref);

	action_groups_widget = g_weak_ref_get (&self->action_groups_widget);
	if (action_groups_widget) {
		gtk_widget_insert_action_group (action_groups_widget, name, G_ACTION_GROUP (action_group));
		g_clear_object (&action_groups_widget);
	}

	g_signal_connect_object (action_group, "added",
		G_CALLBACK (e_ui_manager_action_group_action_added_cb), self, 0);
	g_signal_connect_object (action_group, "removed",
		G_CALLBACK (e_ui_manager_action_group_action_removed_cb), self, 0);
	g_signal_connect_object (action_group, "accel-added",
		G_CALLBACK (e_ui_manager_connect_accel_cb), self, G_CONNECT_SWAPPED);
	g_signal_connect_object (action_group, "accel-removed",
		G_CALLBACK (e_ui_manager_disconnect_accel_cb), self, G_CONNECT_SWAPPED);
}

/**
 * e_ui_manager_get_action_group:
 * @self: an #EUIManager
 * @name: an action group name
 *
 * Gets an #EUIActionGroup named @name. If no such exists, a new is created.
 * Use e_ui_manager_has_action_group() to check whether an action group exists.
 *
 * Returns: (transfer none): an #EUIActionGroup named @name
 *
 * Since: 3.56
 **/
EUIActionGroup *
e_ui_manager_get_action_group (EUIManager *self,
			       const gchar *name)
{
	EUIActionGroup *action_group;

	g_return_val_if_fail (E_IS_UI_MANAGER (self), NULL);
	g_return_val_if_fail (name != NULL, NULL);

	action_group = g_hash_table_lookup (self->action_groups, name);
	if (!action_group) {
		action_group = e_ui_action_group_new (name);

		e_ui_manager_claim_new_action_group (self, action_group);
	}

	return action_group;
}

/**
 * e_ui_manager_add_action_group:
 * @self: an #EUIManager
 * @action_group: (transfer none): an #EUIActionGroup to add
 *
 * Adds an existing action group to the @self, being able to
 * call actions from it. The function does nothing when the @action_group
 * is already part of the @self, nonetheless it's considered a programming
 * error to try to add a different group of the same name into the @self.
 *
 * Since: 3.56
 **/
void
e_ui_manager_add_action_group (EUIManager *self,
			       EUIActionGroup *action_group)
{
	EUIActionGroup *existing_group;

	g_return_if_fail (E_IS_UI_MANAGER (self));
	g_return_if_fail (E_IS_UI_ACTION_GROUP (action_group));

	existing_group = g_hash_table_lookup (self->action_groups, e_ui_action_group_get_name (action_group));

	if (existing_group && existing_group != action_group) {
		g_warning ("%s: A different action group of the name '%s' already exists, ignoring the new group",
			G_STRFUNC, e_ui_action_group_get_name (action_group));
		return;
	} else if (existing_group == action_group) {
		return;
	}

	e_ui_manager_claim_new_action_group (self, g_object_ref (action_group));
}

/**
 * e_ui_manager_list_action_groups:
 * @self: an #EUIManager
 *
 * Lists all action groups added into the @self.
 *
 * Returns: (transfer container) (element-type EUIActionGroup): a #GPtrArray of an #EUIActionGroup groups
 *
 * Since: 3.56
 **/
GPtrArray *
e_ui_manager_list_action_groups (EUIManager *self)
{
	GPtrArray *groups;
	GHashTableIter iter;
	gpointer value = NULL;

	g_return_val_if_fail (E_IS_UI_MANAGER (self), NULL);

	groups = g_ptr_array_new_full (g_hash_table_size (self->action_groups), g_object_unref);

	g_hash_table_iter_init (&iter, self->action_groups);
	while (g_hash_table_iter_next (&iter, NULL, &value)) {
		EUIActionGroup *group = value;

		if (group)
			g_ptr_array_add (groups, g_object_ref (group));
	}

	return groups;
}

/**
 * e_ui_manager_get_action:
 * @self: an #EUIManager
 * @name: an action name
 *
 * Looks up an #EUIAction by its @name among all known action maps and returns it.
 *
 * Returns: (nullable) (transfer none): an #EUIAction named @name, or %NULL, if not found
 *
 * Since: 3.56
 **/
EUIAction *
e_ui_manager_get_action (EUIManager *self,
			 const gchar *name)
{
	GHashTableIter iter;
	gpointer key = NULL, value = NULL;

	g_return_val_if_fail (E_IS_UI_MANAGER (self), NULL);
	g_return_val_if_fail (name != NULL, NULL);

	g_hash_table_iter_init (&iter, self->action_groups);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		const gchar *group_name = key;
		EUIActionGroup *action_group = value;

		if (action_group) {
			EUIAction *action;

			action = e_ui_action_group_get_action (action_group, name);
			if (action) {
				if (E_IS_UI_ACTION (action))
					return action;

				g_warning ("%s: Found action '%s' in action group '%s', but it's not an EUIAction, it's %s instead",
					G_STRFUNC, name, group_name, G_OBJECT_TYPE_NAME (action));
				break;
			}
		}
	}

	return NULL;
}

/**
 * e_ui_manager_get_gicon:
 * @self: an #EUIManager
 * @name: a #GIcon name to retrieve
 *
 * Retrieves a named #GIcon. These are cached. The signal "create-gicon"
 * is used to create icons prefixed with "gicon::" in the .eui definitions,
 * as they are custom icons. The @name is without this prefix.
 *
 * While returning %NULL is possible, it's considered an error and
 * a runtime warning is printed on the terminal, thus there should
 * always be a "create-gicon" signal handler providing the #GIcon.
 *
 * Returns: (transfer none) (nullable): a #GIcon named @name, or %NULL, if not found
 *
 * Since: 3.56
 **/
GIcon *
e_ui_manager_get_gicon (EUIManager *self,
			const gchar *name)
{
	GIcon *gicon = NULL;

	g_return_val_if_fail (E_IS_UI_MANAGER (self), NULL);
	g_return_val_if_fail (name != NULL, NULL);

	gicon = g_hash_table_lookup (self->gicons, name);

	if (!gicon) {
		gboolean handled = FALSE;

		g_signal_emit (self, signals[SIGNAL_CREATE_GICON], 0, name, &gicon, &handled);

		if (!gicon) {
			g_warning ("%s: Nothing created gicon '%s'", G_STRFUNC, name);
			gicon = g_themed_icon_new ("image-missing");
		}

		g_hash_table_insert (self->gicons, g_strdup (name), gicon);
	}

	return gicon;
}

static gboolean
eum_parent_is_header_bar_button (GtkWidget *child)
{
	while (child && !E_IS_HEADER_BAR_BUTTON (child))
		child = gtk_widget_get_parent (child);

	return child != NULL;
}

static void
e_ui_manager_add_css_classes (EUIManager *self,
			      GtkWidget *item,
			      const gchar *css_classes) /* space-separated */
{
	EHeaderBarButton *header_bar_button = NULL;
	GtkStyleContext *style_context;

	if (!item || !css_classes || !*css_classes)
		return;

	header_bar_button = E_IS_HEADER_BAR_BUTTON (item) ? E_HEADER_BAR_BUTTON (item) : NULL;
	style_context = gtk_widget_get_style_context (item);

	if (strchr (css_classes, ' ')) {
		gchar **strv = g_strsplit (css_classes, " ", -1);
		guint ii;

		for (ii = 0; strv && strv[ii]; ii++) {
			gchar *css_class = g_strchomp (strv[ii]);

			if (*css_class) {
				if (header_bar_button)
					e_header_bar_button_css_add_class (header_bar_button, css_class);
				else
					gtk_style_context_add_class (style_context, css_class);
			}
		}

		g_strfreev (strv);
	} else {
		if (header_bar_button)
			e_header_bar_button_css_add_class (header_bar_button, css_classes);
		else
			gtk_style_context_add_class (style_context, css_classes);
	}
}

static GObject *
e_ui_manager_create_headerbar_item (EUIManager *self,
				    EUIElement *elem,
				    EUIAction *action)
{
	GtkWidget *item;

	item = e_header_bar_button_new (e_ui_action_get_label (action), action, self);

	if (e_ui_element_item_get_icon_only_is_set (elem))
		e_header_bar_button_set_show_icon_only (E_HEADER_BAR_BUTTON (item), e_ui_element_item_get_icon_only (elem));

	e_ui_manager_add_css_classes (self, item, e_ui_element_item_get_css_classes (elem));
	e_ui_manager_update_item_from_action (self, item, action);

	return G_OBJECT (item);
}

static GObject *
e_ui_manager_create_toolbar_item (EUIManager *self,
				  EUIElement *elem,
				  EUIAction *action)
{
	GtkToolButton *item = NULL;
	GVariant *state;

	state = g_action_get_state (G_ACTION (action));

	if (e_ui_action_get_radio_group (action))
		item = GTK_TOOL_BUTTON (gtk_toggle_tool_button_new ());
	else if (state && g_variant_is_of_type (state, G_VARIANT_TYPE_BOOLEAN))
		item = GTK_TOOL_BUTTON (gtk_toggle_tool_button_new ());
	else
		item = GTK_TOOL_BUTTON (gtk_tool_button_new (NULL, NULL));

	g_clear_pointer (&state, g_variant_unref);

	e_ui_manager_add_css_classes (self, GTK_WIDGET (item), e_ui_element_item_get_css_classes (elem));
	gtk_tool_item_set_is_important (GTK_TOOL_ITEM (item), e_ui_element_item_get_important (elem));
	e_ui_manager_update_item_from_action (self, item, action);

	return G_OBJECT (item);
}

static GObject *
e_ui_manager_create_menu_item (EUIManager *self,
			       EUIElement *elem,
			       EUIAction *action)
{
	GMenuItem *item;

	item = g_menu_item_new (NULL, NULL);

	e_ui_manager_update_item_from_action (self, item, action);

	if (e_ui_element_item_get_text_only_is_set (elem) &&
	    e_ui_element_item_get_text_only (elem))
		g_menu_item_set_attribute (item, "icon", NULL);

	return G_OBJECT (item);
}

static GObject *
e_ui_manager_create_item_one (EUIManager *self,
			      EUIElement *elem,
			      EUIAction *action,
			      EUIElementKind for_kind)
{
	GObject *item = NULL;
	gboolean handled = FALSE;

	g_return_val_if_fail (elem != NULL, NULL);
	g_return_val_if_fail (action != NULL, NULL);
	g_return_val_if_fail (for_kind == E_UI_ELEMENT_KIND_MENU || for_kind == E_UI_ELEMENT_KIND_HEADERBAR || for_kind == E_UI_ELEMENT_KIND_TOOLBAR, NULL);

	g_signal_emit (self, signals[SIGNAL_CREATE_ITEM], 0, elem, action, for_kind, &item, &handled);

	if (!handled && !item) {
		switch (for_kind) {
		case E_UI_ELEMENT_KIND_HEADERBAR:
			item = e_ui_manager_create_headerbar_item (self, elem, action);
			break;
		case E_UI_ELEMENT_KIND_TOOLBAR:
			item = e_ui_manager_create_toolbar_item (self, elem, action);
			break;
		case E_UI_ELEMENT_KIND_MENU:
			item = e_ui_manager_create_menu_item (self, elem, action);
			break;
		default:
			g_warn_if_reached ();
			break;
		}
	}

	return item;
}

static EUIElement *
eum_get_ui_element (EUIManager *self,
		    const gchar *id,
		    gboolean *out_is_customized)
{
	EUIElement *elem = NULL;

	*out_is_customized = FALSE;

	if (self->customizer)
		elem = e_ui_customizer_get_element (self->customizer, id);

	if (!elem && e_ui_parser_get_root (self->parser))
		elem = e_ui_element_get_child_by_id (e_ui_parser_get_root (self->parser), id);
	else
		*out_is_customized = TRUE;

	return elem;
}

static void
eum_traverse_headerbar_rec (EUIManager *self,
			    EHeaderBar *eheaderbar,
			    GtkHeaderBar *gtkheaderbar,
			    EUIElement *parent_elem,
			    gboolean add_to_start,
			    GHashTable *groups,
			    gboolean *inout_need_separator,
			    gboolean *inout_any_added,
			    gboolean is_customized)
{
	guint ii, len;

	len = e_ui_element_get_n_children (parent_elem);

	for (ii = 0; ii < len; ii++) {
		EUIElement *elem = e_ui_element_get_child (parent_elem, add_to_start ? ii : len - ii - 1);
		EUIAction *action;

		if (!elem)
			continue;

		switch (e_ui_element_get_kind (elem)) {
		case E_UI_ELEMENT_KIND_ITEM:
			action = e_ui_manager_get_action (self, e_ui_element_item_get_action (elem));
			if (action) {
				GObject *item;

				item = e_ui_manager_create_item_one (self, elem, action, E_UI_ELEMENT_KIND_HEADERBAR);
				if (item) {
					GPtrArray *radio_group;

					if (*inout_any_added && *inout_need_separator) {
						GtkWidget *separator;

						separator = gtk_separator_new (GTK_ORIENTATION_VERTICAL);
						gtk_widget_set_visible (separator, TRUE);

						if (eheaderbar) {
							if (add_to_start)
								e_header_bar_pack_start (eheaderbar, separator, 0);
							else
								e_header_bar_pack_end (eheaderbar, separator, 0);
						} else {
							if (add_to_start)
								gtk_header_bar_pack_start (gtkheaderbar, separator);
							else
								gtk_header_bar_pack_end (gtkheaderbar, separator);
						}
					}

					*inout_any_added = TRUE;
					*inout_need_separator = FALSE;

					radio_group = e_ui_action_get_radio_group (action);
					if (radio_group && GTK_IS_RADIO_BUTTON (item)) {
						GSList **pgroup;

						pgroup = g_hash_table_lookup (groups, radio_group);
						if (pgroup) {
							gtk_radio_button_set_group (GTK_RADIO_BUTTON (item), *pgroup);
						} else {
							pgroup = g_new0 (GSList *, 1);
							*pgroup = gtk_radio_button_get_group (GTK_RADIO_BUTTON (item));
							g_hash_table_insert (groups, radio_group, pgroup);
						}
					}

					if (eheaderbar) {
						if (add_to_start)
							e_header_bar_pack_start (eheaderbar, GTK_WIDGET (item), e_ui_element_item_get_label_priority (elem));
						else
							e_header_bar_pack_end (eheaderbar, GTK_WIDGET (item), e_ui_element_item_get_label_priority (elem));
					} else {
						if (add_to_start)
							gtk_header_bar_pack_start (gtkheaderbar, GTK_WIDGET (item));
						else
							gtk_header_bar_pack_end (gtkheaderbar, GTK_WIDGET (item));
					}
				}
			} else if (!is_customized) {
				g_warning ("%s: Cannot find action '%s' for an item", G_STRFUNC, e_ui_element_item_get_action (elem));
			}
			break;
		case E_UI_ELEMENT_KIND_SEPARATOR:
			*inout_need_separator = *inout_any_added;
			break;
		case E_UI_ELEMENT_KIND_PLACEHOLDER:
			eum_traverse_headerbar_rec (self, eheaderbar, gtkheaderbar, elem, add_to_start, groups, inout_need_separator, inout_any_added, is_customized);
			break;
		default:
			g_warn_if_reached ();
			break;
		}
	}
}

static void
eum_traverse_headerbar (EUIManager *self,
			EHeaderBar *eheaderbar,
			GtkHeaderBar *gtkheaderbar,
			EUIElement *elem,
			gboolean is_customized)
{
	GHashTable *groups; /* gpointer group ~> GSList ** */
	gboolean need_separator = FALSE;
	gboolean any_added = FALSE;
	guint ii, len;

	groups = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);

	len = e_ui_element_get_n_children (elem);
	for (ii = 0; ii < len; ii++) {
		EUIElement *subelem = e_ui_element_get_child (elem, ii);

		if (e_ui_element_get_kind (subelem) == E_UI_ELEMENT_KIND_START ||
		    e_ui_element_get_kind (subelem) == E_UI_ELEMENT_KIND_END) {
			eum_traverse_headerbar_rec (self, eheaderbar, gtkheaderbar, subelem,
				e_ui_element_get_kind (subelem) == E_UI_ELEMENT_KIND_START,
				groups, &need_separator, &any_added, is_customized);
		} else {
			g_warn_if_reached ();
		}
	}

	g_hash_table_destroy (groups);
}

static void
eum_headerbar_handle_changed_cb (EUIManager *self,
				 gpointer user_data)
{
	GtkWidget *widget = user_data;
	EHeaderBar *eheaderbar = NULL;
	GtkHeaderBar *gtkheaderbar = NULL;
	EUIElement *elem;
	const gchar *id;
	gboolean is_customized = FALSE;

	g_return_if_fail (E_IS_UI_MANAGER (self));

	if (E_IS_HEADER_BAR (widget)) {
		eheaderbar = E_HEADER_BAR (widget);
	} else if (GTK_IS_HEADER_BAR (widget)) {
		gtkheaderbar = GTK_HEADER_BAR (widget);
	} else {
		g_warning ("%s: Expected EHeaderBar or GtkHeaderBar, but received '%s' instead", G_STRFUNC, G_OBJECT_TYPE_NAME (widget));
		return;
	}

	if (eheaderbar) {
		e_header_bar_remove_all (eheaderbar);
	} else {
		GtkContainer *container = GTK_CONTAINER (gtkheaderbar);
		GList *children, *link;

		children = gtk_container_get_children (container);
		for (link = children; link; link = g_list_next (link)) {
			GtkWidget *child = link->data;
			gtk_container_remove (container, child);
		}

		g_list_free (children);
	}

	id = gtk_widget_get_name (widget);
	g_return_if_fail (id != NULL);
	g_return_if_fail (e_ui_parser_get_root (self->parser) != NULL);

	elem = eum_get_ui_element (self, id, &is_customized);
	if (!elem) {
		g_warning ("%s: Cannot find item with id '%s'", G_STRFUNC, id);
		return;
	}

	eum_traverse_headerbar (self, eheaderbar, gtkheaderbar, elem, is_customized);
}

static GObject *
e_ui_manager_create_item_for_headerbar (EUIManager *self,
					EUIElement *elem,
					gboolean is_customized)
{
	GtkWidget *widget;

	g_return_val_if_fail (elem != NULL, NULL);

	if (e_ui_element_headerbar_get_use_gtk_type (elem)) {
		GtkHeaderBar *gtkheaderbar;

		widget = gtk_header_bar_new ();
		gtkheaderbar = GTK_HEADER_BAR (widget);

		gtk_header_bar_set_show_close_button (gtkheaderbar, TRUE);

		eum_traverse_headerbar (self, NULL, gtkheaderbar, elem, is_customized);
	} else {
		EHeaderBar *eheaderbar;

		widget = e_header_bar_new ();
		eheaderbar = E_HEADER_BAR (widget);

		eum_traverse_headerbar (self, eheaderbar, NULL, elem, is_customized);
	}

	gtk_widget_set_name (widget, e_ui_element_get_id (elem));
	g_signal_connect_object (self, "changed",
		G_CALLBACK (eum_headerbar_handle_changed_cb), widget, 0);

	return G_OBJECT (widget);
}

static void
eum_traverse_toolbar_rec (EUIManager *self,
			  GtkToolbar *toolbar,
			  EUIElement *parent_elem,
			  GHashTable *groups,
			  gboolean *inout_need_separator,
			  gboolean *inout_any_added,
			  gboolean is_customized)
{
	guint ii, len;

	len = e_ui_element_get_n_children (parent_elem);

	for (ii = 0; ii < len; ii++) {
		EUIElement *elem = e_ui_element_get_child (parent_elem, ii);
		EUIAction *action;

		if (!elem)
			continue;

		switch (e_ui_element_get_kind (elem)) {
		case E_UI_ELEMENT_KIND_ITEM:
			action = e_ui_manager_get_action (self, e_ui_element_item_get_action (elem));
			if (action) {
				GObject *item;

				item = e_ui_manager_create_item_one (self, elem, action, E_UI_ELEMENT_KIND_TOOLBAR);
				if (item) {
					if (GTK_IS_TOOL_ITEM (item)) {
						if (*inout_any_added && *inout_need_separator) {
							GtkToolItem *separator;

							separator = gtk_separator_tool_item_new ();
							gtk_toolbar_insert (toolbar, separator, -1);
						}

						*inout_any_added = TRUE;
						*inout_need_separator = FALSE;

						gtk_toolbar_insert (toolbar, GTK_TOOL_ITEM (item), -1);
					} else {
						g_warning ("%s: Expected GtkToolItem, but received %s for action '%s.%s'", G_STRFUNC,
							G_OBJECT_TYPE_NAME (item), e_ui_action_get_map_name (action), e_ui_element_item_get_action (elem));
						g_object_ref_sink (item);
						g_clear_object (&item);
					}
				}
			} else if (!is_customized) {
				g_warning ("%s: Cannot find action '%s' for an item", G_STRFUNC, e_ui_element_item_get_action (elem));
			}
			break;
		case E_UI_ELEMENT_KIND_SEPARATOR:
			*inout_need_separator = *inout_any_added;
			break;
		case E_UI_ELEMENT_KIND_PLACEHOLDER:
			eum_traverse_toolbar_rec (self, toolbar, elem, groups, inout_need_separator, inout_any_added, is_customized);
			break;
		default:
			g_warn_if_reached ();
			break;
		}
	}
}

static void
eum_traverse_toolbar (EUIManager *self,
		      GtkToolbar *toolbar,
		      EUIElement *elem,
		      gboolean is_customized)
{
	GHashTable *groups; /* gpointer group ~> GSList ** */
	gboolean need_separator = FALSE;
	gboolean any_added = FALSE;

	groups = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);

	eum_traverse_toolbar_rec (self, toolbar, elem, groups, &need_separator, &any_added, is_customized);

	g_hash_table_destroy (groups);
}

static void
eum_toolbar_handle_changed_cb (EUIManager *self,
			       gpointer user_data)
{
	GtkContainer *toolbar = user_data;
	GList *children, *link;
	EUIElement *elem;
	const gchar *id;
	gboolean is_customized = FALSE;

	g_return_if_fail (E_IS_UI_MANAGER (self));
	g_return_if_fail (GTK_IS_TOOLBAR (toolbar));

	children = gtk_container_get_children (toolbar);
	for (link = children; link; link = g_list_next (link)) {
		gtk_container_remove (toolbar, link->data);
	}

	g_list_free (children);

	id = gtk_widget_get_name (GTK_WIDGET (toolbar));
	g_return_if_fail (id != NULL);
	g_return_if_fail (e_ui_parser_get_root (self->parser) != NULL);

	elem = eum_get_ui_element (self, id, &is_customized);
	if (!elem) {
		g_warning ("%s: Cannot find item with id '%s'", G_STRFUNC, id);
		return;
	}

	eum_traverse_toolbar (self, GTK_TOOLBAR (toolbar), elem, is_customized);
}

static GObject *
e_ui_manager_create_item_for_toolbar (EUIManager *self,
				      EUIElement *elem,
				      gboolean is_customized)
{
	GtkToolbar *toolbar;

	g_return_val_if_fail (elem != NULL, NULL);

	toolbar = GTK_TOOLBAR (gtk_toolbar_new ());
	gtk_widget_set_name (GTK_WIDGET (toolbar), e_ui_element_get_id (elem));

	if (e_ui_element_toolbar_get_primary (elem)) {
		gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (toolbar)),
			GTK_STYLE_CLASS_PRIMARY_TOOLBAR);
	}

	e_util_setup_toolbar_icon_size (toolbar, GTK_ICON_SIZE_BUTTON);

	g_signal_connect_object (self, "changed",
		G_CALLBACK (eum_toolbar_handle_changed_cb), toolbar, 0);

	eum_traverse_toolbar (self, toolbar, elem, is_customized);

	return G_OBJECT (toolbar);
}

static void
eum_traverse_menu (EUIManager *self,
		   EUIMenu *ui_menu,
		   EUIElement *parent_elem,
		   GMenu *parent_menu,
		   gboolean is_popup,
		   GMenu **inout_section,
		   gboolean is_customized)
{
	GMenu *section = *inout_section;
	guint ii, len;

	len = e_ui_element_get_n_children (parent_elem);

	for (ii = 0; ii < len; ii++) {
		EUIElement *elem = e_ui_element_get_child (parent_elem, ii);
		EUIAction *action;

		if (!elem)
			continue;

		switch (e_ui_element_get_kind (elem)) {
		case E_UI_ELEMENT_KIND_SUBMENU:
			action = e_ui_manager_get_action (self, e_ui_element_submenu_get_action (elem));
			if (action) {
				e_ui_menu_track_action (ui_menu, action);

				if (e_ui_action_is_visible (action) &&
				    (!is_popup || g_action_get_enabled (G_ACTION (action)))) {
					GMenu *submenu;
					GMenu *subsection = NULL;

					submenu = g_menu_new ();

					eum_traverse_menu (self, ui_menu, elem, submenu, is_popup, &subsection, is_customized);

					if (subsection && g_menu_model_get_n_items (G_MENU_MODEL (subsection)) > 0)
						g_menu_append_section (submenu, NULL, G_MENU_MODEL (subsection));

					g_clear_object (&subsection);

					if (g_menu_model_get_n_items (G_MENU_MODEL (submenu)) > 0) {
						GMenuItem *item;

						if (!section)
							section = g_menu_new ();

						item = g_menu_item_new_submenu (e_ui_action_get_label (action), G_MENU_MODEL (submenu));
						e_ui_manager_update_item_from_action (self, item, action);

						g_menu_append_item (section, item);

						g_clear_object (&item);
					}

					g_clear_object (&submenu);
				}
			} else if (!is_customized) {
				g_warning ("%s: Cannot find action '%s' for a submenu", G_STRFUNC, e_ui_element_submenu_get_action (elem));
			}
			break;
		case E_UI_ELEMENT_KIND_ITEM:
			action = e_ui_manager_get_action (self, e_ui_element_item_get_action (elem));
			if (action) {
				e_ui_menu_track_action (ui_menu, action);

				if (e_ui_action_is_visible (action) &&
				    (!is_popup || g_action_get_enabled (G_ACTION (action)))) {
					GObject *item;

					item = e_ui_manager_create_item_one (self, elem, action, E_UI_ELEMENT_KIND_MENU);
					if (item) {
						if (G_IS_MENU_ITEM (item)) {
							if (!section)
								section = g_menu_new ();
							g_menu_append_item (section, G_MENU_ITEM (item));
						} else {
							g_warning ("%s: Expected GMenuItem, but received %s for action '%s.%s'", G_STRFUNC,
								G_OBJECT_TYPE_NAME (item), e_ui_action_get_map_name (action), e_ui_element_item_get_action (elem));
						}
						g_clear_object (&item);
					}
				}
			} else if (!is_customized) {
				g_warning ("%s: Cannot find action '%s' for an item", G_STRFUNC, e_ui_element_item_get_action (elem));
			}
			break;
		case E_UI_ELEMENT_KIND_SEPARATOR:
			if (section && g_menu_model_get_n_items (G_MENU_MODEL (section)) > 0) {
				if (parent_menu)
					g_menu_append_section (parent_menu, NULL, G_MENU_MODEL (section));
				else
					e_ui_menu_append_section (ui_menu, G_MENU_MODEL (section));
			}

			g_clear_object (&section);
			break;
		case E_UI_ELEMENT_KIND_PLACEHOLDER:
			eum_traverse_menu (self, ui_menu, elem, parent_menu, is_popup, &section, is_customized);
			break;
		default:
			g_warn_if_reached ();
			break;
		}
	}

	*inout_section = section;
}

static GObject *
e_ui_manager_create_item_for_menu (EUIManager *self,
				   EUIElement *elem)
{
	EUIMenu *ui_menu;
	guint ii;

	g_return_val_if_fail (elem != NULL, NULL);

	ui_menu = e_ui_menu_new (self, e_ui_element_get_id (elem));

	/* to match the freeze count, because the ui_menu listens for both "freeze"
	   and "thaw" signals where it will receive only the "thaw" signal without
	   the corresponding "freeze" signal for this frozen count */
	for (ii = 0; ii < self->frozen; ii++) {
		e_ui_menu_freeze (ui_menu);
	}

	return G_OBJECT (ui_menu);
}

/**
 * e_ui_manager_create_item:
 * @self: an #EUIManager
 * @id: an identifier of the toplevel item
 *
 * Creates a new item corresponding to the identifier @id. This
 * is supposed to be part of the @self's #EUIParser on the toplevel,
 * thus either a menu, a headerbar or a toolbar. It's an error to ask
 * for an item which does not exist.
 *
 * The returned #GObject is an #EUIMenu (a #GMenuModel descendant) for the menu,
 * an #EHeaderBar for the headerbar and a #GtkToolbar for the toolbar.
 *
 * Returns: (transfer full): a new #GObject for the @id
 *
 * Since: 3.56
 **/
GObject *
e_ui_manager_create_item (EUIManager *self,
			  const gchar *id)
{
	EUIElement *elem;
	GObject *object = NULL;
	gboolean is_customized = FALSE;

	g_return_val_if_fail (E_IS_UI_MANAGER (self), NULL);
	g_return_val_if_fail (id != NULL, NULL);
	g_return_val_if_fail (e_ui_parser_get_root (self->parser) != NULL, NULL);

	elem = eum_get_ui_element (self, id, &is_customized);
	if (!elem) {
		g_warning ("%s: Cannot find item with id '%s'", G_STRFUNC, id);
		return NULL;
	}

	switch (e_ui_element_get_kind (elem)) {
	case E_UI_ELEMENT_KIND_HEADERBAR:
		object = e_ui_manager_create_item_for_headerbar (self, elem, is_customized);
		break;
	case E_UI_ELEMENT_KIND_TOOLBAR:
		object = e_ui_manager_create_item_for_toolbar (self, elem, is_customized);
		break;
	case E_UI_ELEMENT_KIND_MENU:
		object = e_ui_manager_create_item_for_menu (self, elem);
		break;
	default:
		g_warn_if_reached ();
		break;
	}

	if (GTK_IS_WIDGET (object))
		gtk_widget_set_visible (GTK_WIDGET (object), TRUE);

	return object;
}

/**
 * e_ui_manager_fill_menu:
 * @self: an #EUIManager
 * @id: a menu ID to fill the @ui_menu with
 * @ui_menu: an #EUIMenu to be filled
 *
 * Fills the @ui_menu with the menu definition with ID @id.
 * This is meant to be used by #EUIMenu itself during its rebuild.
 *
 * Since: 3.56
 **/
void
e_ui_manager_fill_menu (EUIManager *self,
			const gchar *id,
			EUIMenu *ui_menu)
{
	EUIElement *elem;
	GMenu *section = NULL;
	gboolean is_customized = FALSE;

	g_return_if_fail (E_IS_UI_MANAGER (self));
	g_return_if_fail (id != NULL);
	g_return_if_fail (E_IS_UI_MENU (ui_menu));
	g_return_if_fail (e_ui_parser_get_root (self->parser) != NULL);

	elem = eum_get_ui_element (self, id, &is_customized);
	if (!elem) {
		g_warning ("%s: Cannot find menu with id '%s'", G_STRFUNC, id);
		return;
	}

	if (e_ui_element_get_kind (elem) != E_UI_ELEMENT_KIND_MENU) {
		g_warning ("%s: Item with ID '%s' is not a menu, it's '%s' instead", G_STRFUNC,
			id, e_enum_to_string (E_TYPE_UI_ELEMENT_KIND, e_ui_element_get_kind (elem)));
		return;
	}

	eum_traverse_menu (self, ui_menu, elem, NULL, e_ui_element_menu_get_is_popup (elem), &section, is_customized);

	if (section && g_menu_model_get_n_items (G_MENU_MODEL (section)) > 0)
		e_ui_menu_append_section (ui_menu, G_MENU_MODEL (section));

	g_clear_object (&section);
}

static gboolean
e_ui_manager_transform_without_underscores_cb (GBinding *binding,
					       const GValue *from_value,
					       GValue *to_value,
					       gpointer user_data)
{
	const gchar *text = g_value_get_string (from_value);

	if (!text || !strchr (text, '_'))
		g_value_set_string (to_value, text);
	else
		g_value_take_string (to_value, e_str_without_underscores (text));

	return TRUE;
}

static void
e_ui_manager_create_named_binding (EUIManager *self,
				   gboolean strip_underscores,
				   const gchar *binding_name,
				   EUIAction *action,
				   const gchar *action_prop_name,
				   gpointer item,
				   const gchar *item_prop_name)
{
	GWeakRef *weakref;
	GBinding *binding;

	weakref = g_object_get_data (G_OBJECT (item), binding_name);
	binding = weakref ? g_weak_ref_get (weakref) : NULL;
	if (binding) {
		g_binding_unbind (binding);
		g_object_unref (binding);
	}

	if (strip_underscores) {
		binding = e_binding_bind_property_full (action, action_prop_name,
			item, item_prop_name,
			G_BINDING_SYNC_CREATE,
			e_ui_manager_transform_without_underscores_cb,
			NULL, NULL, NULL);
	} else {
		binding = e_binding_bind_property (action, action_prop_name,
			item, item_prop_name,
			G_BINDING_SYNC_CREATE);
	}

	g_object_set_data_full (G_OBJECT (item), binding_name, e_weak_ref_new (binding), (GDestroyNotify) e_weak_ref_free);
}

static void
e_ui_manager_synchro_menu_item_attribute (EUIManager *self,
					  EUIAction *action,
					  const gchar *prop_name,
					  GMenuItem *item)
{
	const gchar *value;

	if (g_strcmp0 (prop_name, "label") == 0) {
		value = e_ui_action_get_label (action);
		g_menu_item_set_label (item, value ? value : "");
	} else if (g_strcmp0 (prop_name, "accel") == 0) {
		gboolean has_customized = FALSE;

		value = NULL;
		if (self->customizer) {
			GPtrArray *custom_accels;

			custom_accels = e_ui_customizer_get_accels (self->customizer, g_action_get_name (G_ACTION (action)));
			if (custom_accels && custom_accels->len > 0)
				value = g_ptr_array_index (custom_accels, 0);
			has_customized = custom_accels != NULL;
		}
		if (!value && !has_customized)
			value = e_ui_action_get_accel (action);
		g_menu_item_set_attribute (item, "accel", (value && *value) ? "s" : NULL, value);
	} else {
		g_warning ("%s: Unhandled property '%s'", G_STRFUNC, prop_name);
	}
}

typedef struct _MenuItemData {
	GWeakRef manager_weakref; /* EUIManager * */
	GWeakRef item_weakref; /* GMenuItem * */
} MenuItemData;

static MenuItemData *
menu_item_data_new (EUIManager *self,
		    GMenuItem *item)
{
	MenuItemData *data;

	data = g_new0 (MenuItemData, 1);
	g_weak_ref_init (&data->manager_weakref, self);
	g_weak_ref_init (&data->item_weakref, item);

	return data;
}

static void
menu_item_data_free (gpointer ptr,
		     GClosure *unused)
{
	MenuItemData *data = ptr;

	if (data) {
		g_weak_ref_clear (&data->manager_weakref);
		g_weak_ref_clear (&data->item_weakref);
		g_free (data);
	}
}

static void
e_ui_manager_synchro_menu_item_attribute_cb (EUIAction *action,
					     GParamSpec *param,
					     gpointer user_data)
{
	MenuItemData *data = user_data;
	EUIManager *self;
	GMenuItem *item;

	g_return_if_fail (param != NULL);

	if (g_strcmp0 (param->name, "label") != 0 &&
	    g_strcmp0 (param->name, "accel") != 0)
		return;

	self = g_weak_ref_get (&data->manager_weakref);
	item = g_weak_ref_get (&data->item_weakref);

	if (self && item)
		e_ui_manager_synchro_menu_item_attribute (self, action, param->name, item);

	g_clear_object (&item);
	g_clear_object (&self);
}

/**
 * e_ui_manager_update_item_from_action:
 * @self: an #EUIManager
 * @item: an item object, its type varies based on the place of use
 * @action: an #EUIAction to update the @item with
 *
 * Updates properties of the @item with the values from the @action.
 * The function can handle only #GMenuItem, #GtkToolButton,
 * #GtkButton and #EHeaderBarButton descendants.
 *
 * Passing other than supported types into the function is
 * considered a programming error.
 *
 * Since: 3.56
 **/
void
e_ui_manager_update_item_from_action (EUIManager *self,
				      gpointer item,
				      EUIAction *action)
{
	GAction *gaction;
	GVariant *target;
	const GVariantType *param_type;
	const gchar *value;
	gchar *action_name_full;

	g_return_if_fail (E_IS_UI_MANAGER (self));
	g_return_if_fail (E_IS_UI_ACTION (action));

	if (!item)
		return;

	gaction = G_ACTION (action);
	value = g_action_get_name (gaction);
	param_type = g_action_get_parameter_type (gaction);
	target = e_ui_action_ref_target (action);

	action_name_full = g_strconcat (e_ui_action_get_map_name (action), ".", value, NULL);

	if (G_IS_MENU_ITEM (item)) {
		static gint with_icons = -1;

		/* this is remembered per application run, also because it's not
		   that easy to rebuild the menu once it's created */
		if (with_icons == -1) {
			GtkSettings *settings = gtk_settings_get_default ();
			gboolean gtk_menu_images = TRUE;
			g_object_get (settings, "gtk-menu-images", &gtk_menu_images, NULL);
			with_icons = gtk_menu_images ? 1 : 0;
		}

		g_menu_item_set_action_and_target_value (item, action_name_full, param_type && target ? target : NULL);

		g_signal_handlers_disconnect_matched (action, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
			G_CALLBACK (e_ui_manager_synchro_menu_item_attribute_cb), NULL);

		g_signal_connect_data (action, "notify",
			G_CALLBACK (e_ui_manager_synchro_menu_item_attribute_cb),
			menu_item_data_new (self, item), menu_item_data_free, 0);

		e_ui_manager_synchro_menu_item_attribute (self, action, "label", item);
		e_ui_manager_synchro_menu_item_attribute (self, action, "accel", item);

		if (with_icons) {
			value = e_ui_action_get_icon_name (action);
			if (value) {
				if (g_str_has_prefix (value, "gicon::")) {
					GIcon *icon;

					icon = e_ui_manager_get_gicon (self, value + 7);
					if (icon)
						g_menu_item_set_icon (item, icon);
				} else {
					g_menu_item_set_attribute (item, "icon", "s", value);
				}
			}
		}
	} else if (GTK_IS_TOOL_BUTTON (item)) {
		gtk_actionable_set_action_name (GTK_ACTIONABLE (item), action_name_full);
		if (param_type && target)
			gtk_actionable_set_action_target_value (GTK_ACTIONABLE (item), target);

		gtk_tool_button_set_use_underline (item, TRUE);

		value = e_ui_action_get_icon_name (action);
		if (value) {
			if (g_str_has_prefix (value, "gicon::")) {
				GIcon *icon;

				icon = e_ui_manager_get_gicon (self, value + 7);
				if (icon) {
					GtkWidget *image;

					image = gtk_image_new_from_gicon (icon, gtk_tool_item_get_icon_size (item));
					gtk_widget_set_visible (image, TRUE);
					gtk_tool_button_set_icon_widget (item, image);
				}
			} else {
				gtk_tool_button_set_icon_name (item, value);
			}
		}

		e_ui_manager_create_named_binding (self, FALSE, "EUIManager::binding:label",
			action, "label",
			item, "label");

		e_ui_manager_create_named_binding (self, FALSE, "EUIManager::binding:tooltip",
			action, "tooltip",
			item, "tooltip-text");
	} else if (GTK_IS_BUTTON (item)) {
		gboolean headerbar_parent;

		gtk_actionable_set_action_name (GTK_ACTIONABLE (item), action_name_full);
		if (param_type && target)
			gtk_actionable_set_action_target_value (GTK_ACTIONABLE (item), target);

		headerbar_parent = E_IS_HEADER_BAR_BUTTON (gtk_widget_get_parent (item));

		/* mnemonics on the header bar button can steal menu mnemonics */
		gtk_button_set_use_underline (item, !headerbar_parent);

		value = e_ui_action_get_icon_name (action);
		if (value) {
			GtkWidget *image = NULL;

			if (g_str_has_prefix (value, "gicon::")) {
				GIcon *icon;

				icon = e_ui_manager_get_gicon (self, value + 7);
				if (icon)
					image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_BUTTON);
			} else {
				image = gtk_image_new_from_icon_name (value, GTK_ICON_SIZE_BUTTON);
			}

			if (image) {
				gtk_button_set_image (item, image);
				gtk_widget_show (image);
			} else {
				gtk_button_set_image (item, NULL);
			}
		} else {
			gtk_button_set_image (item, NULL);
		}

		e_ui_manager_create_named_binding (self, headerbar_parent, "EUIManager::binding:label",
			action, "label",
			item, "label");

		e_ui_manager_create_named_binding (self, FALSE, "EUIManager::binding:tooltip",
			action, "tooltip",
			item, "tooltip-text");
	} else if (E_IS_HEADER_BAR_BUTTON (item)) {
	} else {
		g_warning ("%s: Do not know how to update item '%s'", G_STRFUNC,
			item ? G_OBJECT_TYPE_NAME (item) : "null");
	}

	if (GTK_IS_WIDGET (item)) {
		/* This is called for both labeled and icon-only buttons of the EHeaderBarButton
		   widget, but those two take care of the correct visibility on their own,
		   thus do not influence it here. */
		if (!eum_parent_is_header_bar_button (gtk_widget_get_parent (item))) {
			e_ui_manager_create_named_binding (self, FALSE, "EUIManager::binding:visible",
				action, "is-visible",
				item, "visible");
		}

		e_ui_manager_create_named_binding (self, FALSE, "EUIManager::binding:enabled",
			action, "enabled",
			item, "sensitive");
	}

	g_clear_pointer (&target, g_variant_unref);
	g_clear_pointer (&action_name_full, g_free);
}


/**
 * e_ui_manager_create_item_from_menu_model:
 * @self: an #EUIManager
 * @elem: (nullable): corresponding #EUIElement, or %NULL
 * @action: an #EUIAction
 * @for_kind: for which part of the UI the item should be created; it influences the type of the returned item
 * @menu_model: (transfer none): a #GMenuModel to use for the created item
 *
 * Creates a new UI item suitable for the @for_kind, containing the @menu_model.
 * For %E_UI_ELEMENT_KIND_MENU returns a submenu #GMenuItem;
 * for %E_UI_ELEMENT_KIND_TOOLBAR returns a #GtkMenuToolButton and
 * for %E_UI_ELEMENT_KIND_HEADERBAR returns an #EHeaderBarButton.
 *
 * This might be usually called from #EUIManager 's "create-item" callback.
 *
 * Returns: (transfer full): a new UI item
 *
 * Since: 3.56
 **/
GObject *
e_ui_manager_create_item_from_menu_model (EUIManager *self,
					  EUIElement *elem,
					  EUIAction *action,
					  EUIElementKind for_kind,
					  GMenuModel *menu_model)
{
	GObject *item = NULL;

	g_return_val_if_fail (E_IS_UI_MANAGER (self), NULL);
	g_return_val_if_fail (E_IS_UI_ACTION (action), NULL);
	g_return_val_if_fail (G_IS_MENU_MODEL (menu_model), NULL);

	if (for_kind == E_UI_ELEMENT_KIND_MENU) {
		item = G_OBJECT (g_menu_item_new_submenu (e_ui_action_get_label (action), menu_model));
	} else if (for_kind == E_UI_ELEMENT_KIND_TOOLBAR) {
		GtkToolItem *tool_item;
		GtkWidget *menu;

		menu = gtk_menu_new_from_model (menu_model);
		tool_item = gtk_menu_tool_button_new (NULL, e_ui_action_get_label (action));
		gtk_menu_tool_button_set_menu (GTK_MENU_TOOL_BUTTON (tool_item), menu);

		if (elem) {
			e_ui_manager_add_css_classes (self, GTK_WIDGET (tool_item), e_ui_element_item_get_css_classes (elem));
			gtk_tool_item_set_is_important (tool_item, e_ui_element_item_get_important (elem));
		} else {
			gtk_tool_item_set_is_important (tool_item, TRUE);
		}

		e_ui_manager_update_item_from_action (self, tool_item, action);
		e_ui_action_util_assign_to_widget (action, GTK_WIDGET (tool_item));

		item = G_OBJECT (tool_item);
	} else if (for_kind == E_UI_ELEMENT_KIND_HEADERBAR) {
		GtkWidget *widget, *menu;

		menu = gtk_menu_new_from_model (menu_model);

		widget = e_header_bar_button_new (e_ui_action_get_label (action), action, self);

		e_header_bar_button_take_menu (E_HEADER_BAR_BUTTON (widget), menu);

		e_binding_bind_property (
			action, "sensitive",
			widget, "sensitive",
			G_BINDING_SYNC_CREATE);

		e_binding_bind_property (
			action, "visible",
			widget, "visible",
			G_BINDING_SYNC_CREATE);

		item = G_OBJECT (widget);
	} else {
		g_warning ("%s: Unhandled element kind '%d' for action '%s'", G_STRFUNC, (gint) for_kind, g_action_get_name (G_ACTION (action)));
	}

	return item;
}

/**
 * e_ui_manager_set_actions_usable_for_kinds:
 * @self: an #EUIManager
 * @kinds: a bit-or of #EUIElementKind to set
 * @first_action_name: name of the first action
 * @...: (null-terminated): NULL-terminated list of additional action names
 *
 * Sets "usable-for-kinds" @kinds to one or more actions identified by their name.
 *
 * The @kinds can contain only %E_UI_ELEMENT_KIND_HEADERBAR,
 * %E_UI_ELEMENT_KIND_TOOLBAR and %E_UI_ELEMENT_KIND_MENU.
 *
 * See e_ui_manager_set_entries_usable_for_kinds(),
 *    e_ui_manager_set_enum_entries_usable_for_kinds()
 *
 * Since: 3.56
 **/
void
e_ui_manager_set_actions_usable_for_kinds (EUIManager *self,
					   guint32 kinds,
					   const gchar *first_action_name,
					   ...)
{
	va_list va;
	const gchar *action_name;

	g_return_if_fail (E_IS_UI_MANAGER (self));
	g_return_if_fail ((kinds & (~(E_UI_ELEMENT_KIND_HEADERBAR | E_UI_ELEMENT_KIND_TOOLBAR | E_UI_ELEMENT_KIND_MENU))) == 0);

	va_start (va, first_action_name);

	action_name = first_action_name;
	while (action_name) {
		EUIAction *action;

		action = e_ui_manager_get_action (self, action_name);
		if (action)
			e_ui_action_set_usable_for_kinds (action, kinds);
		else
			g_warning ("%s: Cannot find action '%s'", G_STRFUNC, action_name);

		action_name = va_arg (va, const gchar *);
	}

	va_end (va);
}

/**
 * e_ui_manager_set_entries_usable_for_kinds:
 * @self: an #EUIManager
 * @kinds: a bit-or of #EUIElementKind to set
 * @entries: action entries to be added, as #EUIActionEntry array
 * @n_entries: how many items @entries has, or -1 when NULL-terminated
 *
 * Sets "usable-for-kinds" @kinds to all of the actions in the @entries.
 *
 * The @kinds can contain only %E_UI_ELEMENT_KIND_HEADERBAR,
 * %E_UI_ELEMENT_KIND_TOOLBAR and %E_UI_ELEMENT_KIND_MENU.
 *
 * See e_ui_manager_set_enum_entries_usable_for_kinds(),
 *    e_ui_manager_set_actions_usable_for_kinds()
 *
 * Since: 3.56
 **/
void
e_ui_manager_set_entries_usable_for_kinds (EUIManager *self,
					   guint32 kinds,
					   const EUIActionEntry *entries,
					   gint n_entries)
{
	gint ii;

	g_return_if_fail (E_IS_UI_MANAGER (self));
	g_return_if_fail ((kinds & (~(E_UI_ELEMENT_KIND_HEADERBAR | E_UI_ELEMENT_KIND_TOOLBAR | E_UI_ELEMENT_KIND_MENU))) == 0);
	g_return_if_fail (entries != NULL);

	for (ii = 0; n_entries < 0 ? entries[ii].name != NULL : ii < n_entries; ii++) {
		const EUIActionEntry *entry = &(entries[ii]);
		EUIAction *action;

		action = e_ui_manager_get_action (self, entry->name);
		if (action)
			e_ui_action_set_usable_for_kinds (action, kinds);
		else
			g_warning ("%s: Cannot find action '%s'", G_STRFUNC, entry->name);
	}
}

/**
 * e_ui_manager_set_enum_entries_usable_for_kinds:
 * @self: an #EUIManager
 * @kinds: a bit-or of #EUIElementKind to set
 * @entries: action entries to be added, as #EUIActionEnumEntry array
 * @n_entries: how many items @entries has, or -1 when NULL-terminated
 *
 * Sets "usable-for-kinds" @kinds to all of the actions in the @entries.
 *
 * The @kinds can contain only %E_UI_ELEMENT_KIND_HEADERBAR,
 * %E_UI_ELEMENT_KIND_TOOLBAR and %E_UI_ELEMENT_KIND_MENU.
 *
 * See e_ui_manager_set_entries_usable_for_kinds(),
 *    e_ui_manager_set_actions_usable_for_kinds()
 *
 * Since: 3.56
 **/
void
e_ui_manager_set_enum_entries_usable_for_kinds (EUIManager *self,
						guint32 kinds,
						const EUIActionEnumEntry *entries,
						gint n_entries)
{
	gint ii;

	g_return_if_fail (E_IS_UI_MANAGER (self));
	g_return_if_fail ((kinds & (~(E_UI_ELEMENT_KIND_HEADERBAR | E_UI_ELEMENT_KIND_TOOLBAR | E_UI_ELEMENT_KIND_MENU))) == 0);
	g_return_if_fail (entries != NULL);

	for (ii = 0; n_entries < 0 ? entries[ii].name != NULL : ii < n_entries; ii++) {
		const EUIActionEnumEntry *entry = &(entries[ii]);
		EUIAction *action;

		action = e_ui_manager_get_action (self, entry->name);
		if (action)
			e_ui_action_set_usable_for_kinds (action, kinds);
		else
			g_warning ("%s: Cannot find action '%s'", G_STRFUNC, entry->name);
	}
}

/**
 * e_ui_manager_shortcut_def_hash:
 * @ptr: an #EUIManagerShortcutDef
 *
 * Returns a hash value of the #EUIManagerShortcutDef.
 *
 * Returns: a hash value of the #EUIManagerShortcutDef
 *
 * Since: 3.56
 **/
guint
e_ui_manager_shortcut_def_hash (gconstpointer ptr)
{
	const EUIManagerShortcutDef *sd = ptr;

	if (!ptr)
		return 0;

	return g_int_hash (&sd->key) + g_int_hash (&sd->mods);
}

/**
 * e_ui_manager_shortcut_def_equal:
 * @ptr1: an #EUIManagerShortcutDef
 * @ptr2: an #EUIManagerShortcutDef
 *
 * Returns whether the two #EUIManagerShortcutDef equal.
 *
 * Returns: whether the two #EUIManagerShortcutDef equal
 *
 * Since: 3.56
 **/
gboolean
e_ui_manager_shortcut_def_equal (gconstpointer ptr1,
				 gconstpointer ptr2)
{
	const EUIManagerShortcutDef *sd1 = ptr1;
	const EUIManagerShortcutDef *sd2 = ptr2;

	if (sd1 == sd2)
		return TRUE;

	if (!sd1 || !sd2)
		return FALSE;

	return sd1->key == sd2->key && sd1->mods == sd2->mods;
}
