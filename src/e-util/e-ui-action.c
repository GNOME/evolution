/*
 * SPDX-FileCopyrightText: (C) 2024 Red Hat (www.redhat.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <libedataserver/libedataserver.h>

#include "e-headerbar-button.h"
#include "e-ui-action-group.h"

#include "e-ui-action.h"

/**
 * SECTION: e-ui-action
 * @include: e-util/e-util.h
 * @short_description: a UI action
 *
 * The #EUIAction is similar to a #GSimpleAction, with added properties
 * suitable for easier manipulation of the action appearance. It implements
 * a #GAction interface.
 *
 * The action can be part of a single #EUIActionGroup only. The GAction::enabled
 * property considers also the groups sensitive value.
 *
 * While the @EUIAction:visible property is relevant for the action itself,
 * the @EUIAction:is-visible property considers also visibility of the group, if any.
 *
 * The object is not thread-safe, it's meant to be used only from
 * the main/GUI thread.
 *
 * Since: 3.56
 **/

/**
 * EUIActionFunc:
 * @action: an #EUIAction
 * @value: (nullable): a #GVariant value for the function
 * @user_data: user data for the function
 *
 * A function called either on @action activate or change_state signal.
 *
 * For the activate, the @value is a parameter, with which the activate
 * was called. Stateless actions do not provide any parameter.
 *
 * For the change_state, the @value is a new value the state should be changed to.
 *
 * Since: 3.56
 **/

/**
 * EUIActionEntry:
 * @name: an action name
 * @icon_name: an icon name
 * @label: an action label
 * @accel: an action accelerator
 * @tooltip: an action tooltip
 * @activate: (nullable): an #EUIActionFunc callback to call when the action is activated, or %NULL
 * @parameter_type: an action parameter type
 * @state: an action state for stateful actions
 * @change_state: (nullable): an #EUIActionFunc callback to call when the state of the action should be changed, or %NULL
 *
 * A structure describing an action, which can be created in bulk
 * with e_ui_manager_add_actions() function.
 *
 * Simple actions declare only the @activate callback, where it reacts
 * to the action activation.
 *
 * To create a checkbox-like widgets from the action, set the @state
 * to a boolean value (like "true") and provide a @change_state
 * function, which will set e_ui_action_set_state (action, value);
 * and also react to the change of the state.
 *
 * To create a radiobutton-like widgets from the action, set the @parameter_type
 * for example to string ("s"), the @state to the value the radio group
 * should be switched to (like "'value1'") and the @change_state
 * function, which calls e_ui_action_set_state (action, value); and reacts
 * to the change of the state. Each action in the radio group can use the same
 * @change_state callback. Make sure the actions are part of the same radio
 * group with the e_ui_action_set_radio_group().
 *
 * See #EUIActionEnumEntry
 *
 * Since: 3.56
 **/

/**
 * EUIActionEnumEntry:
 * @name: an action name
 * @icon_name: an icon name
 * @label: an action label
 * @accel: an action accelerator
 * @tooltip: an action tooltip
 * @activate: (nullable): an #EUIActionFunc callback to call when the action is activated, or %NULL
 * @state: the corresponding enum value, as gint
 *
 * A structure describing a radio action, whose states are enum values.
 * The actions can be created in bulk with e_ui_manager_add_actions_enum() function.
 *
 * See #EUIActionEntry.
 *
 * Since: 3.56
 **/

/* This is a copy of the GSimpleAction, with added "visible" property */

struct _EUIAction {
	GObject parent;

	gchar *map_name;
	gchar *name;
	gchar *icon_name;
	gchar *label;
	gchar *accel;
	gchar *tooltip;
	GVariantType *parameter_type;
	GVariant *target; /* the first state set; needed for radio groups */
	GVariant *state;
	GVariant *state_hint;
	GPtrArray *secondary_accels;
	GPtrArray *radio_group;
	EUIActionGroup *action_group;
	gboolean sensitive;
	gboolean visible;
	guint32 usable_for_kinds;
};

static void e_ui_action_action_iface_init (GActionInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EUIAction, e_ui_action, G_TYPE_OBJECT,
	G_IMPLEMENT_INTERFACE (G_TYPE_ACTION, e_ui_action_action_iface_init))

enum {
	PROP_0,
	PROP_MAP_NAME,
	PROP_NAME,
	PROP_ICON_NAME,
	PROP_LABEL,
	PROP_ACCEL,
	PROP_TOOLTIP,
	PROP_PARAMETER_TYPE,
	PROP_ENABLED,
	PROP_STATE_TYPE,
	PROP_STATE,
	PROP_STATE_HINT,
	PROP_VISIBLE,
	PROP_SENSITIVE,
	PROP_IS_VISIBLE,
	PROP_ACTIVE,
	N_PROPS
};

enum {
	SIGNAL_CHANGE_STATE,
	SIGNAL_ACTIVATE,
	SIGNAL_CHANGED,
	SIGNAL_ACCEL_ADDED,
	SIGNAL_ACCEL_REMOVED,
	N_SIGNALS
};

static GParamSpec *properties[N_PROPS] = { NULL, };
static guint signals[N_SIGNALS] = { 0, };

static const gchar *
e_ui_action_get_name (GAction *action)
{
	g_return_val_if_fail (E_IS_UI_ACTION (action), NULL);

	return E_UI_ACTION (action)->name;
}

static const GVariantType *
e_ui_action_get_parameter_type (GAction *action)
{
	g_return_val_if_fail (E_IS_UI_ACTION (action), NULL);

	return E_UI_ACTION (action)->parameter_type;
}

static GVariant *
e_ui_action_get_state (GAction *action)
{
	EUIAction *self;

	g_return_val_if_fail (E_IS_UI_ACTION (action), NULL);

	self = E_UI_ACTION (action);

	return self->state ? g_variant_ref (self->state) : NULL;
}

static const GVariantType *
e_ui_action_get_state_type (GAction *action)
{
	EUIAction *self;

	g_return_val_if_fail (E_IS_UI_ACTION (action), NULL);

	self = E_UI_ACTION (action);

	return self->state ? g_variant_get_type (self->state) : NULL;
}

static GVariant *
e_ui_action_get_state_hint (GAction *action)
{
	EUIAction *self;

	g_return_val_if_fail (E_IS_UI_ACTION (action), NULL);

	self = E_UI_ACTION (action);

	return self->state_hint ? g_variant_ref (self->state_hint) : NULL;
}

static gboolean
e_ui_action_get_enabled (GAction *action)
{
	EUIAction *self;

	g_return_val_if_fail (E_IS_UI_ACTION (action), FALSE);

	self = E_UI_ACTION (action);

	return self->sensitive && (!self->action_group || e_ui_action_group_get_sensitive (self->action_group));
}

