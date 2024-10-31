/*
 * Copyright (C) 2017 Red Hat, Inc. (www.redhat.com)
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION: e-config-lookup-result-simple
 * @include: e-util/e-util.h
 * @short_description: An implementation of configuration lookup result interface
 *
 * #EConfigLookupResultSimple is a simple implementation of
 * the #EConfigLookupResult interface.
 *
 * Respective configuration changes are added with e_config_lookup_result_simple_add_value(),
 * then they are saved into the #ESource in #EConfigLookupResultInterface.configure_sources()
 * call. This does the default implementation of #EConfigLookupResultSimpleClass.configure_sources(),
 * which any descendants can override, if needed.
 **/

#include "evolution-config.h"

#include <string.h>
#include <libedataserver/libedataserver.h>

#include "e-util-enumtypes.h"
#include "e-config-lookup.h"
#include "e-config-lookup-result.h"

#include "e-config-lookup-result-simple.h"

struct _EConfigLookupResultSimplePrivate {
	EConfigLookupResultKind kind;
	gint priority;
	gboolean is_complete;
	gchar *protocol;
	gchar *display_name;
	gchar *description;
	gchar *password;
	GSList *values; /* ValueData * */
};

enum {
	PROP_0,
	PROP_KIND,
	PROP_PRIORITY,
	PROP_IS_COMPLETE,
	PROP_PROTOCOL,
	PROP_DISPLAY_NAME,
	PROP_DESCRIPTION,
	PROP_PASSWORD
};

static void e_config_lookup_result_simple_result_init (EConfigLookupResultInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EConfigLookupResultSimple, e_config_lookup_result_simple, G_TYPE_OBJECT,
	G_ADD_PRIVATE (EConfigLookupResultSimple)
	G_IMPLEMENT_INTERFACE (E_TYPE_CONFIG_LOOKUP_RESULT, e_config_lookup_result_simple_result_init))

typedef struct _ValueData {
	gchar *extension_name;
	gchar *property_name;
	GValue value;
} ValueData;

static ValueData *
value_data_new (const gchar *extension_name,
		const gchar *property_name,
		const GValue *value)
{
	ValueData *vd;

	vd = g_slice_new0 (ValueData);
	vd->extension_name = g_strdup (extension_name);
	vd->property_name = g_strdup (property_name);

	g_value_init (&vd->value, G_VALUE_TYPE (value));
	g_value_copy (value, &vd->value);

	return vd;
}

static void
value_data_free (gpointer ptr)
{
	ValueData *vd = ptr;

	if (vd) {
		g_free (vd->extension_name);
		g_free (vd->property_name);
		g_value_reset (&vd->value);
		g_slice_free (ValueData, vd);
	}
}

static EConfigLookupResultKind
config_lookup_result_simple_get_kind (EConfigLookupResult *lookup_result)
{
	g_return_val_if_fail (E_IS_CONFIG_LOOKUP_RESULT_SIMPLE (lookup_result), E_CONFIG_LOOKUP_RESULT_UNKNOWN);

	return E_CONFIG_LOOKUP_RESULT_SIMPLE (lookup_result)->priv->kind;
}

static gint
config_lookup_result_simple_get_priority (EConfigLookupResult *lookup_result)
{
	g_return_val_if_fail (E_IS_CONFIG_LOOKUP_RESULT_SIMPLE (lookup_result), ~0);

	return E_CONFIG_LOOKUP_RESULT_SIMPLE (lookup_result)->priv->priority;
}

static gboolean
config_lookup_result_simple_get_is_complete (EConfigLookupResult *lookup_result)
{
	g_return_val_if_fail (E_IS_CONFIG_LOOKUP_RESULT_SIMPLE (lookup_result), FALSE);

	return E_CONFIG_LOOKUP_RESULT_SIMPLE (lookup_result)->priv->is_complete;
}

