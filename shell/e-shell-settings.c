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

/**
 * SECTION: e-shell-settings
 * @short_description: settings management
 * @include: shell/e-shell-settings.h
 **/

#include "e-shell-settings.h"

#include "e-util/gconf-bridge.h"

#define E_SHELL_SETTINGS_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SHELL_SETTINGS, EShellSettingsPrivate))

struct _EShellSettingsPrivate {
	GArray *value_array;
	guint debug	: 1;
};

static GList *instances;
static guint property_count;
static gpointer parent_class;

static GParamSpec *
shell_settings_pspec_for_key (const gchar *property_name,
                              const gchar *gconf_key)
{
	GConfClient *client;
	GConfEntry *entry;
	GConfSchema *schema;
	GConfValue *default_value;
	GConfValueType value_type;
	GParamSpec *pspec;
	const gchar *bad_type;
	const gchar *schema_name;
	GError *error = NULL;

	client = gconf_client_get_default ();

	entry = gconf_client_get_entry (client, gconf_key, NULL, TRUE, &error);
	if (error != NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
		return NULL;
	}

	schema_name = gconf_entry_get_schema_name (entry);
	g_return_val_if_fail (schema_name != NULL, NULL);

	schema = gconf_client_get_schema (client, schema_name, &error);
	if (error != NULL) {
		g_warning ("%s", error->message);
		gconf_entry_unref (entry);
		g_error_free (error);
		return NULL;
	}

	value_type = gconf_schema_get_type (schema);
	default_value = gconf_schema_get_default_value (schema);

	/* If the schema does not specify a default value, make one up. */
	if (default_value == NULL) {
		default_value = gconf_value_new (value_type);

		/* XXX This should NOT be necessary, but the GConfValue
		 *     documentation claims it is.  Bother. */
		switch (value_type) {
			case GCONF_VALUE_STRING:
				gconf_value_set_string (default_value, "");
				break;

			case GCONF_VALUE_INT:
				gconf_value_set_int (default_value, 0);
				break;

			case GCONF_VALUE_FLOAT:
				gconf_value_set_float (default_value, 0.0);
				break;

			case GCONF_VALUE_BOOL:
				gconf_value_set_bool (default_value, FALSE);
				break;

			default:
				/* We'll fail in the next switch statement. */
				break;
		}
	} else {
		default_value = gconf_value_copy (default_value);
	}

	switch (value_type) {
		case GCONF_VALUE_STRING:
			pspec = g_param_spec_string (
				property_name, NULL, NULL,
				gconf_value_get_string (default_value),
				G_PARAM_READWRITE);
			break;

		case GCONF_VALUE_INT:
			pspec = g_param_spec_int (
				property_name, NULL, NULL,
				G_MININT, G_MAXINT,
				gconf_value_get_int (default_value),
				G_PARAM_READWRITE);
			break;

		case GCONF_VALUE_FLOAT:
			pspec = g_param_spec_double (
				property_name, NULL, NULL,
				-G_MAXDOUBLE, G_MAXDOUBLE,
				gconf_value_get_float (default_value),
				G_PARAM_READWRITE);
			break;

		case GCONF_VALUE_BOOL:
			pspec = g_param_spec_boolean (
				property_name, NULL, NULL,
				gconf_value_get_bool (default_value),
				G_PARAM_READWRITE);
			break;

		case GCONF_VALUE_SCHEMA:
			bad_type = "schema";
			goto fail;

		case GCONF_VALUE_LIST:
			bad_type = "list";
			goto fail;

		case GCONF_VALUE_PAIR:
			bad_type = "pair";
			goto fail;

		default:
			bad_type = "invalid";
			goto fail;
	}

	gconf_value_free (default_value);
	gconf_schema_free (schema);
	gconf_entry_unref (entry);

	return pspec;

fail:
	g_error (
		"Unable to create EShellSettings property for "
		"GConf key '%s' of type '%s'", gconf_key, bad_type);
	g_assert_not_reached ();
}

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

	if (priv->debug) {
		gchar *contents;

		contents = g_strdup_value_contents (value);
		g_debug (
			"Setting '%s' set to '%s' (%s)",
			pspec->name, contents, G_VALUE_TYPE_NAME (value));
		g_free (contents);
	}
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
	GParamSpec **pspecs;
	guint ii;

	instances = g_list_prepend (instances, shell_settings);

	value_array = g_array_new (FALSE, TRUE, sizeof (GValue));
	g_array_set_size (value_array, property_count);

	shell_settings->priv = E_SHELL_SETTINGS_GET_PRIVATE (shell_settings);
	shell_settings->priv->value_array = value_array;

	g_object_freeze_notify (G_OBJECT (shell_settings));

	pspecs = g_object_class_list_properties (object_class, NULL);
	for (ii = 0; ii < property_count; ii++) {
		GParamSpec *pspec = pspecs[ii];
		GValue *value;

		value = &g_array_index (value_array, GValue, ii);
		g_value_init (value, G_PARAM_SPEC_VALUE_TYPE (pspec));
		g_param_value_set_default (pspec, value);
		g_object_notify (G_OBJECT (shell_settings), pspec->name);

		/* FIXME Need to bind those properties that have
		 *       associated GConf keys. */
	}
	g_free (pspecs);

	g_object_thaw_notify (G_OBJECT (shell_settings));
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