static void
e_ui_action_change_state (GAction *action,
			  GVariant *value)
{
	EUIAction *self = E_UI_ACTION (action);

	/* If the user connected a signal handler then they are responsible
	 * for handling state changes.
	 */
	if (g_signal_has_handler_pending (self, signals[SIGNAL_CHANGE_STATE], 0, TRUE))
		g_signal_emit (self, signals[SIGNAL_CHANGE_STATE], 0, value);

	/* If not, then the default behaviour is to just set the state. */
	else
		e_ui_action_set_state (self, value);
}

static void
e_ui_action_activate (GAction *action,
		      GVariant *parameter)
{
	EUIAction *self = E_UI_ACTION (action);

	g_return_if_fail (self->parameter_type == NULL ? parameter == NULL :
		(parameter != NULL && g_variant_is_of_type (parameter, self->parameter_type)));

	if (g_action_get_enabled (action) && e_ui_action_is_visible (self)) {
		if (parameter != NULL)
			g_variant_ref_sink (parameter);

		/* If the user connected a signal handler then they are responsible
		 * for handling activation.
		 */
		if (g_signal_has_handler_pending (self, signals[SIGNAL_ACTIVATE], 0, TRUE)) {
			g_signal_emit (self, signals[SIGNAL_ACTIVATE], 0, parameter);

		/* If not, do some reasonable defaults for stateful actions. */
		} else if (self->state) {
			/* If we have no parameter and this is a boolean action, toggle. */
			if (parameter == NULL && g_variant_is_of_type (self->state, G_VARIANT_TYPE_BOOLEAN)) {
				gboolean was_enabled = g_variant_get_boolean (self->state);
				e_ui_action_change_state (action, g_variant_new_boolean (!was_enabled));
			/* else, if the parameter and state type are the same, do a change-state */
			} else if (g_variant_is_of_type (self->state, g_variant_get_type (parameter))) {
				e_ui_action_change_state (action, parameter);
			}
		}

		if (parameter != NULL)
			g_variant_unref (parameter);
	}
}