static const gchar *
config_lookup_result_simple_get_protocol (EConfigLookupResult *lookup_result)
{
	g_return_val_if_fail (E_IS_CONFIG_LOOKUP_RESULT_SIMPLE (lookup_result), NULL);

	return E_CONFIG_LOOKUP_RESULT_SIMPLE (lookup_result)->priv->protocol;
}

static const gchar *
config_lookup_result_simple_get_display_name (EConfigLookupResult *lookup_result)
{
	g_return_val_if_fail (E_IS_CONFIG_LOOKUP_RESULT_SIMPLE (lookup_result), NULL);

	return E_CONFIG_LOOKUP_RESULT_SIMPLE (lookup_result)->priv->display_name;
}

static const gchar *
config_lookup_result_simple_get_description (EConfigLookupResult *lookup_result)
{
	g_return_val_if_fail (E_IS_CONFIG_LOOKUP_RESULT_SIMPLE (lookup_result), NULL);

	return E_CONFIG_LOOKUP_RESULT_SIMPLE (lookup_result)->priv->description;
}

static const gchar *
config_lookup_result_simple_get_password (EConfigLookupResult *lookup_result)
{
	g_return_val_if_fail (E_IS_CONFIG_LOOKUP_RESULT_SIMPLE (lookup_result), NULL);

	return E_CONFIG_LOOKUP_RESULT_SIMPLE (lookup_result)->priv->password;
}

static gboolean
config_lookup_result_simple_configure_source (EConfigLookupResult *lookup_result,
					      EConfigLookup *config_lookup,
					      ESource *source)
{
	EConfigLookupResultSimple *result_simple;
	GSList *link;

	g_return_val_if_fail (E_IS_CONFIG_LOOKUP_RESULT_SIMPLE (lookup_result), FALSE);
	g_return_val_if_fail (E_IS_CONFIG_LOOKUP (config_lookup), FALSE);
	g_return_val_if_fail (E_IS_SOURCE (source), FALSE);

	result_simple = E_CONFIG_LOOKUP_RESULT_SIMPLE (lookup_result);

	if (!result_simple->priv->values)
		return FALSE;

	for (link = result_simple->priv->values; link; link = g_slist_next (link)) {
		ValueData *vd = link->data;
		gpointer object;

		if (!vd)
			return FALSE;

		if (vd->extension_name && *vd->extension_name) {
			object = e_source_get_extension (source, vd->extension_name);

			/* Special-case the ESourceCamel extension, where the properties
			   reference the CamelSettings object, not the extension itself. */
			if (object && E_IS_SOURCE_CAMEL (object))
				object = e_source_camel_get_settings (object);
		} else {
			object = source;
		}

		g_warn_if_fail (object != NULL);

		if (object)
			g_object_set_property (object, vd->property_name, &vd->value);
	}

	return TRUE;
}

static gboolean
config_lookup_result_simple_configure_source_wrapper (EConfigLookupResult *lookup_result,
						      EConfigLookup *config_lookup,
						      ESource *source)
{
	EConfigLookupResultSimpleClass *klass;

	g_return_val_if_fail (E_IS_CONFIG_LOOKUP_RESULT_SIMPLE (lookup_result), FALSE);
	g_return_val_if_fail (E_IS_CONFIG_LOOKUP (config_lookup), FALSE);
	g_return_val_if_fail (E_IS_SOURCE (source), FALSE);

	klass = E_CONFIG_LOOKUP_RESULT_SIMPLE_GET_CLASS (lookup_result);
	g_return_val_if_fail (klass != NULL, FALSE);
	g_return_val_if_fail (klass->configure_source != NULL, FALSE);

	return klass->configure_source (lookup_result, config_lookup, source);
}

static void
config_lookup_result_simple_set_kind (EConfigLookupResultSimple *result_simple,
				      EConfigLookupResultKind kind)
{
	g_return_if_fail (E_IS_CONFIG_LOOKUP_RESULT_SIMPLE (result_simple));
	g_return_if_fail (kind != E_CONFIG_LOOKUP_RESULT_UNKNOWN);

	result_simple->priv->kind = kind;
}