/**
 * e_shell_settings_install_property:
 * @pspec: a #GParamSpec
 *
 * Installs a new #EShellSettings class property from @pspec.
 * This is usually done during initialization of an #EShellBackend
 * or other dynamically loaded entity.
 **/
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
		EShellSettings *shell_settings;
		GArray *value_array;
		GValue *value;

		shell_settings = E_SHELL_SETTINGS (iter->data);
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

/**
 * e_shell_settings_install_property_for_key:
 * @property_name: the name of the property to install
 * @gconf_key: the GConf key to bind the property to
 *
 * Installs a new #EShellSettings class property by examining the
 * GConf schema for @gconf_key to determine the appropriate type and
 * default value.  This is usually done during initialization of an
 * #EShellBackend of other dynamically loaded entity.
 *
 * After the class property is installed, all #EShellSettings instances
 * are bound to @gconf_key, causing @property_name and @gconf_key to have
 * the same value at all times.
 **/
void
e_shell_settings_install_property_for_key (const gchar *property_name,
                                           const gchar *gconf_key)
{
	GParamSpec *pspec;
	GList *iter, *next;

	g_return_if_fail (property_name != NULL);
	g_return_if_fail (gconf_key != NULL);

	pspec = shell_settings_pspec_for_key (property_name, gconf_key);
	if (!pspec)
		return;

	e_shell_settings_install_property (pspec);

	for (iter = instances; iter != NULL; iter = iter->next)
		g_object_freeze_notify (iter->data);

	for (iter = instances; iter != NULL; iter = iter->next) {
		EShellSettings *shell_settings;

		shell_settings = E_SHELL_SETTINGS (iter->data);

		gconf_bridge_bind_property (
			gconf_bridge_get (), gconf_key,
			G_OBJECT (shell_settings), property_name);
	}

	for (iter = instances; iter != NULL; iter = next) {
		next = iter->next;
		g_object_thaw_notify (iter->data);
	}
}

/**
 * e_shell_settings_enable_debug:
 * @shell_settings: an #EShellSettings
 *
 * Print a debug message to standard output when a property value changes.
 **/
void
e_shell_settings_enable_debug (EShellSettings *shell_settings)
{
	g_return_if_fail (E_IS_SHELL_SETTINGS (shell_settings));

	shell_settings->priv->debug = TRUE;
}

/**
 * e_shell_settings_get_boolean:
 * @shell_settings: an #EShellSettings
 * @property_name: an installed property name
 *
 * Return the contents of an #EShellSettings property of type
 * #G_TYPE_BOOLEAN.
 *
 * Returns: boolean contents of @property_name
 **/
gboolean
e_shell_settings_get_boolean (EShellSettings *shell_settings,
                              const gchar *property_name)
{
	GObject *object;
	GValue value = { 0, };
	gboolean v_boolean;

	g_return_val_if_fail (E_IS_SHELL_SETTINGS (shell_settings), FALSE);
	g_return_val_if_fail (property_name != NULL, FALSE);

	object = G_OBJECT (shell_settings);
	g_value_init (&value, G_TYPE_BOOLEAN);
	g_object_get_property (object, property_name, &value);
	v_boolean = g_value_get_boolean (&value);
	g_value_unset (&value);

	return v_boolean;
}