static void
e_ui_action_set_property (GObject *object,
			  guint prop_id,
			  const GValue *value,
			  GParamSpec *pspec)
{
	EUIAction *self = E_UI_ACTION (object);

	switch (prop_id) {
	case PROP_MAP_NAME:
		g_free (self->map_name);
		self->map_name = g_value_dup_string (value);
		break;
	case PROP_NAME:
		g_free (self->name);
		self->name = g_value_dup_string (value);
		break;
	case PROP_ICON_NAME:
		e_ui_action_set_icon_name (self, g_value_get_string (value));
		break;
	case PROP_LABEL:
		e_ui_action_set_label (self, g_value_get_string (value));
		break;
	case PROP_ACCEL:
		e_ui_action_set_accel (self, g_value_get_string (value));
		break;
	case PROP_TOOLTIP:
		e_ui_action_set_tooltip (self, g_value_get_string (value));
		break;
	case PROP_PARAMETER_TYPE:
		g_clear_pointer (&self->parameter_type, g_variant_type_free);
		self->parameter_type = g_value_dup_boxed (value);
		break;
	case PROP_ENABLED:
		e_ui_action_set_sensitive (self, g_value_get_boolean (value));
		break;
	case PROP_STATE:
		e_ui_action_set_state (self, g_value_get_variant (value));
		break;
	case PROP_STATE_HINT:
		e_ui_action_set_state_hint (self, g_value_get_variant (value));
		break;
	case PROP_VISIBLE:
		e_ui_action_set_visible (self, g_value_get_boolean (value));
		break;
	case PROP_SENSITIVE:
		e_ui_action_set_sensitive (self, g_value_get_boolean (value));
		break;
	case PROP_ACTIVE:
		e_ui_action_set_active (self, g_value_get_boolean (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
e_ui_action_get_property (GObject *object,
			  guint prop_id,
			  GValue *value,
			  GParamSpec *pspec)
{
	GAction *action = G_ACTION (object);

	switch (prop_id) {
	case PROP_MAP_NAME:
		g_value_set_string (value, e_ui_action_get_map_name (E_UI_ACTION (action)));
		break;
	case PROP_NAME:
		g_value_set_string (value, e_ui_action_get_name (action));
		break;
	case PROP_ICON_NAME:
		g_value_set_string (value, e_ui_action_get_icon_name (E_UI_ACTION (action)));
		break;
	case PROP_LABEL:
		g_value_set_string (value, e_ui_action_get_label (E_UI_ACTION (action)));
		break;
	case PROP_ACCEL:
		g_value_set_string (value, e_ui_action_get_accel (E_UI_ACTION (action)));
		break;
	case PROP_TOOLTIP:
		g_value_set_string (value, e_ui_action_get_tooltip (E_UI_ACTION (action)));
		break;
	case PROP_PARAMETER_TYPE:
		g_value_set_boxed (value, e_ui_action_get_parameter_type (action));
		break;
	case PROP_ENABLED:
		g_value_set_boolean (value, e_ui_action_get_enabled (action));
		break;
	case PROP_STATE_TYPE:
		g_value_set_boxed (value, e_ui_action_get_state_type (action));
		break;
	case PROP_STATE:
		g_value_take_variant (value, e_ui_action_get_state (action));
		break;
	case PROP_STATE_HINT:
		g_value_take_variant (value, e_ui_action_get_state_hint (action));
		break;
	case PROP_VISIBLE:
		g_value_set_boolean (value, e_ui_action_get_visible (E_UI_ACTION (action)));
		break;
	case PROP_SENSITIVE:
		g_value_set_boolean (value, e_ui_action_get_sensitive (E_UI_ACTION (action)));
		break;
	case PROP_ACTIVE:
		g_value_set_boolean (value, e_ui_action_get_active (E_UI_ACTION (action)));
		break;
	case PROP_IS_VISIBLE:
		g_value_set_boolean (value, e_ui_action_is_visible (E_UI_ACTION (action)));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
e_ui_action_finalize (GObject *object)
{
	EUIAction *self = E_UI_ACTION (object);

	e_ui_action_set_radio_group (self, NULL);
	e_ui_action_set_action_group (self, NULL);

	g_clear_pointer (&self->map_name, g_free);
	g_clear_pointer (&self->name, g_free);
	g_clear_pointer (&self->icon_name, g_free);
	g_clear_pointer (&self->label, g_free);
	g_clear_pointer (&self->accel, g_free);
	g_clear_pointer (&self->tooltip, g_free);
	g_clear_pointer (&self->secondary_accels, g_ptr_array_unref);
	g_clear_pointer (&self->parameter_type, g_variant_type_free);
	g_clear_pointer (&self->target, g_variant_unref);
	g_clear_pointer (&self->state, g_variant_unref);
	g_clear_pointer (&self->state_hint, g_variant_unref);

	G_OBJECT_CLASS (e_ui_action_parent_class)->finalize (object);
}

static void
e_ui_action_action_iface_init (GActionInterface *iface)
{
	iface->get_name = e_ui_action_get_name;
	iface->get_parameter_type = e_ui_action_get_parameter_type;
	iface->get_state_type = e_ui_action_get_state_type;
	iface->get_state_hint = e_ui_action_get_state_hint;
	iface->get_enabled = e_ui_action_get_enabled;
	iface->get_state = e_ui_action_get_state;
	iface->change_state = e_ui_action_change_state;
	iface->activate = e_ui_action_activate;
}

static void
e_ui_action_class_init (EUIActionClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->set_property = e_ui_action_set_property;
	object_class->get_property = e_ui_action_get_property;
	object_class->finalize = e_ui_action_finalize;

	/**
	 * EUIAction:map-name:
	 *
	 * A #GActionMap name the action is declared at. It usually matches #EUIActionGroup
	 * name, when one is set on the action.
	 *
	 * Since: 3.56
	 **/
	properties[PROP_MAP_NAME] = g_param_spec_string ("map-name", NULL, NULL, NULL,
		G_PARAM_READWRITE |
		G_PARAM_CONSTRUCT_ONLY |
		G_PARAM_STATIC_STRINGS |
		G_PARAM_EXPLICIT_NOTIFY);

	/**
	 * EUIAction:name:
	 *
	 * An action name.
	 *
	 * Since: 3.56
	 **/
	properties[PROP_NAME] = g_param_spec_string ("name", NULL, NULL, NULL,
		G_PARAM_READWRITE |
		G_PARAM_CONSTRUCT_ONLY |
		G_PARAM_STATIC_STRINGS |
		G_PARAM_EXPLICIT_NOTIFY);

	/**
	 * EUIAction:icon-name:
	 *
	 * An icon name, which can be a standard icon name, or a text with prefix "gicon::",
	 * which are generated by the action owner through #EUIManager::create-gicon signal.
	 * The signal is executed without the "gicon::" prefix.
	 *
	 * Since: 3.56
	 **/
	properties[PROP_ICON_NAME] = g_param_spec_string ("icon-name", NULL, NULL, NULL,
		G_PARAM_READWRITE |
		G_PARAM_STATIC_STRINGS |
		G_PARAM_EXPLICIT_NOTIFY);

	/**
	 * EUIAction:label:
	 *
	 * An action label.
	 *
	 * Since: 3.56
	 **/
	properties[PROP_LABEL] = g_param_spec_string ("label", NULL, NULL, NULL,
		G_PARAM_READWRITE |
		G_PARAM_STATIC_STRINGS |
		G_PARAM_EXPLICIT_NOTIFY);

	/**
	 * EUIAction:accel:
	 *
	 * An action accelerator.
	 *
	 * Since: 3.56
	 **/
	properties[PROP_ACCEL] = g_param_spec_string ("accel", NULL, NULL, NULL,
		G_PARAM_READWRITE |
		G_PARAM_STATIC_STRINGS |
		G_PARAM_EXPLICIT_NOTIFY);

	/**
	 * EUIAction:tooltip:
	 *
	 * An action tooltip text (not markup).
	 *
	 * Since: 3.56
	 **/
	properties[PROP_TOOLTIP] = g_param_spec_string ("tooltip", NULL, NULL, NULL,
		G_PARAM_READWRITE |
		G_PARAM_STATIC_STRINGS |
		G_PARAM_EXPLICIT_NOTIFY);

	/**
	 * EUIAction:parameter-type:
	 *
	 * A parameter type of a stateful action, as a #GVariantType, or %NULL.
	 *
	 * Since: 3.56
	 **/
	properties[PROP_PARAMETER_TYPE] = g_param_spec_boxed ("parameter-type", NULL, NULL, G_TYPE_VARIANT_TYPE,
		G_PARAM_READWRITE |
		G_PARAM_CONSTRUCT_ONLY |
		G_PARAM_STATIC_STRINGS |
		G_PARAM_EXPLICIT_NOTIFY);

	/**
	 * EUIAction:enabled:
	 *
	 * An enabled state of the action. When the action is part of an action group,
	 * the @EUIActionGroup:sensitive is consulted together with the @EUIAction:sensitive
	 * property when determining whether the action is enabled.
	 *
	 * Since: 3.56
	 **/
	properties[PROP_ENABLED] = g_param_spec_boolean ("enabled", NULL, NULL, TRUE,
		G_PARAM_READWRITE |
		G_PARAM_STATIC_STRINGS |
		G_PARAM_EXPLICIT_NOTIFY);

	/**
	 * EUIAction:state-type:
	 *
	 * State type of a stateful action, as a #GVariantType, or %NULL.
	 *
	 * Since: 3.56
	 **/
	properties[PROP_STATE_TYPE] = g_param_spec_boxed ("state-type", NULL, NULL, G_TYPE_VARIANT_TYPE,
		G_PARAM_READABLE |
		G_PARAM_STATIC_STRINGS |
		G_PARAM_EXPLICIT_NOTIFY);

	/**
	 * EUIAction:state:
	 *
	 * A state of a stateful action, as a #GVariant, or %NULL.
	 *
	 * Since: 3.56
	 **/
	properties[PROP_STATE] = g_param_spec_variant ("state", NULL, NULL, G_VARIANT_TYPE_ANY, NULL,
		G_PARAM_READWRITE |
		G_PARAM_STATIC_STRINGS |
		G_PARAM_EXPLICIT_NOTIFY);

	/**
	 * EUIAction:state-hint:
	 *
	 * A state hint of a stateful action, as a #GVariantType, or %NULL.
	 *
	 * Since: 3.56
	 **/
	properties[PROP_STATE_HINT] = g_param_spec_boxed ("state-hint", NULL, NULL, G_TYPE_VARIANT_TYPE,
		G_PARAM_READWRITE |
		G_PARAM_STATIC_STRINGS |
		G_PARAM_EXPLICIT_NOTIFY);

	/**
	 * EUIAction:visible:
	 *
	 * Whether the action is visible. It does not consult associated
	 * #EUIActionGroup visibility.
	 *
	 * Since: 3.56
	 **/
	properties[PROP_VISIBLE] = g_param_spec_boolean ("visible", NULL, NULL, TRUE,
		G_PARAM_READWRITE |
		G_PARAM_STATIC_STRINGS |
		G_PARAM_EXPLICIT_NOTIFY);

	/**
	 * EUIAction:is-visible:
	 *
	 * Read-only property to determine whether both @EUIAction:visible property and
	 * possibly associated #EUIActionGroup 's visible property are %TRUE.
	 *
	 * Since: 3.56
	 **/
	properties[PROP_IS_VISIBLE] = g_param_spec_boolean ("is-visible", NULL, NULL, TRUE,
		G_PARAM_READABLE |
		G_PARAM_STATIC_STRINGS |
		G_PARAM_EXPLICIT_NOTIFY);

	/**
	 * EUIAction:sensitive:
	 *
	 * Action's sensitive state. It does not consult associated
	 * #EUIActionGroup sensitivity.
	 *
	 * Since: 3.56
	 **/
	properties[PROP_SENSITIVE] = g_param_spec_boolean ("sensitive", NULL, NULL, TRUE,
		G_PARAM_READWRITE |
		G_PARAM_STATIC_STRINGS |
		G_PARAM_EXPLICIT_NOTIFY);

	/**
	 * EUIAction:active:
	 *
	 * Action's active state. It is relevant only for stateful actions, like toggle
	 * actions (with a boolean state) or radio actions (with state and target).
	 * See e_ui_action_get_active() and e_ui_action_set_active() for more information.
	 *
	 * Since: 3.56
	 **/
	properties[PROP_ACTIVE] = g_param_spec_boolean ("active", NULL, NULL, TRUE,
		G_PARAM_READWRITE |
		G_PARAM_STATIC_STRINGS |
		G_PARAM_EXPLICIT_NOTIFY);

	g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);

	/**
	 * EUIAction::activate:
	 * @self: an #EUIAction
	 * @parameter: (nullable): an activation parameter as a #GVariant, or %NULL
	 *
	 * A signal emitted when the action should be activated.
	 * The @parameter is provided only for stateful actions.
	 *
	 * Since: 3.56
	 **/
	signals[SIGNAL_ACTIVATE] = g_signal_new ("activate",
		E_TYPE_UI_ACTION,
		G_SIGNAL_RUN_LAST | G_SIGNAL_MUST_COLLECT,
		0, NULL, NULL,
		NULL,
		G_TYPE_NONE, 1,
		G_TYPE_VARIANT);

	/**
	 * EUIAction::change-state:
	 * @self: an #EUIAction
	 * @new_state: a new state
	 *
	 * A signal emitted to set a new state to a stateful action.
	 *
	 * Since: 3.56
	 **/
	signals[SIGNAL_CHANGE_STATE] = g_signal_new ("change-state",
		E_TYPE_UI_ACTION,
		G_SIGNAL_RUN_LAST | G_SIGNAL_MUST_COLLECT,
		0, NULL, NULL,
		NULL,
		G_TYPE_NONE, 1,
		G_TYPE_VARIANT);

	/**
	 * EUIAction::changed:
	 * @self: an #EUIAction
	 *
	 * A signal emitted when the action changes its content.
	 * It's meant to be called when the menu content corresponding
	 * to this action should be regenerated, similar to when
	 * the #EUIAction:is-visible property changes.
	 *
	 * Since: 3.56
	 **/
	signals[SIGNAL_CHANGED] = g_signal_new ("changed",
		E_TYPE_UI_ACTION,
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		0, NULL, NULL,
		NULL,
		G_TYPE_NONE, 0,
		G_TYPE_NONE);

	/**
	 * EUIAction::accel-added:
	 * @self: an #EUIAction
	 * @accel: the added accelerator string
	 *
	 * A signal emitted when the action has either set the primary
	 * accelerator or it has added a secondary accelerator.
	 *
	 * Since: 3.56
	 **/
	signals[SIGNAL_ACCEL_ADDED] = g_signal_new ("accel-added",
		E_TYPE_UI_ACTION,
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		0, NULL, NULL,
		NULL,
		G_TYPE_NONE, 1,
		G_TYPE_STRING);

	/**
	 * EUIAction::accel-removed:
	 * @self: an #EUIAction
	 * @accel: the removed accelerator string
	 *
	 * A signal emitted when the action has either unset the primary
	 * accelerator or it has removed a secondary accelerator.
	 * The @accel is the removed accelerator string.
	 *
	 * Since: 3.56
	 **/
	signals[SIGNAL_ACCEL_REMOVED] = g_signal_new ("accel-removed",
		E_TYPE_UI_ACTION,
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		0, NULL, NULL,
		NULL,
		G_TYPE_NONE, 1,
		G_TYPE_STRING);
}

static void
e_ui_action_init (EUIAction *self)
{
	self->sensitive = TRUE;
	self->visible = TRUE;
	self->usable_for_kinds = E_UI_ELEMENT_KIND_HEADERBAR | E_UI_ELEMENT_KIND_TOOLBAR | E_UI_ELEMENT_KIND_MENU;
}

/**
 * e_ui_action_new:
 * @map_name: a name of a #GActionMap the action belongs to
 * @action_name: a name of the action to be created
 * @parameter_type: (nullable): a type of the action parameter, as #GVariantType, or %NULL
 *
 * Creates a new #EUIAction.
 *
 * Returns: (transfer full): a new #EUIAction
 *
 * Since: 3.56
 **/
EUIAction *
e_ui_action_new (const gchar *map_name,
		 const gchar *action_name,
		 const GVariantType *parameter_type)
{
	g_return_val_if_fail (map_name != NULL, NULL);
	g_return_val_if_fail (action_name != NULL, NULL);

	return g_object_new (E_TYPE_UI_ACTION,
		"map-name", map_name,
		"name", action_name,
		"parameter-type", parameter_type,
		NULL);
}

/**
 * e_ui_action_new_stateful:
 * @map_name: a name of a #GActionMap the action belongs to
 * @action_name: a name of the action to be created
 * @parameter_type: (nullable): a type of the action parameter, as #GVariantType, or %NULL
 * @state: a #GVariant for the action to have set a state to
 *
 * Creates a new stateful #EUIAction. Actions with boolean state are
 * usually presented as toggle buttons and menu items, while actions
 * with string states are usually presented as radio buttons and menu
 * items. Radio actions need to have set radio group with
 * e_ui_action_set_radio_group().
 *
 * Returns: (transfer full): a new stateful #EUIAction
 *
 * Since: 3.56
 **/
EUIAction *
e_ui_action_new_stateful (const gchar *map_name,
			  const gchar *action_name,
			  const GVariantType *parameter_type,
			  GVariant *state)
{
	g_return_val_if_fail (map_name != NULL, NULL);
	g_return_val_if_fail (action_name != NULL, NULL);

	return g_object_new (E_TYPE_UI_ACTION,
		"map-name", map_name,
		"name", action_name,
		"parameter-type", parameter_type,
		"state", state,
		NULL);
}

/**
 * e_ui_action_new_from_entry:
 * @map_name: a name of a #GActionMap the action belongs to
 * @entry: a single #EUIActionEntry to construct the action from
 * @translation_domain: (nullable): a translation domain to use
 *    to localize the entry members for label and tooltip
 *
 * Creates a new #EUIAction from the @entry.
 *
 * Returns: (transfer full): a new #EUIAction
 *
 * Since: 3.56
 **/
EUIAction *
e_ui_action_new_from_entry (const gchar *map_name,
			    const EUIActionEntry *entry,
			    const gchar *translation_domain)
{
	const GVariantType *parameter_type;
	EUIAction *action;

	g_return_val_if_fail (map_name != NULL, NULL);
	g_return_val_if_fail (entry != NULL, NULL);

	if (entry->parameter_type) {
		if (!g_variant_type_string_is_valid (entry->parameter_type)) {
			g_critical ("%s: the type string '%s' given as the parameter type for "
				"action '%s' is not a valid GVariant type string.  This action will not be added.",
				G_STRFUNC, entry->parameter_type, entry->name);
			return NULL;
		}

		parameter_type = G_VARIANT_TYPE (entry->parameter_type);
	} else {
		parameter_type = NULL;
	}

	if (entry->state) {
		GError *error = NULL;
		GVariant *state;

		state = g_variant_parse (NULL, entry->state, NULL, NULL, &error);
		if (state == NULL) {
			g_critical ("%s: GVariant could not parse the state value given for action '%s' "
				"('%s'): %s.  This action will not be added.",
				G_STRFUNC, entry->name, entry->state, error->message);
			g_clear_error (&error);
			return NULL;
		}

		action = e_ui_action_new_stateful (map_name, entry->name, parameter_type, state);

		g_clear_pointer (&state, g_variant_unref);
	} else {
		action = e_ui_action_new (map_name, entry->name, parameter_type);
	}

	if (action) {
		const gchar *domain = translation_domain;

		if (!domain || !*domain)
			domain = GETTEXT_PACKAGE;

		e_ui_action_set_icon_name (action, entry->icon_name);
		e_ui_action_set_label (action, entry->label && *entry->label ? g_dgettext (domain, entry->label) : NULL);
		e_ui_action_set_accel (action, entry->accel);
		e_ui_action_set_tooltip (action, entry->tooltip && *entry->tooltip ? g_dgettext (domain, entry->tooltip) : NULL);
	}

	return action;
}

/**
 * e_ui_action_new_from_enum_entry:
 * @map_name: a name of a #GActionMap the action belongs to
 * @entry: a single #EUIActionEnumEntry to construct the action from
 * @translation_domain: (nullable): a translation domain to use
 *    to localize the entry members for label and tooltip
 *
 * Creates a new #EUIAction from the @entry.
 *
 * Returns: (transfer full): a new #EUIAction
 *
 * Since: 3.56
 **/
EUIAction *
e_ui_action_new_from_enum_entry (const gchar *map_name,
				 const EUIActionEnumEntry *entry,
				 const gchar *translation_domain)
{
	EUIAction *action;

	g_return_val_if_fail (map_name != NULL, NULL);
	g_return_val_if_fail (entry != NULL, NULL);

	action = e_ui_action_new_stateful (map_name, entry->name, G_VARIANT_TYPE_INT32, g_variant_new_int32 (entry->state));

	if (action) {
		const gchar *domain = translation_domain;

		if (!domain || !*domain)
			domain = GETTEXT_PACKAGE;

		e_ui_action_set_icon_name (action, entry->icon_name);
		e_ui_action_set_label (action, entry->label && *entry->label ? g_dgettext (domain, entry->label) : NULL);
		e_ui_action_set_accel (action, entry->accel);
		e_ui_action_set_tooltip (action, entry->tooltip && *entry->tooltip ? g_dgettext (domain, entry->tooltip) : NULL);
	}

	return action;
}

/**
 * e_ui_action_get_map_name:
 * @self: an #EUIAction
 *
 * Get name of the action map. It usually matches a name of
 * an associated #EUIActionGroup.
 *
 * Returns: action map name of the @self
 *
 * Since: 3.56
 **/
const gchar *
e_ui_action_get_map_name (EUIAction *self)
{
	g_return_val_if_fail (E_IS_UI_ACTION (self), NULL);

	return self->map_name;
}

/**
 * e_ui_action_ref_target:
 * @self: an #EUIAction
 *
 * References target of a stateful action. This is usable for radio
 * actions, where the state contains the current state of the whole
 * radio group, while the target is the value to be set in the group
 * when the action is activated.
 *
 * Returns: (transfer full) (nullable): the target value of the @self,
 *    or %NULL, when the @self is not a radio action
 *
 * Since: 3.56
 **/
GVariant *
e_ui_action_ref_target (EUIAction *self)
{
	g_return_val_if_fail (E_IS_UI_ACTION (self), NULL);

	return self->target ? g_variant_ref (self->target) : NULL;
}

static void
e_ui_action_set_state_without_radio_group (EUIAction *self,
					   GVariant *value)
{
	g_return_if_fail (E_IS_UI_ACTION (self));
	g_return_if_fail (value != NULL);

	if (!self->state || !g_variant_equal (self->state, value)) {
		if (self->state)
			g_variant_unref (self->state);

		self->state = g_variant_ref_sink (value);

		if (!self->target && !g_variant_is_of_type (self->state, G_VARIANT_TYPE_BOOLEAN))
			self->target = g_variant_ref_sink (value);

		g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_STATE]);
		g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACTIVE]);
	}
}

/**
 * e_ui_action_set_state:
 * @self: an #EUIAction
 * @value: (transfer full): a state to set, as a #GVariant
 *
 * Sets a state of the @self to @value.
 *
 * Since: 3.56
 **/
void
e_ui_action_set_state (EUIAction *self,
		       GVariant *value)
{
	guint ii;

	g_return_if_fail (E_IS_UI_ACTION (self));
	g_return_if_fail (value != NULL);

	g_variant_ref_sink (value);

	if (self->radio_group) {
		/* to have the whole group in the consistent state, because
		   any listener to notify::state may not get correct result
		   when checking whether the action is active or not */
		for (ii = 0; ii < self->radio_group->len; ii++) {
			EUIAction *item = g_ptr_array_index (self->radio_group, ii);
			g_object_freeze_notify (G_OBJECT (item));
		}
	}

	e_ui_action_set_state_without_radio_group (self, value);

	if (self->radio_group) {
		for (ii = 0; ii < self->radio_group->len; ii++) {
			EUIAction *item = g_ptr_array_index (self->radio_group, ii);

			if (item != self)
				e_ui_action_set_state_without_radio_group (item, value);
		}

		for (ii = 0; ii < self->radio_group->len; ii++) {
			EUIAction *item = g_ptr_array_index (self->radio_group, ii);
			g_object_thaw_notify (G_OBJECT (item));
		}
	}

	g_variant_unref (value);
}

/**
 * e_ui_action_set_state_hint:
 * @self: an #EUIAction
 * @state_hint: (nullable) (transfer full): a state hint as a #GVariant, or %NULL
 *
 * Sets or unsets the action state hint.
 *
 * Since: 3.56
 **/
void
e_ui_action_set_state_hint (EUIAction *self,
			    GVariant *state_hint)
{
	g_return_if_fail (E_IS_UI_ACTION (self));

	if (self->state_hint == state_hint)
		return;

	if (state_hint)
		g_variant_ref_sink (state_hint);

	g_clear_pointer (&self->state_hint, g_variant_unref);
	self->state_hint = state_hint;

	g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_STATE_HINT]);
}

