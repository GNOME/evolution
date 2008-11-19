/*
 * e-shell-settings.c
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

#include "e-shell-settings.h"

#include "e-util/gconf-bridge.h"

#define E_SHELL_SETTINGS_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SHELL_SETTINGS, EShellSettingsPrivate))

struct _EShellSettingsPrivate {
	GArray *value_array;
};

static GList *instances;
static guint property_count;
static gpointer parent_class;

static void
shell_settings_set_property (GObject *object,
                             guint property_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
	EShellSettingsPrivate *priv;
	GValue *dest_value;

	priv = E_SHELL_SETTINGS_GET_PRIVATE (object);

	dest_value = &g_array_index (
		priv->value_array, GValue, property_id - 1);

	g_value_copy (value, dest_value);
	g_object_notify (object, pspec->name);
}

static void
shell_settings_get_property (GObject *object,
                             guint property_id,
                             GValue *value,
                             GParamSpec *pspec)
{
	EShellSettingsPrivate *priv;
	GValue *src_value;

	priv = E_SHELL_SETTINGS_GET_PRIVATE (object);

	src_value = &g_array_index (
		priv->value_array, GValue, property_id - 1);

	g_value_copy (src_value, value);
}

static void
shell_settings_finalize (GObject *object)
{
	EShellSettingsPrivate *priv;
	guint ii;

	priv = E_SHELL_SETTINGS_GET_PRIVATE (object);

	for (ii = 0; ii < priv->value_array->len; ii++)
		g_value_unset (&g_array_index (priv->value_array, GValue, ii));

	g_array_free (priv->value_array, TRUE);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
shell_settings_class_init (EShellSettingsClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EShellSettingsPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = shell_settings_set_property;
	object_class->get_property = shell_settings_get_property;
	object_class->finalize = shell_settings_finalize;
}

static void
shell_settings_init (EShellSettings *shell_settings,
                     GObjectClass *object_class)
{
	GArray *value_array;

	instances = g_list_prepend (instances, shell_settings);

	value_array = g_array_new (FALSE, TRUE, sizeof (GValue));
	g_array_set_size (value_array, property_count);

	shell_settings->priv = E_SHELL_SETTINGS_GET_PRIVATE (shell_settings);
	shell_settings->priv->value_array = value_array;
}

GType
e_shell_settings_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		const GTypeInfo type_info = {
			sizeof (EShellSettingsClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) shell_settings_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EShellSettings),
			0,     /* n_preallocs */
			(GInstanceInitFunc) shell_settings_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			G_TYPE_OBJECT, "EShellSettings", &type_info, 0);
	}

	return type;
}

void
e_shell_settings_install_property (GParamSpec *pspec)
{
	static GObjectClass *class = NULL;
	GList *iter, *next;

	g_return_if_fail (G_IS_PARAM_SPEC (pspec));

	if (G_UNLIKELY (class == NULL))
		class = g_type_class_ref (E_TYPE_SHELL_SETTINGS);

	if (g_object_class_find_property (class, pspec->name) != NULL) {
		g_warning (
			"Settings property \"%s\" already exists",
			pspec->name);
		return;
	}

	for (iter = instances; iter != NULL; iter = iter->next)
		g_object_freeze_notify (iter->data);

	g_object_class_install_property (class, ++property_count, pspec);

	for (iter = instances; iter != NULL; iter = iter->next) {
		EShellSettings *shell_settings = iter->data;
		GArray *value_array;
		GValue *value;

		value_array = shell_settings->priv->value_array;
		g_array_set_size (value_array, property_count);

		value = &g_array_index (
			value_array, GValue, property_count - 1);
		g_value_init (value, G_PARAM_SPEC_VALUE_TYPE (pspec));
		g_param_value_set_default (pspec, value);
		g_object_notify (G_OBJECT (shell_settings), pspec->name);
	}

	for (iter = instances; iter != NULL; iter = next) {
		next = iter->next;
		g_object_thaw_notify (iter->data);
	}
}

void
e_shell_settings_bind_to_gconf (EShellSettings *shell_settings,
                                const gchar *property_name,
                                const gchar *gconf_key)
{
	g_return_if_fail (E_IS_SHELL_SETTINGS (shell_settings));
	g_return_if_fail (property_name != NULL);
	g_return_if_fail (gconf_key != NULL);

	gconf_bridge_bind_property (
		gconf_bridge_get (), gconf_key,
		G_OBJECT (shell_settings), property_name);
}
