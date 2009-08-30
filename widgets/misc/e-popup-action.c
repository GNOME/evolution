/*
 * e-popup-action.c
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "e-popup-action.h"

#include <glib/gi18n.h>
#include "e-util/e-binding.h"

#define E_POPUP_ACTION_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_POPUP_ACTION, EPopupActionPrivate))

enum {
	PROP_0,
	PROP_SOURCE
};

struct _EPopupActionPrivate {
	GtkAction *source;
};

static gpointer parent_class;

static void
popup_action_set_source (EPopupAction *popup_action,
                         GtkAction *source)
{
	g_return_if_fail (popup_action->priv->source == NULL);
	g_return_if_fail (GTK_IS_ACTION (source));

	popup_action->priv->source = g_object_ref (source);
}

static void
popup_action_set_property (GObject *object,
                           guint property_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SOURCE:
			popup_action_set_source (
				E_POPUP_ACTION (object),
				g_value_get_object (value));
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
		case PROP_SOURCE:
			g_value_set_object (
				value, e_popup_action_get_source (
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

	if (priv->source != NULL) {
		g_object_unref (priv->source);
		priv->source = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
popup_action_constructed (GObject *object)
{
	EPopupActionPrivate *priv;
	GObject *source;
	gchar *icon_name;
	gchar *label;
	gchar *stock_id;
	gchar *tooltip;

	priv = E_POPUP_ACTION_GET_PRIVATE (object);

	source = G_OBJECT (priv->source);

	g_object_get (
		object, "icon-name", &icon_name, "label", &label,
		"stock-id", &stock_id, "tooltip", &tooltip, NULL);

	if (label == NULL)
		e_binding_new (source, "label", object, "label");

	if (tooltip == NULL)
		e_binding_new (source, "tooltip", object, "tooltip");

	if (icon_name == NULL && stock_id == NULL) {
		g_free (icon_name);
		g_free (stock_id);

		g_object_get (
			source, "icon-name", &icon_name,
			"stock-id", &stock_id, NULL);

		if (icon_name == NULL) {
			e_binding_new (
				source, "icon-name", object, "icon-name");
			e_binding_new (
				source, "stock-id", object, "stock-id");
		} else {
			e_binding_new (
				source, "stock-id", object, "stock-id");
			e_binding_new (
				source, "icon-name", object, "icon-name");
		}
	}

	e_binding_new (source, "sensitive", object, "visible");

	g_free (icon_name);
	g_free (label);
	g_free (stock_id);
	g_free (tooltip);
}

static void
popup_action_class_init (EPopupActionClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EPopupActionPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = popup_action_set_property;
	object_class->get_property = popup_action_get_property;
	object_class->dispose = popup_action_dispose;

	g_object_class_install_property (
		object_class,
		PROP_SOURCE,
		g_param_spec_object (
			"source",
			_("Source Action"),
			_("The source action to proxy"),
			GTK_TYPE_ACTION,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));
}

static void
popup_action_init (EPopupAction *popup_action)
{
	popup_action->priv = E_POPUP_ACTION_GET_PRIVATE (popup_action);
}

GType
e_popup_action_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EPopupActionClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) popup_action_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EPopupAction),
			0,     /* n_preallocs */
			(GInstanceInitFunc) popup_action_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			GTK_TYPE_ACTION, "EPopupAction", &type_info, 0);
	}

	return type;
}

EPopupAction *
e_popup_action_new (const gchar *name,
                    const gchar *label,
                    GtkAction *source)
{
	EPopupAction *popup_action;

	g_return_val_if_fail (name != NULL, NULL);
	g_return_val_if_fail (GTK_IS_ACTION (source), NULL);

	popup_action = g_object_new (
		E_TYPE_POPUP_ACTION, "name", name,
		"label", label, "source", source, NULL);

	/* XXX This is a hack to work around the fact that GtkAction's
	 *     "label" and "tooltip" properties are not constructor
	 *     properties, even though they're supplied upfront.
	 *
	 *     See: http://bugzilla.gnome.org/show_bug.cgi?id=568334 */
	popup_action_constructed (G_OBJECT (popup_action));

	return popup_action;
}

GtkAction *
e_popup_action_get_source (EPopupAction *popup_action)
{
	g_return_val_if_fail (E_IS_POPUP_ACTION (popup_action), NULL);

	return popup_action->priv->source;
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
		GtkAction *source;
		const gchar *label;

		label = gtk_action_group_translate_string (
			action_group, entries[ii].label);

		source = gtk_action_group_get_action (
			action_group, entries[ii].source);

		if (source == NULL) {
			g_warning (
				"Source action '%s' not found in "
				"action group '%s'", entries[ii].source,
				gtk_action_group_get_name (action_group));
			continue;
		}

		popup_action = e_popup_action_new (
			entries[ii].name, label, source);

		g_signal_connect_swapped (
			popup_action, "activate",
			G_CALLBACK (gtk_action_activate),
			popup_action->priv->source);

		gtk_action_group_add_action (
			action_group, GTK_ACTION (popup_action));

		g_object_unref (popup_action);
	}
}