/**
 * e_ui_action_set_visible:
 * @self: an #EUIAction
 * @visible: a value to set
 *
 * Sets whether the @self is visible.
 *
 * Since: 3.56
 **/
void
e_ui_action_set_visible (EUIAction *self,
			 gboolean visible)
{
	GObject *object;

	g_return_if_fail (E_IS_UI_ACTION (self));

	if ((!self->visible) == (!visible))
		return;

	self->visible = visible;

	object = G_OBJECT (self);
	g_object_freeze_notify (object);
	g_object_notify_by_pspec (object, properties[PROP_VISIBLE]);
	g_object_notify_by_pspec (object, properties[PROP_IS_VISIBLE]);
	g_object_thaw_notify (object);
}

/**
 * e_ui_action_get_visible:
 * @self: an #EUIAction
 *
 * Gets whether the action is visible. It does not consider the associated
 * group visibility; use e_ui_action_is_visible() instead.
 *
 * Returns: whether the action itself is visible
 *
 * Since: 3.56
 **/
gboolean
e_ui_action_get_visible (EUIAction *self)
{
	g_return_val_if_fail (E_IS_UI_ACTION (self), FALSE);

	return self->visible;
}

/**
 * e_ui_action_set_sensitive:
 * @self: an #EUIAction
 * @sensitive: a value to set
 *
 * Sets whether the action is sensitive.
 *
 * Since: 3.56
 **/
