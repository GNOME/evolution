/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-uri-schema-registry.c
 *
 * Copyright (C) 2001  Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-uri-schema-registry.h"

#include <gal/util/e-util.h>


#define PARENT_TYPE gtk_object_get_type ()
static GtkObjectClass *parent_class = NULL;

struct _SchemaHandler {
	char *schema;
	EvolutionShellComponentClient *component;
};
typedef struct _SchemaHandler SchemaHandler;

struct _EUriSchemaRegistryPrivate {
	GHashTable *schema_to_handler;
};


/* SchemaHandler.  */

static SchemaHandler *
schema_handler_new (const char *schema,
		    EvolutionShellComponentClient *component)
{
	SchemaHandler *handler;

	handler = g_new (SchemaHandler, 1);
	handler->schema    = g_strdup (schema);
	handler->component = component;

	bonobo_object_ref (BONOBO_OBJECT (component));

	return handler;
}

static void
schema_handler_free (SchemaHandler *handler)
{
	g_free (handler->schema);
	bonobo_object_unref (BONOBO_OBJECT (handler->component));

	g_free (handler);
}


static void
schema_to_handler_destroy_foreach_callback (void *key,
					    void *value,
					    void *data)
{
	schema_handler_free ((SchemaHandler *) value);
}


/* GtkObject methods.  */

static void
impl_finalize (GObject *object)
{
	EUriSchemaRegistry *registry;
	EUriSchemaRegistryPrivate *priv;

	registry = E_URI_SCHEMA_REGISTRY (object);
	priv = registry->priv;

	g_hash_table_foreach (priv->schema_to_handler, schema_to_handler_destroy_foreach_callback, NULL);
	g_hash_table_destroy (priv->schema_to_handler);
	g_free (priv);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}


static void
class_init (GObjectClass *object_class)
{
	parent_class = gtk_type_class (PARENT_TYPE);

	object_class->finalize = impl_finalize;
}

static void
init (EUriSchemaRegistry *uri_schema_registry)
{
	EUriSchemaRegistryPrivate *priv;

	priv = g_new (EUriSchemaRegistryPrivate, 1);
	priv->schema_to_handler = g_hash_table_new (g_str_hash, g_str_equal);

	uri_schema_registry->priv = priv;

	GTK_OBJECT_UNSET_FLAGS (uri_schema_registry, GTK_FLOATING);
}


EUriSchemaRegistry *
e_uri_schema_registry_new (void)
{
	EUriSchemaRegistry *registry;

	registry = g_object_new (e_uri_schema_registry_get_type (), NULL);

	return registry;
}


void
e_uri_schema_registry_set_handler_for_schema (EUriSchemaRegistry *registry,
					      const char *schema,
					      EvolutionShellComponentClient *shell_component)
{
	EUriSchemaRegistryPrivate *priv;
	SchemaHandler *existing_handler;
	SchemaHandler *new_handler;

	g_return_if_fail (registry != NULL);
	g_return_if_fail (E_IS_URI_SCHEMA_REGISTRY (registry));
	g_return_if_fail (schema != NULL);
	g_return_if_fail (shell_component == NULL || EVOLUTION_IS_SHELL_COMPONENT_CLIENT (shell_component));

	priv = registry->priv;

	existing_handler = g_hash_table_lookup (priv->schema_to_handler, schema);
	if (existing_handler != NULL) {
		g_hash_table_remove (priv->schema_to_handler, existing_handler->schema);
		schema_handler_free (existing_handler);
	}

	new_handler = schema_handler_new (schema, shell_component);
	g_hash_table_insert (priv->schema_to_handler, new_handler->schema, new_handler);
}

EvolutionShellComponentClient *
e_uri_schema_registry_get_handler_for_schema (EUriSchemaRegistry *registry,
					      const char *schema)
{
	EUriSchemaRegistryPrivate *priv;
	const SchemaHandler *handler;

	g_return_val_if_fail (registry != NULL, NULL);
	g_return_val_if_fail (E_IS_URI_SCHEMA_REGISTRY (registry), NULL);
	g_return_val_if_fail (schema != NULL, NULL);

	priv = registry->priv;

	handler = g_hash_table_lookup (priv->schema_to_handler, schema);
	if (handler == NULL)
		return NULL;

	return handler->component;
}


E_MAKE_TYPE (e_uri_schema_registry, "EUriSchemaRegistry", EUriSchemaRegistry, class_init, init, PARENT_TYPE)
