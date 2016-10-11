/*
 * e-popup-action.c
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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "e-popup-action.h"

#include <glib/gi18n.h>

#define E_POPUP_ACTION_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_POPUP_ACTION, EPopupActionPrivate))

enum {
	PROP_0,
	PROP_RELATED_ACTION,
	PROP_USE_ACTION_APPEARANCE
};

struct _EPopupActionPrivate {
	GtkAction *related_action;
	gboolean use_action_appearance;
	gulong activate_handler_id;
	gulong notify_handler_id;
};

/* Forward Declarations */
static void e_popup_action_activatable_init (GtkActivatableIface *iface);

G_DEFINE_TYPE_WITH_CODE (
	EPopupAction,
	e_popup_action,
	GTK_TYPE_ACTION,
	G_IMPLEMENT_INTERFACE (
		GTK_TYPE_ACTIVATABLE,
		e_popup_action_activatable_init))

static void
popup_action_notify_cb (GtkAction *action,
                        GParamSpec *pspec,
                        GtkActivatable *activatable)
{
	GtkActivatableIface *iface;

	iface = GTK_ACTIVATABLE_GET_IFACE (activatable);
	g_return_if_fail (iface->update != NULL);

	iface->update (activatable, action, pspec->name);
}

static GtkAction *
popup_action_get_related_action (EPopupAction *popup_action)
{
	return popup_action->priv->related_action;
}

static void
popup_action_set_related_action (EPopupAction *popup_action,
                                 GtkAction *related_action)
{
	GtkActivatable *activatable;

	/* Do not call gtk_activatable_do_set_related_action() because
	 * it assumes the activatable object is a widget and tries to add
	 * it to the related actions's proxy list.  Instead we'll just do
	 * the relevant steps manually. */

	activatable = GTK_ACTIVATABLE (popup_action);

	if (related_action == popup_action->priv->related_action)
		return;

	if (related_action != NULL)
		g_object_ref (related_action);

	if (popup_action->priv->related_action != NULL) {
		g_signal_handler_disconnect (
			popup_action,
			popup_action->priv->activate_handler_id);
		g_signal_handler_disconnect (
			popup_action->priv->related_action,
			popup_action->priv->notify_handler_id);
		popup_action->priv->activate_handler_id = 0;
		popup_action->priv->notify_handler_id = 0;
		g_object_unref (popup_action->priv->related_action);
	}

	popup_action->priv->related_action = related_action;

	if (related_action != NULL) {
		popup_action->priv->activate_handler_id =
			g_signal_connect_swapped (
				popup_action, "activate",
				G_CALLBACK (gtk_action_activate),
				related_action);
		popup_action->priv->notify_handler_id =
			g_signal_connect (
				related_action, "notify",
				G_CALLBACK (popup_action_notify_cb),
				popup_action);
		gtk_activatable_sync_action_properties (
			activatable, related_action);
	} else
		gtk_action_set_visible (GTK_ACTION (popup_action), FALSE);

	g_object_notify (G_OBJECT (popup_action), "related-action");
}

static gboolean
popup_action_get_use_action_appearance (EPopupAction *popup_action)
{
	return popup_action->priv->use_action_appearance;
}

static void
popup_action_set_use_action_appearance (EPopupAction *popup_action,
                                        gboolean use_action_appearance)
{
	GtkActivatable *activatable;
	GtkAction *related_action;

	if (popup_action->priv->use_action_appearance == use_action_appearance)
		return;

	popup_action->priv->use_action_appearance = use_action_appearance;

	g_object_notify (G_OBJECT (popup_action), "use-action-appearance");

	activatable = GTK_ACTIVATABLE (popup_action);
	related_action = popup_action_get_related_action (popup_action);
	gtk_activatable_sync_action_properties (activatable, related_action);
}