void
e_ui_action_set_sensitive (EUIAction *self,
			   gboolean sensitive)
{
	GObject *object;

	g_return_if_fail (E_IS_UI_ACTION (self));

	if ((!self->sensitive) == (!sensitive))
		return;

	self->sensitive = sensitive;

	object = G_OBJECT (self);
	g_object_freeze_notify (object);
	g_object_notify_by_pspec (object, properties[PROP_SENSITIVE]);
	g_object_notify_by_pspec (object, properties[PROP_ENABLED]);
	g_object_thaw_notify (object);
}

/**
 * e_ui_action_get_sensitive:
 * @self: an #EUIAction
 *
 * Gets whether the action is sensitive. It does not consider the associated
 * group sensitivity; use g_action_get_enabled() instead.
 *
 * Returns: whether the action itself is sensitive
 *
 * Since: 3.56
 **/
gboolean
e_ui_action_get_sensitive (EUIAction *self)
{
	g_return_val_if_fail (E_IS_UI_ACTION (self), FALSE);

	return self->sensitive;
}

/**
 * e_ui_action_is_visible:
 * @self: an #EUIAction
 *
 * Gets whether the action itself and the associated #EUIActionGroup,
 * if any, are both visible. Use e_ui_action_get_visible() to get
 * visibility of the action itself.
 *
 * Returns: whether the @self with the associated action group are visible
 *
 * Since: 3.56
 **/