/**
 * e_shell_settings_set_boolean:
 * @shell_settings: an #EShellSettings
 * @property_name: an installed property name
 * @v_boolean: boolean value to be set
 *
 * Sets the contents of an #EShellSettings property of type #G_TYPE_BOOLEAN
 * to @v_boolean.  If @property_name is bound to a GConf key, the GConf key
 * will also be set to @v_boolean.
 **/
void
e_shell_settings_set_boolean (EShellSettings *shell_settings,
                              const gchar *property_name,
                              gboolean v_boolean)
{
	GObject *object;
	GValue value = { 0, };

	g_return_if_fail (E_IS_SHELL_SETTINGS (shell_settings));
	g_return_if_fail (property_name != NULL);

	object = G_OBJECT (shell_settings);
	g_value_init (&value, G_TYPE_BOOLEAN);
	g_value_set_boolean (&value, v_boolean);
	g_object_set_property (object, property_name, &value);
	g_value_unset (&value);
}

/**
 * e_shell_settings_get_int:
 * @shell_settings: an #EShellSettings
 * @property_name: an installed property name
 *
 * Returns the contents of an #EShellSettings property of type
 * #G_TYPE_INT.
 *
 * Returns: integer contents of @property_name
 **/
gint
e_shell_settings_get_int (EShellSettings *shell_settings,
                          const gchar *property_name)
{
	GObject *object;
	GValue value = { 0, };
	gint v_int;

	g_return_val_if_fail (E_IS_SHELL_SETTINGS (shell_settings), 0);
	g_return_val_if_fail (property_name != NULL, 0);

	object = G_OBJECT (shell_settings);
	g_value_init (&value, G_TYPE_INT);
	g_object_get_property (object, property_name, &value);
	v_int = g_value_get_int (&value);
	g_value_unset (&value);

	return v_int;
}

/**
 * e_shell_settings_set_int:
 * @shell_settings: an #EShellSettings
 * @property_name: an installed property name
 * @v_int: integer value to be set
 *
 * Sets the contents of an #EShellSettings property of type #G_TYPE_INT
 * to @v_int.  If @property_name is bound to a GConf key, the GConf key
 * will also be set to @v_int.
 **/
void
e_shell_settings_set_int (EShellSettings *shell_settings,
                          const gchar *property_name,
                          gint v_int)
{
	GObject *object;
	GValue value = { 0, };

	g_return_if_fail (E_IS_SHELL_SETTINGS (shell_settings));
	g_return_if_fail (property_name != NULL);

	object = G_OBJECT (shell_settings);
	g_value_init (&value, G_TYPE_INT);
	g_value_set_int (&value, v_int);
	g_object_set_property (object, property_name, &value);
	g_value_unset (&value);
}

/**
 * e_shell_settings_get_string:
 * @shell_settings: an #EShellSettings
 * @property_name: an installed property name
 *
 * Returns the contents of an #EShellSettings property of type
 * #G_TYPE_STRING.  The returned string should be freed using g_free().
 *
 * Returns: string contents of @property_name
 **/
gchar *
e_shell_settings_get_string (EShellSettings *shell_settings,
                             const gchar *property_name)
{
	GObject *object;
	GValue value = { 0, };
	gchar *v_string;

	g_return_val_if_fail (E_IS_SHELL_SETTINGS (shell_settings), NULL);
	g_return_val_if_fail (property_name != NULL, NULL);

	object = G_OBJECT (shell_settings);
	g_value_init (&value, G_TYPE_STRING);
	g_object_get_property (object, property_name, &value);
	v_string = g_value_dup_string (&value);
	g_value_unset (&value);

	return v_string;
}

/**
 * e_shell_settings_set_string:
 * @shell_settings: an #EShellSettings
 * @property_name: an installed property name
 * @v_string: string to be set
 *
 * Sets the contents of an #EShellSettings property of type #G_TYPE_STRING
 * to @v_string.  If @property_name is bound to a GConf key, the GConf key
 * will also be set to @v_string.
 **/