static void
popup_action_set_property (GObject *object,
                           guint property_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_RELATED_ACTION:
			popup_action_set_related_action (
				E_POPUP_ACTION (object),
				g_value_get_object (value));
			return;

		case PROP_USE_ACTION_APPEARANCE:
			popup_action_set_use_action_appearance (
				E_POPUP_ACTION (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
popup_action_get_property (GObject *object,
                           guint property_id,
                           GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_RELATED_ACTION:
			g_value_set_object (
				value,
				popup_action_get_related_action (
				E_POPUP_ACTION (object)));
			return;

		case PROP_USE_ACTION_APPEARANCE:
			g_value_set_boolean (
				value,
				popup_action_get_use_action_appearance (
				E_POPUP_ACTION (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
popup_action_dispose (GObject *object)
{
	EPopupActionPrivate *priv;

	priv = E_POPUP_ACTION_GET_PRIVATE (object);

	if (priv->related_action != NULL) {
		g_signal_handler_disconnect (
			object,
			priv->activate_handler_id);
		g_signal_handler_disconnect (
			priv->related_action,
			priv->notify_handler_id);
		g_object_unref (priv->related_action);
		priv->related_action = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_popup_action_parent_class)->dispose (object);
}

static void
popup_action_update (GtkActivatable *activatable,
                     GtkAction *action,
                     const gchar *property_name)
{
	GObjectClass *class;
	GParamSpec *pspec;
	GValue *value;

	/* Ignore "action-group" changes" */
	if (strcmp (property_name, "action-group") == 0)
		return;

	/* Ignore "visible" changes. */
	if (strcmp (property_name, "visible") == 0)
		return;

	value = g_slice_new0 (GValue);
	class = G_OBJECT_GET_CLASS (action);
	pspec = g_object_class_find_property (class, property_name);
	g_value_init (value, pspec->value_type);

	g_object_get_property (G_OBJECT (action), property_name, value);

	if (strcmp (property_name, "sensitive") == 0)
		property_name = "visible";
	else if (!gtk_activatable_get_use_action_appearance (activatable))
		goto exit;

	g_object_set_property (G_OBJECT (activatable), property_name, value);

exit:
	g_value_unset (value);
	g_slice_free (GValue, value);
}

static void
popup_action_sync_action_properties (GtkActivatable *activatable,
                                     GtkAction *action)
{
	if (action == NULL)
		return;

	/* XXX GTK+ 2.18 is still missing accessor functions for
	 *     "hide-if-empty" and "visible-overflown" properties.
	 *     These are rarely used so we'll skip them for now. */

	/* A popup action is never shown as insensitive. */
	gtk_action_set_sensitive (GTK_ACTION (activatable), TRUE);

	gtk_action_set_visible (
		GTK_ACTION (activatable),
		gtk_action_get_sensitive (action));

	gtk_action_set_visible_horizontal (
		GTK_ACTION (activatable),
		gtk_action_get_visible_horizontal (action));

	gtk_action_set_visible_vertical (
		GTK_ACTION (activatable),
		gtk_action_get_visible_vertical (action));

	gtk_action_set_is_important (
		GTK_ACTION (activatable),
		gtk_action_get_is_important (action));

	if (!gtk_activatable_get_use_action_appearance (activatable))
		return;

	gtk_action_set_label (
		GTK_ACTION (activatable),
		gtk_action_get_label (action));

	gtk_action_set_short_label (
		GTK_ACTION (activatable),
		gtk_action_get_short_label (action));

	gtk_action_set_tooltip (
		GTK_ACTION (activatable),
		gtk_action_get_tooltip (action));

	gtk_action_set_stock_id (
		GTK_ACTION (activatable),
		gtk_action_get_stock_id (action));

	gtk_action_set_gicon (
		GTK_ACTION (activatable),
		gtk_action_get_gicon (action));

	gtk_action_set_icon_name (
		GTK_ACTION (activatable),
		gtk_action_get_icon_name (action));
}

static void
e_popup_action_class_init (EPopupActionClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (EPopupActionPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = popup_action_set_property;
	object_class->get_property = popup_action_get_property;
	object_class->dispose = popup_action_dispose;

	g_object_class_override_property (
		object_class,
		PROP_RELATED_ACTION,
		"related-action");

	g_object_class_override_property (
		object_class,
		PROP_USE_ACTION_APPEARANCE,
		"use-action-appearance");
}

static void
e_popup_action_init (EPopupAction *popup_action)
{
	popup_action->priv = E_POPUP_ACTION_GET_PRIVATE (popup_action);
	popup_action->priv->use_action_appearance = TRUE;

	/* Remain invisible until we have a related action. */
	gtk_action_set_visible (GTK_ACTION (popup_action), FALSE);
}

static void
e_popup_action_activatable_init (GtkActivatableIface *iface)
{
	iface->update = popup_action_update;
	iface->sync_action_properties = popup_action_sync_action_properties;
}

EPopupAction *
e_popup_action_new (const gchar *name)
{
	g_return_val_if_fail (name != NULL, NULL);

	return g_object_new (E_TYPE_POPUP_ACTION, "name", name, NULL);
}

void
e_action_group_add_popup_actions (GtkActionGroup *action_group,
                                  const EPopupActionEntry *entries,
                                  guint n_entries)
{
	guint ii;

	g_return_if_fail (GTK_IS_ACTION_GROUP (action_group));

	for (ii = 0; ii < n_entries; ii++) {
		EPopupAction *popup_action;
		GtkAction *related_action;
		const gchar *label;

		label = gtk_action_group_translate_string (
			action_group, entries[ii].label);

		related_action = gtk_action_group_get_action (
			action_group, entries[ii].related);

		if (related_action == NULL) {
			g_warning (
				"Related action '%s' not found in "
				"action group '%s'", entries[ii].related,
				gtk_action_group_get_name (action_group));
			continue;
		}

		popup_action = e_popup_action_new (entries[ii].name);

		gtk_activatable_set_related_action (
			GTK_ACTIVATABLE (popup_action), related_action);

		if (label != NULL && *label != '\0')
			gtk_action_set_label (
				GTK_ACTION (popup_action), label);

		gtk_action_group_add_action (
			action_group, GTK_ACTION (popup_action));

		g_object_unref (popup_action);
	}
}