gboolean
e_ui_action_is_visible (EUIAction *self)
{
	g_return_val_if_fail (E_IS_UI_ACTION (self), FALSE);

	return self->visible && (!self->action_group || e_ui_action_group_get_visible (self->action_group));
}

/**
 * e_ui_action_set_icon_name:
 * @self: an #EUIAction
 * @icon_name: (nullable): an icon name, or %NULL
 *
 * Sets or unsets and icon name for the @self. It can be a standard icon name,
 * or a text with prefix "gicon::", which are generated by the action owner
 * through #EUIManager::create-gicon signal. The signal is executed without
 * the "gicon::" prefix.
 *
 * Since: 3.56
 **/
void
e_ui_action_set_icon_name (EUIAction *self,
			   const gchar *icon_name)
{
	g_return_if_fail (E_IS_UI_ACTION (self));

	if (e_util_strcmp0 (self->icon_name, icon_name) == 0)
		return;

	g_free (self->icon_name);
	self->icon_name = g_strdup (icon_name);

	g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ICON_NAME]);
}

/**
 * e_ui_action_get_icon_name:
 * @self: an #EUIAction
 *
 * Gets the icon name of the @self.
 *
 * Returns: (nullable): an icon name for the @self, or %NULL, when none is set
 *
 * Since: 3.56
 **/
const gchar *
e_ui_action_get_icon_name (EUIAction *self)
{
	g_return_val_if_fail (E_IS_UI_ACTION (self), NULL);

	return self->icon_name;
}

/**
 * e_ui_action_set_label:
 * @self: an #EUIAction
 * @label: (nullable): a label to set, or %NULL
 *
 * Sets a label for the action. The label should be already localized.
 *
 * Since: 3.56
 **/
void
e_ui_action_set_label (EUIAction *self,
		       const gchar *label)
{
	g_return_if_fail (E_IS_UI_ACTION (self));

	if (e_util_strcmp0 (self->label, label) == 0)
		return;

	g_free (self->label);
	self->label = g_strdup (label);

	g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LABEL]);
}

/**
 * e_ui_action_get_label:
 * @self: an #EUIAction
 *
 * Gets a label of the @self.
 *
 * Returns: (nullable): a label of the @self, or %NULL, when none is set
 *
 * Since: 3.56
 **/
const gchar *
e_ui_action_get_label (EUIAction *self)
{
	g_return_val_if_fail (E_IS_UI_ACTION (self), NULL);

	return self->label;
}

/**
 * e_ui_action_set_accel:
 * @self: an #EUIAction
 * @accel: (nullable): an accelerator string, or %NULL
 *
 * Sets or unsets an accelerator string for the @self.
 * The @accel should be parseable by the gtk_accelerator_parse()
 * function.
 *
 * Since: 3.56
 **/
void
e_ui_action_set_accel (EUIAction *self,
		       const gchar *accel)
{
	g_return_if_fail (E_IS_UI_ACTION (self));

	if (e_util_strcmp0 (self->accel, accel) == 0)
		return;

	if (self->accel)
		g_signal_emit (self, signals[SIGNAL_ACCEL_REMOVED], 0, self->accel, NULL);

	g_free (self->accel);
	self->accel = g_strdup (accel);

	if (self->accel)
		g_signal_emit (self, signals[SIGNAL_ACCEL_ADDED], 0, self->accel, NULL);

	g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACCEL]);
}

/**
 * e_ui_action_get_accel:
 * @self: an #EUIAction
 *
 * Gets an accelarator string for the @self.
 *
 * Returns: (nullable): an accelerator string for the @self, or %NULL, when none is set
 *
 * Since: 3.56
 **/
const gchar *
e_ui_action_get_accel (EUIAction *self)
{
	g_return_val_if_fail (E_IS_UI_ACTION (self), NULL);

	return self->accel;
}

/**
 * e_ui_action_add_secondary_accel:
 * @self: an #EUIAction
 * @accel: (not nullable): an accelerator string
 *
 * Adds a secondary accelerator string for the @self. This is in addition
 * to the "accel" property, which is meant to be the main accelerator,
 * shown in the GUI and such.
 *
 * The @accel should be parseable by the gtk_accelerator_parse()
 * function.
 *
 * Since: 3.56
 **/
void
e_ui_action_add_secondary_accel (EUIAction *self,
				 const gchar *accel)
{
	g_return_if_fail (E_IS_UI_ACTION (self));
	g_return_if_fail (accel != NULL);

	if (!self->secondary_accels) {
		self->secondary_accels = g_ptr_array_new_with_free_func (g_free);
	} else {
		guint ii;

		for (ii = 0; ii < self->secondary_accels->len; ii++) {
			const gchar *existing = g_ptr_array_index (self->secondary_accels, ii);
			if (e_util_strcmp0 (existing, accel) == 0)
				return;
		}
	}

	g_ptr_array_add (self->secondary_accels, g_strdup (accel));

	g_signal_emit (self, signals[SIGNAL_ACCEL_ADDED], 0, accel, NULL);
}