void
e_shell_settings_set_string (EShellSettings *shell_settings,
                             const gchar *property_name,
                             const gchar *v_string)
{
	GObject *object;
	GValue value = { 0, };

	g_return_if_fail (E_IS_SHELL_SETTINGS (shell_settings));
	g_return_if_fail (property_name != NULL);

	object = G_OBJECT (shell_settings);
	g_value_init (&value, G_TYPE_STRING);
	g_value_set_string (&value, v_string);
	g_object_set_property (object, property_name, &value);
	g_value_unset (&value);
}

/**
 * e_shell_settings_get_object:
 * @shell_settings: an #EShellSettings
 * @property_name: an installed property name
 *
 * Returns the contents of an #EShellSettings property of type
 * #G_TYPE_OBJECT.  The caller owns the reference to the returned
 * object, and should call g_object_unref() when finished with it.
 *
 * Returns: a new reference to the object under @property_name
 **/
gpointer
e_shell_settings_get_object (EShellSettings *shell_settings,
                             const gchar *property_name)
{
	GObject *object;
	GValue value = { 0, };
	gpointer v_object;

	g_return_val_if_fail (E_IS_SHELL_SETTINGS (shell_settings), NULL);
	g_return_val_if_fail (property_name != NULL, NULL);

	object = G_OBJECT (shell_settings);
	g_value_init (&value, G_TYPE_OBJECT);
	g_object_get_property (object, property_name, &value);
	v_object = g_value_dup_object (&value);
	g_value_unset (&value);

	return v_object;
}

/**
 * e_shell_settings_set_object:
 * @shell_settings: an #EShellSettings
 * @property_name: an installed property name
 * @v_object: object to be set
 *
 * Sets the contents of an #EShellSettings property of type #G_TYPE_OBJECT
 * to @v_object.
 **/
void
e_shell_settings_set_object (EShellSettings *shell_settings,
                             const gchar *property_name,
                             gpointer v_object)
{
	GObject *object;
	GValue value = { 0, };

	g_return_if_fail (E_IS_SHELL_SETTINGS (shell_settings));
	g_return_if_fail (property_name != NULL);

	object = G_OBJECT (shell_settings);
	g_value_init (&value, G_TYPE_OBJECT);
	g_value_set_object (&value, v_object);
	g_object_set_property (object, property_name, &value);
	g_value_unset (&value);
}

/**
 * e_shell_settings_get_pointer:
 * @shell_settings: an #EShellSettings
 * @property_name: an installed property name
 *
 * Returns the contents of an #EShellSettings property of type
 * #G_TYPE_POINTER.
 *
 * Returns: pointer contents of @property_name
 **/
gpointer
e_shell_settings_get_pointer (EShellSettings *shell_settings,
                              const gchar *property_name)
{
	GObject *object;
	GValue value = { 0, };
	gpointer v_pointer;

	g_return_val_if_fail (E_IS_SHELL_SETTINGS (shell_settings), NULL);
	g_return_val_if_fail (property_name != NULL, NULL);

	object = G_OBJECT (shell_settings);
	g_value_init (&value, G_TYPE_POINTER);
	g_object_get_property (object, property_name, &value);
	v_pointer = g_value_get_pointer (&value);
	g_value_unset (&value);

	return v_pointer;
}

/**
 * e_shell_settings_set_pointer:
 * @shell_settings: an #EShellSettings
 * @property_name: an installed property name
 * @v_pointer: pointer to be set
 *
 * Sets the contents of an #EShellSettings property of type #G_TYPE_POINTER
 * to @v_pointer.
 **/
void
e_shell_settings_set_pointer (EShellSettings *shell_settings,
                              const gchar *property_name,
                              gpointer v_pointer)
{
	GObject *object;
	GValue value = { 0, };

	g_return_if_fail (E_IS_SHELL_SETTINGS (shell_settings));
	g_return_if_fail (property_name != NULL);

	object = G_OBJECT (shell_settings);
	g_value_init (&value, G_TYPE_POINTER);
	g_value_set_pointer (&value, v_pointer);
	g_object_set_property (object, property_name, &value);
	g_value_unset (&value);
}