static void
config_lookup_result_simple_set_priority (EConfigLookupResultSimple *result_simple,
					  gint priority)
{
	g_return_if_fail (E_IS_CONFIG_LOOKUP_RESULT_SIMPLE (result_simple));

	result_simple->priv->priority = priority;
}

static void
config_lookup_result_simple_set_is_complete (EConfigLookupResultSimple *result_simple,
					     gboolean is_complete)
{
	g_return_if_fail (E_IS_CONFIG_LOOKUP_RESULT_SIMPLE (result_simple));

	result_simple->priv->is_complete = is_complete;
}

static void
config_lookup_result_simple_set_string (EConfigLookupResultSimple *result_simple,
					const gchar *value,
					gchar **destination)
{
	g_return_if_fail (E_IS_CONFIG_LOOKUP_RESULT_SIMPLE (result_simple));
	g_return_if_fail (destination != NULL);
	g_return_if_fail (*destination == NULL);

	*destination = g_strdup (value);
}

static void
config_lookup_result_simple_set_property (GObject *object,
					  guint property_id,
					  const GValue *value,
					  GParamSpec *pspec)
{
	EConfigLookupResultSimple *result_simple = E_CONFIG_LOOKUP_RESULT_SIMPLE (object);

	switch (property_id) {
		case PROP_KIND:
			config_lookup_result_simple_set_kind (
				result_simple, g_value_get_enum (value));
			return;

		case PROP_PRIORITY:
			config_lookup_result_simple_set_priority (
				result_simple, g_value_get_int (value));
			return;

		case PROP_IS_COMPLETE:
			config_lookup_result_simple_set_is_complete (
				result_simple, g_value_get_boolean (value));
			return;

		case PROP_PROTOCOL:
			config_lookup_result_simple_set_string (
				result_simple, g_value_get_string (value),
				&result_simple->priv->protocol);
			return;

		case PROP_DISPLAY_NAME:
			config_lookup_result_simple_set_string (
				result_simple, g_value_get_string (value),
				&result_simple->priv->display_name);
			return;

		case PROP_DESCRIPTION:
			config_lookup_result_simple_set_string (
				result_simple, g_value_get_string (value),
				&result_simple->priv->description);
			return;

		case PROP_PASSWORD:
			config_lookup_result_simple_set_string (
				result_simple, g_value_get_string (value),
				&result_simple->priv->password);
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
config_lookup_result_simple_get_property (GObject *object,
					  guint property_id,
					  GValue *value,
					  GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_KIND:
			g_value_set_enum (
				value,
				config_lookup_result_simple_get_kind (
				E_CONFIG_LOOKUP_RESULT (object)));
			return;

		case PROP_PRIORITY:
			g_value_set_int (
				value,
				config_lookup_result_simple_get_priority (
				E_CONFIG_LOOKUP_RESULT (object)));
			return;

		case PROP_IS_COMPLETE:
			g_value_set_boolean (
				value,
				config_lookup_result_simple_get_is_complete (
				E_CONFIG_LOOKUP_RESULT (object)));
			return;

		case PROP_PROTOCOL:
			g_value_set_string (
				value,
				config_lookup_result_simple_get_protocol (
				E_CONFIG_LOOKUP_RESULT (object)));
			return;

		case PROP_DISPLAY_NAME:
			g_value_set_string (
				value,
				config_lookup_result_simple_get_display_name (
				E_CONFIG_LOOKUP_RESULT (object)));
			return;

		case PROP_DESCRIPTION:
			g_value_set_string (
				value,
				config_lookup_result_simple_get_description (
				E_CONFIG_LOOKUP_RESULT (object)));
			return;

		case PROP_PASSWORD:
			g_value_set_string (
				value,
				config_lookup_result_simple_get_password (
				E_CONFIG_LOOKUP_RESULT (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
config_lookup_result_simple_finalize (GObject *object)
{
	EConfigLookupResultSimple *result_simple = E_CONFIG_LOOKUP_RESULT_SIMPLE (object);

	g_free (result_simple->priv->protocol);
	g_free (result_simple->priv->display_name);
	g_free (result_simple->priv->description);
	e_util_safe_free_string (result_simple->priv->password);
	g_slist_free_full (result_simple->priv->values, value_data_free);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_config_lookup_result_simple_parent_class)->finalize (object);
}

static void
e_config_lookup_result_simple_class_init (EConfigLookupResultSimpleClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->set_property = config_lookup_result_simple_set_property;
	object_class->get_property = config_lookup_result_simple_get_property;
	object_class->finalize = config_lookup_result_simple_finalize;

	klass->configure_source = config_lookup_result_simple_configure_source;

	/**
	 * EConfigLookupResultSimple:kind:
	 *
	 * The kind for the #EConfigLookupResult.
	 *
	 * Since: 3.26
	 **/
	g_object_class_install_property (
		object_class,
		PROP_KIND,
		g_param_spec_enum (
			"kind",
			"Kind",
			NULL,
			E_TYPE_CONFIG_LOOKUP_RESULT_KIND,
			E_CONFIG_LOOKUP_RESULT_UNKNOWN,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EConfigLookupResultSimple:priority:
	 *
	 * The priority for the #EConfigLookupResult. Lower value means higher priority.
	 *
	 * Since: 3.26
	 **/
	g_object_class_install_property (
		object_class,
		PROP_PRIORITY,
		g_param_spec_int (
			"priority",
			"Priority",
			NULL,
			G_MININT, G_MAXINT, ~0,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EConfigLookupResultSimple:is-complete:
	 *
	 * Whether the #EConfigLookupResult is complete, that is, whether it doesn't
	 * require any further user interaction.
	 *
	 * Since: 3.26
	 **/
	g_object_class_install_property (
		object_class,
		PROP_IS_COMPLETE,
		g_param_spec_boolean (
			"is-complete",
			"Is Complete",
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EConfigLookupResultSimple:protocol:
	 *
	 * The protocol name for the #EConfigLookupResult.
	 *
	 * Since: 3.26
	 **/
	g_object_class_install_property (
		object_class,
		PROP_PROTOCOL,
		g_param_spec_string (
			"protocol",
			"Protocol",
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EConfigLookupResultSimple:display_name:
	 *
	 * The display name for the #EConfigLookupResult.
	 *
	 * Since: 3.26
	 **/
	g_object_class_install_property (
		object_class,
		PROP_DISPLAY_NAME,
		g_param_spec_string (
			"display-name",
			"Display Name",
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EConfigLookupResultSimple:description:
	 *
	 * The description for the #EConfigLookupResult.
	 *
	 * Since: 3.26
	 **/
	g_object_class_install_property (
		object_class,
		PROP_DESCRIPTION,
		g_param_spec_string (
			"description",
			"Description",
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EConfigLookupResultSimple:password:
	 *
	 * The password to store for the #EConfigLookupResult.
	 * Can be %NULL, to not store any.
	 *
	 * Since: 3.28
	 **/
	g_object_class_install_property (
		object_class,
		PROP_PASSWORD,
		g_param_spec_string (
			"password",
			"Password",
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));
}

static void
e_config_lookup_result_simple_result_init (EConfigLookupResultInterface *iface)
{
	iface->get_kind = config_lookup_result_simple_get_kind;
	iface->get_priority = config_lookup_result_simple_get_priority;
	iface->get_is_complete = config_lookup_result_simple_get_is_complete;
	iface->get_protocol = config_lookup_result_simple_get_protocol;
	iface->get_display_name = config_lookup_result_simple_get_display_name;
	iface->get_description = config_lookup_result_simple_get_description;
	iface->get_password = config_lookup_result_simple_get_password;
	iface->configure_source = config_lookup_result_simple_configure_source_wrapper;
}

static void
e_config_lookup_result_simple_init (EConfigLookupResultSimple *result_simple)
{
	result_simple->priv = e_config_lookup_result_simple_get_instance_private (result_simple);
}

/**
 * e_config_lookup_result_simple_new:
 * @kind: a kind of the result, one of #EConfigLookupResultKind
 * @priority: a priority of the result
 * @is_complete: whether the result is complete
 * @protocol: (nullable): protocol name of the result, or %NULL
 * @display_name: display name of the result
 * @description: description of the result
 * @password: (nullable): password to store with the result
 *
 * Creates a new #EConfigLookupResultSimple instance with prefilled values.
 *
 * Returns: (transfer full): an #EConfigLookupResultSimple
 *
 * Since: 3.26
 **/
EConfigLookupResult *
e_config_lookup_result_simple_new (EConfigLookupResultKind kind,
				   gint priority,
				   gboolean is_complete,
				   const gchar *protocol,
				   const gchar *display_name,
				   const gchar *description,
				   const gchar *password)
{
	g_return_val_if_fail (kind != E_CONFIG_LOOKUP_RESULT_UNKNOWN, NULL);
	g_return_val_if_fail (display_name != NULL, NULL);
	g_return_val_if_fail (description != NULL, NULL);

	return g_object_new (E_TYPE_CONFIG_LOOKUP_RESULT_SIMPLE,
		"kind", kind,
		"priority", priority,
		"is-complete", is_complete,
		"protocol", protocol,
		"display-name", display_name,
		"description", description,
		"password", password,
		NULL);
}

/**
 * e_config_lookup_result_simple_add_value:
 * @lookup_result: an #EConfigLookupResultSimple
 * @extension_name: (nullable): extension name, or %NULL, to change property of the #ESource itself
 * @property_name: property name within the extension
 * @value: value to be set
 *
 * Adds a value to be stored into an #ESource when e_config_lookup_result_configure_source().
 * is called. The @value is identified as a property named @property_name in an extension
 * named @extension_name, or in the #ESource itself, when @extension_name is %NULL.
 *
 * In case multiple values are stored for the same extension and property,
 * then the first is saved.
 *
 * Since: 3.26
 **/
void
e_config_lookup_result_simple_add_value (EConfigLookupResult *lookup_result,
					 const gchar *extension_name,
					 const gchar *property_name,
					 const GValue *value)
{
	EConfigLookupResultSimple *result_simple;

	g_return_if_fail (E_IS_CONFIG_LOOKUP_RESULT_SIMPLE (lookup_result));
	g_return_if_fail (property_name != NULL);
	g_return_if_fail (value != NULL);

	result_simple = E_CONFIG_LOOKUP_RESULT_SIMPLE (lookup_result);

	result_simple->priv->values = g_slist_prepend (result_simple->priv->values,
		value_data_new (extension_name, property_name, value));
}

/**
 * e_config_lookup_result_simple_add_boolean:
 * @lookup_result: an #EConfigLookupResultSimple
 * @extension_name: (nullable): extension name, or %NULL, to change property of the #ESource itself
 * @property_name: property name within the extension
 * @value: value to set
 *
 * Calls e_config_lookup_result_simple_add_value() with a #GValue initialized
 * to @value.
 *
 * Since: 3.26
 **/
void
e_config_lookup_result_simple_add_boolean (EConfigLookupResult *lookup_result,
					   const gchar *extension_name,
					   const gchar *property_name,
					   gboolean value)
{
	GValue gvalue;

	g_return_if_fail (E_IS_CONFIG_LOOKUP_RESULT_SIMPLE (lookup_result));
	g_return_if_fail (property_name != NULL);

	memset (&gvalue, 0, sizeof (GValue));
	g_value_init (&gvalue, G_TYPE_BOOLEAN);
	g_value_set_boolean (&gvalue, value);

	e_config_lookup_result_simple_add_value (lookup_result, extension_name, property_name, &gvalue);

	g_value_reset (&gvalue);
}

/**
 * e_config_lookup_result_simple_add_int:
 * @lookup_result: an #EConfigLookupResultSimple
 * @extension_name: (nullable): extension name, or %NULL, to change property of the #ESource itself
 * @property_name: property name within the extension
 * @value: value to set
 *
 * Calls e_config_lookup_result_simple_add_value() with a #GValue initialized
 * to @value.
 *
 * Since: 3.26
 **/
void
e_config_lookup_result_simple_add_int (EConfigLookupResult *lookup_result,
				       const gchar *extension_name,
				       const gchar *property_name,
				       gint value)
{
	GValue gvalue;

	g_return_if_fail (E_IS_CONFIG_LOOKUP_RESULT_SIMPLE (lookup_result));
	g_return_if_fail (property_name != NULL);

	memset (&gvalue, 0, sizeof (GValue));
	g_value_init (&gvalue, G_TYPE_INT);
	g_value_set_int (&gvalue, value);

	e_config_lookup_result_simple_add_value (lookup_result, extension_name, property_name, &gvalue);

	g_value_reset (&gvalue);
}

/**
 * e_config_lookup_result_simple_add_uint:
 * @lookup_result: an #EConfigLookupResultSimple
 * @extension_name: (nullable): extension name, or %NULL, to change property of the #ESource itself
 * @property_name: property name within the extension
 * @value: value to set
 *
 * Calls e_config_lookup_result_simple_add_value() with a #GValue initialized
 * to @value.
 *
 * Since: 3.26
 **/
void
e_config_lookup_result_simple_add_uint (EConfigLookupResult *lookup_result,
					const gchar *extension_name,
					const gchar *property_name,
					guint value)
{
	GValue gvalue;

	g_return_if_fail (E_IS_CONFIG_LOOKUP_RESULT_SIMPLE (lookup_result));
	g_return_if_fail (property_name != NULL);

	memset (&gvalue, 0, sizeof (GValue));
	g_value_init (&gvalue, G_TYPE_UINT);
	g_value_set_uint (&gvalue, value);

	e_config_lookup_result_simple_add_value (lookup_result, extension_name, property_name, &gvalue);

	g_value_reset (&gvalue);
}

/**
 * e_config_lookup_result_simple_add_int64:
 * @lookup_result: an #EConfigLookupResultSimple
 * @extension_name: (nullable): extension name, or %NULL, to change property of the #ESource itself
 * @property_name: property name within the extension
 * @value: value to set
 *
 * Calls e_config_lookup_result_simple_add_value() with a #GValue initialized
 * to @value.
 *
 * Since: 3.26
 **/
void
e_config_lookup_result_simple_add_int64 (EConfigLookupResult *lookup_result,
					 const gchar *extension_name,
					 const gchar *property_name,
					 gint64 value)
{
	GValue gvalue;

	g_return_if_fail (E_IS_CONFIG_LOOKUP_RESULT_SIMPLE (lookup_result));
	g_return_if_fail (property_name != NULL);

	memset (&gvalue, 0, sizeof (GValue));
	g_value_init (&gvalue, G_TYPE_INT64);
	g_value_set_int64 (&gvalue, value);

	e_config_lookup_result_simple_add_value (lookup_result, extension_name, property_name, &gvalue);

	g_value_reset (&gvalue);
}

/**
 * e_config_lookup_result_simple_add_uint64:
 * @lookup_result: an #EConfigLookupResultSimple
 * @extension_name: (nullable): extension name, or %NULL, to change property of the #ESource itself
 * @property_name: property name within the extension
 * @value: value to set
 *
 * Calls e_config_lookup_result_simple_add_value() with a #GValue initialized
 * to @value.
 *
 * Since: 3.26
 **/
void
e_config_lookup_result_simple_add_uint64 (EConfigLookupResult *lookup_result,
					  const gchar *extension_name,
					  const gchar *property_name,
					  guint64 value)
{
	GValue gvalue;

	g_return_if_fail (E_IS_CONFIG_LOOKUP_RESULT_SIMPLE (lookup_result));
	g_return_if_fail (property_name != NULL);

	memset (&gvalue, 0, sizeof (GValue));
	g_value_init (&gvalue, G_TYPE_UINT64);
	g_value_set_uint64 (&gvalue, value);

	e_config_lookup_result_simple_add_value (lookup_result, extension_name, property_name, &gvalue);

	g_value_reset (&gvalue);
}

/**
 * e_config_lookup_result_simple_add_double:
 * @lookup_result: an #EConfigLookupResultSimple
 * @extension_name: (nullable): extension name, or %NULL, to change property of the #ESource itself
 * @property_name: property name within the extension
 * @value: value to set
 *
 * Calls e_config_lookup_result_simple_add_value() with a #GValue initialized
 * to @value.
 *
 * Since: 3.26
 **/
void
e_config_lookup_result_simple_add_double (EConfigLookupResult *lookup_result,
					  const gchar *extension_name,
					  const gchar *property_name,
					  gdouble value)
{
	GValue gvalue;

	g_return_if_fail (E_IS_CONFIG_LOOKUP_RESULT_SIMPLE (lookup_result));
	g_return_if_fail (property_name != NULL);

	memset (&gvalue, 0, sizeof (GValue));
	g_value_init (&gvalue, G_TYPE_DOUBLE);
	g_value_set_double (&gvalue, value);

	e_config_lookup_result_simple_add_value (lookup_result, extension_name, property_name, &gvalue);

	g_value_reset (&gvalue);
}

/**
 * e_config_lookup_result_simple_add_string:
 * @lookup_result: an #EConfigLookupResultSimple
 * @extension_name: (nullable): extension name, or %NULL, to change property of the #ESource itself
 * @property_name: property name within the extension
 * @value: value to set
 *
 * Calls e_config_lookup_result_simple_add_value() with a #GValue initialized
 * to @value.
 *
 * Since: 3.26
 **/
void
e_config_lookup_result_simple_add_string (EConfigLookupResult *lookup_result,
					  const gchar *extension_name,
					  const gchar *property_name,
					  const gchar *value)
{
	GValue gvalue;

	g_return_if_fail (E_IS_CONFIG_LOOKUP_RESULT_SIMPLE (lookup_result));
	g_return_if_fail (property_name != NULL);

	memset (&gvalue, 0, sizeof (GValue));
	g_value_init (&gvalue, G_TYPE_STRING);
	g_value_set_string (&gvalue, value);

	e_config_lookup_result_simple_add_value (lookup_result, extension_name, property_name, &gvalue);

	g_value_reset (&gvalue);
}

/**
 * e_config_lookup_result_simple_add_enum:
 * @lookup_result: an #EConfigLookupResultSimple
 * @extension_name: (nullable): extension name, or %NULL, to change property of the #ESource itself
 * @property_name: property name within the extension
 * @enum_type: a #GType of the enum
 * @value: value to set
 *
 * Calls e_config_lookup_result_simple_add_value() with a #GValue initialized
 * to @value.
 *
 * Since: 3.26
 **/
void
e_config_lookup_result_simple_add_enum (EConfigLookupResult *lookup_result,
					const gchar *extension_name,
					const gchar *property_name,
					GType enum_type,
					gint value)
{
	GValue gvalue;

	g_return_if_fail (E_IS_CONFIG_LOOKUP_RESULT_SIMPLE (lookup_result));
	g_return_if_fail (property_name != NULL);

	memset (&gvalue, 0, sizeof (GValue));
	g_value_init (&gvalue, enum_type);
	g_value_set_enum (&gvalue, value);

	e_config_lookup_result_simple_add_value (lookup_result, extension_name, property_name, &gvalue);

	g_value_reset (&gvalue);
}