/**
 * e_ui_action_get_secondary_accels:
 * @self: an #EUIAction
 *
 * Gets an array of the secondary accelarators for the @self. The caller should
 * not modify the array in any way.
 *
 * Returns: (nullable) (transfer none) (element-type utf8): an array
 *    of the secondary accelerator strings for the @self, or %NULL, when none is set
 *
 * Since: 3.56
 **/
GPtrArray *
e_ui_action_get_secondary_accels (EUIAction *self)
{
	g_return_val_if_fail (E_IS_UI_ACTION (self), NULL);

	return self->secondary_accels;
}

/**
 * e_ui_action_remove_secondary_accels:
 * @self: an #EUIAction
 *
 * Removes all secondary accelerator strings.
 *
 * Since: 3.56
 **/
void
e_ui_action_remove_secondary_accels (EUIAction *self)
{
	g_return_if_fail (E_IS_UI_ACTION (self));

	if (self->secondary_accels) {
		guint ii;

		for (ii = 0; ii < self->secondary_accels->len; ii++) {
			const gchar *accel = g_ptr_array_index (self->secondary_accels, ii);

			g_signal_emit (self, signals[SIGNAL_ACCEL_REMOVED], 0, accel, NULL);
		}
	}

	g_clear_pointer (&self->secondary_accels, g_ptr_array_unref);
}

/**
 * e_ui_action_set_tooltip:
 * @self: an #EUIAction
 * @tooltip: (nullable): a tooltip text to set, or %NULL
 *
 * Sets or unsets a tooltip text (not markup) for the @self.
 *
 * Since: 3.56
 **/
void
e_ui_action_set_tooltip (EUIAction *self,
			 const gchar *tooltip)
{
	g_return_if_fail (E_IS_UI_ACTION (self));

	if (e_util_strcmp0 (self->tooltip, tooltip) == 0)
		return;

	g_free (self->tooltip);
	self->tooltip = g_strdup (tooltip);

	g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TOOLTIP]);
}

/**
 * e_ui_action_get_tooltip:
 * @self: an #EUIAction
 *
 * Gets tooltip text for the @self.
 *
 * Returns: (nullable): a tooltip text (not markup) for the @self, or %NULL, when none is set.
 *
 * Since: 3.56
 **/
const gchar *
e_ui_action_get_tooltip (EUIAction *self)
{
	g_return_val_if_fail (E_IS_UI_ACTION (self), NULL);

	return self->tooltip;
}

/**
 * e_ui_action_set_radio_group:
 * @self: an #EUIAction
 * @radio_group: (nullable) (transfer none) (element-type EUIAction): a radio group to use, or %NULL
 *
 * Stateful actions can be grouped together, by setting the same radio
 * group to each action. When not %NULL, the @self references the @radio_group,
 * thus the caller can just create one array, set it to each action
 * and then unref it.
 *
 * An action can be in a single radio group only. Trying to set a different
 * radio group when one is already set is considered a programming error.
 *
 * Since: 3.56
 **/
void
e_ui_action_set_radio_group (EUIAction *self,
			     GPtrArray *radio_group)
{
	g_return_if_fail (E_IS_UI_ACTION (self));

	if (!radio_group) {
		if (self->radio_group) {
			g_ptr_array_remove (self->radio_group, self);
			g_clear_pointer (&self->radio_group, g_ptr_array_unref);
		}
		return;
	}

	if (self->radio_group && self->radio_group != radio_group) {
		g_warning ("%s: Action '%s' is already in another radio group", G_STRFUNC, self->name);
		return;
	}

	if (self->radio_group == radio_group)
		return;

	g_return_if_fail (self->radio_group == NULL);

	self->radio_group = g_ptr_array_ref (radio_group);
	g_ptr_array_add (self->radio_group, self); /* do not ref the 'self', due to circular dependency */
}

/**
 * e_ui_action_get_radio_group:
 * @self: an #EUIAction
 *
 * Gets the radio group the @self is part of. The array is owned by the @self,
 * it cannot be modified neither freed by the caller.
 *
 * Returns: (nullable) (transfer none) (element-type EUIAction): a radio group
 *    the @self is part of, or %NULL
 *
 * Since: 3.56
 **/
GPtrArray *
e_ui_action_get_radio_group (EUIAction *self)
{
	g_return_val_if_fail (E_IS_UI_ACTION (self), NULL);

	return self->radio_group;
}

/**
 * e_ui_action_set_action_group:
 * @self: an #EUIAction
 * @action_group: (nullable): an #EUIActionGroup to set, or %NULL
 *
 * Sets the @self as being part of the @action_group, or unsets the group,
 * when it's %NULL. An action cannot be part of multiple action groups,
 * thus when trying to set a different action group the action is removed
 * from the previous group (unlike e_ui_action_set_radio_group(), which
 * does not move an action between radio groups automatically).
 *
 * The @action_group usually matches the @EUIAction:map-name property.
 *
 * See e_ui_action_get_sensitive(), e_ui_action_get_visible(),
 *   e_ui_action_is_visible().
 *
 * Since: 3.56
 **/
void
e_ui_action_set_action_group (EUIAction *self,
			      struct _EUIActionGroup *action_group)
{
	GObject *object;
	gboolean old_enabled, old_is_visible;

	g_return_if_fail (E_IS_UI_ACTION (self));

	if (self->action_group == action_group)
		return;

	old_enabled = e_ui_action_get_enabled (G_ACTION (self));
	old_is_visible = e_ui_action_is_visible (self);

	if (self->action_group) {
		EUIActionGroup *old_action_group = self->action_group;
		/* to avoid recursion */
		self->action_group = NULL;
		e_ui_action_group_remove (old_action_group, self);
	}

	if (action_group) {
		self->action_group = action_group;
		e_ui_action_group_add (action_group, self);
	}

	object = G_OBJECT (self);
	g_object_freeze_notify (object);

	if ((!old_enabled) != (!e_ui_action_get_enabled (G_ACTION (self))))
		g_object_notify_by_pspec (object, properties[PROP_ENABLED]);

	if ((!old_is_visible) != (!e_ui_action_is_visible (self)))
		g_object_notify_by_pspec (object, properties[PROP_IS_VISIBLE]);

	g_object_thaw_notify (object);
}

/**
 * e_ui_action_get_action_group:
 * @self: an #EUIAction
 *
 * Gets an action group the @self is part of.
 *
 * Returns: (transfer none) (nullable): an #EUIActionGroup the @self is part of,
 *    or %NULL, when none is set
 *
 * Since: 3.56
 **/
struct _EUIActionGroup *
e_ui_action_get_action_group (EUIAction *self)
{
	g_return_val_if_fail (E_IS_UI_ACTION (self), NULL);

	return self->action_group;
}

/**
 * e_ui_action_get_active:
 * @self: an #EUIAction
 *
 * Checks whether the @self state is the active, aka current, value.
 *
 * For toggle actions (with boolean state) returns whether the state is %TRUE.
 * For radio actions returns whether the state matches the action target.
 * For stateless actions returns FALSE.
 *
 * Returns: whether the @self is active
 *
 * Since: 3.56
 **/
gboolean
e_ui_action_get_active (EUIAction *self)
{
	g_return_val_if_fail (E_IS_UI_ACTION (self), FALSE);

	if (!self->state)
		return FALSE;

	if (self->target)
		return g_variant_equal (self->state, self->target);

	return g_variant_is_of_type (self->state, G_VARIANT_TYPE_BOOLEAN) &&
		g_variant_get_boolean (self->state);
}

/**
 * e_ui_action_set_active:
 * @self: an #EUIAction
 * @active: value to set
 *
 * Sets whether the @self state is the @active, aka current, value.
 *
 * For toggle actions (with boolean state) sets the @active directly.
 * For radio actions sets the state to the @self target when @active is %TRUE,
 * otherwise does nothing. For stateless actions does nothing.
 *
 * Since: 3.56
 **/
void
e_ui_action_set_active (EUIAction *self,
			gboolean active)
{
	g_return_if_fail (E_IS_UI_ACTION (self));

	if (!self->state)
		return;

	if (g_variant_is_of_type (self->state, G_VARIANT_TYPE_BOOLEAN)) {
		e_ui_action_set_state (self, g_variant_new_boolean (active));
		return;
	}

	if (self->target && active)
		e_ui_action_set_state (self, self->target);
}

/**
 * e_ui_action_emit_changed:
 * @self: an #EUIAction
 *
 * Emits the #EUIAction::changed signal on the @self.
 *
 * Since: 3.56
 **/
void
e_ui_action_emit_changed (EUIAction *self)
{
	g_return_if_fail (E_IS_UI_ACTION (self));

	g_signal_emit (self, signals[SIGNAL_CHANGED], 0, NULL);
}

/**
 * e_ui_action_get_usable_for_kinds:
 * @self: an #EUIAction
 *
 * Returns bit-or of #EUIElementKind, for which the action can be used.
 * Only the %E_UI_ELEMENT_KIND_HEADERBAR, %E_UI_ELEMENT_KIND_TOOLBAR
 * and %E_UI_ELEMENT_KIND_MENU are considered.
 *
 * By default, the @self can be used for all these kinds.
 *
 * Returns: bit-or of #EUIElementKind
 *
 * Since: 3.56
 **/
guint32
e_ui_action_get_usable_for_kinds (EUIAction *self)
{
	g_return_val_if_fail (E_IS_UI_ACTION (self), 0);

	return self->usable_for_kinds;
}

/**
 * e_ui_action_set_usable_for_kinds:
 * @self: an #EUIAction
 * @kinds: a bit-or of #EUIElementKind
 *
 * Sets which #EUIElementKind -s the @self can be used for.
 * Only the %E_UI_ELEMENT_KIND_HEADERBAR, %E_UI_ELEMENT_KIND_TOOLBAR
 * and %E_UI_ELEMENT_KIND_MENU are considered.
 *
 * By default, the @self can be used for all these kinds.
 *
 * Since: 3.56
 **/
void
e_ui_action_set_usable_for_kinds (EUIAction *self,
				  guint32 kinds)
{
	g_return_if_fail (E_IS_UI_ACTION (self));

	self->usable_for_kinds = kinds;
}

/**
 * e_ui_action_util_gvalue_to_enum_state:
 * @binding: a #GBinding
 * @from_value: a source #GValue
 * @to_value: a destination #GValue
 * @user_data: unused binding user data
 *
 * A utility function, which can be used as a transformation function
 * of a #GBinding from an enum property to an enum state of an #EUIAction
 * created by e_ui_action_new_from_enum_entry().
 *
 * Returns: %TRUE
 *
 * Since: 3.56
 **/
gboolean
e_ui_action_util_gvalue_to_enum_state (GBinding *binding,
				       const GValue *from_value,
				       GValue *to_value,
				       gpointer user_data)
{
	gint value;

	if (G_VALUE_HOLDS_ENUM (from_value))
		value = g_value_get_enum (from_value);
	else
		value = g_value_get_int (from_value);

	g_value_set_variant (to_value, g_variant_new_int32 (value));

	return TRUE;
}

/**
 * e_ui_action_util_enum_state_to_gvalue:
 * @binding: a #GBinding
 * @from_value: a source #GValue
 * @to_value: a destination #GValue
 * @user_data: unused binding user data
 *
 * A utility function, which can be used as a transformation function
 * of a #GBinding from an enum state of an #EUIAction created by
 * e_ui_action_new_from_enum_entry() to an enum property.
 *
 * Returns: %TRUE
 *
 * Since: 3.56
 **/
gboolean
e_ui_action_util_enum_state_to_gvalue (GBinding *binding,
				       const GValue *from_value,
				       GValue *to_value,
				       gpointer user_data)
{
	GVariant *value = g_value_get_variant (from_value);

	if (G_VALUE_HOLDS_ENUM (to_value))
		g_value_set_enum (to_value, value ? g_variant_get_int32 (value) : -1);
	else
		g_value_set_int (to_value, value ? g_variant_get_int32 (value) : -1);

	return TRUE;
}

/**
 * e_ui_action_util_assign_to_widget:
 * @action: an #EUIAction
 * @widget: a #GtkWidget
 *
 * Assigns @action to the @widget, syncing visible, sensitive and tooltip properties,
 * together with the actionable properties.
 *
 * Since: 3.56
 **/
void
e_ui_action_util_assign_to_widget (EUIAction *action,
				   GtkWidget *widget)
{
	gchar full_action_name[128];
	gint would_copy_bytes;
	GtkActionable *actionable;
	GVariant *target;

	g_return_if_fail (E_IS_UI_ACTION (action));
	g_return_if_fail (GTK_IS_WIDGET (widget));

	would_copy_bytes = g_snprintf (full_action_name, sizeof (full_action_name), "%s.%s",
		e_ui_action_get_map_name (action),
		g_action_get_name (G_ACTION (action)));
	g_warn_if_fail (would_copy_bytes < sizeof (full_action_name) - 1);

	actionable = GTK_ACTIONABLE (widget);
	target = e_ui_action_ref_target (action);

	gtk_actionable_set_action_target_value (actionable, target);
	gtk_actionable_set_action_name (actionable, full_action_name);

	g_clear_pointer (&target, g_variant_unref);

	e_binding_bind_property (action, "visible",
		widget, "visible",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (action, "sensitive",
		widget, "sensitive",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (action, "tooltip",
		widget, "tooltip-text",
		G_BINDING_SYNC_CREATE);
}
