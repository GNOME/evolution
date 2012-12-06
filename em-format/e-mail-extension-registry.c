/*
 * e-mail-extension-registry.c
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
 */

#include <glib-object.h>

#include "e-mail-extension-registry.h"
#include "e-mail-format-extensions.h"
#include <libebackend/libebackend.h>
#include <camel/camel.h>

#include <glib-object.h>

#include <string.h>

#define E_MAIL_EXTENSION_REGISTRY_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_EXTENSION_REGISTRY, EMailExtensionRegistryPrivate))

struct _EMailExtensionRegistryPrivate {
	GHashTable *table;
};

G_DEFINE_ABSTRACT_TYPE (
	EMailExtensionRegistry,
	e_mail_extension_registry,
	G_TYPE_OBJECT)

/**
 * EMailExtensionRegistry:
 *
 * The #EMailExtensionRegistry is an abstract class representing a registry
 * for #EMailExtension<!-//>s.
 *
 * #EMailParser and #EMailFormatter both have internally a registry object
 * based on the #EMailExtensionRegistry.
 *
 * One extension can registry itself for more mime-types.
 */

static void
mail_extension_registry_finalize (GObject *object)
{
	EMailExtensionRegistry *reg = E_MAIL_EXTENSION_REGISTRY (object);

	if (reg->priv->table) {
		g_hash_table_destroy (reg->priv->table);
		reg->priv->table = NULL;
	}

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_mail_extension_registry_parent_class)->
		finalize (object);
}

void
e_mail_extension_registry_class_init (EMailExtensionRegistryClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (EMailExtensionRegistryPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = mail_extension_registry_finalize;
}

static void
destroy_queue (GQueue *queue)
{
	g_queue_free_full (queue, g_object_unref);
}

void
e_mail_extension_registry_init (EMailExtensionRegistry *reg)
{
	reg->priv = E_MAIL_EXTENSION_REGISTRY_GET_PRIVATE (reg);

	reg->priv->table = g_hash_table_new_full (
		g_str_hash, g_str_equal, NULL, (GDestroyNotify) destroy_queue);
}

/**
 * e_mail_extension_registry_add_extension:
 * @reg: an #EMailExtensionRegistry
 * @mime_types: a %NULL-terminated array of MIME types
 * @extension_type: the #GType of the extension being added
 *
 * Creates an instance of @extension_type and registers the instance for
 * all provided MIME types.
 */
void
e_mail_extension_registry_add_extension (EMailExtensionRegistry *reg,
                                         const gchar **mime_types,
                                         GType extension_type)
{
	GObject *extension;
	gint ii;

	g_return_if_fail (E_IS_MAIL_EXTENSION_REGISTRY (reg));
	g_return_if_fail (mime_types != NULL);
	g_return_if_fail (extension_type != G_TYPE_INVALID);

	extension = g_object_new (extension_type, NULL);

	for (ii = 0; mime_types[ii] != NULL; ii++) {
		GQueue *queue;

		queue = g_hash_table_lookup (
			reg->priv->table, mime_types[ii]);
		if (queue == NULL) {
			queue = g_queue_new ();
			g_hash_table_insert (
				reg->priv->table,
				(gpointer) mime_types[ii],
				queue);
		}

		g_queue_push_head (queue, g_object_ref (extension));

		if (camel_debug ("emformat:registry")) {
			printf (
				"Added extension '%s' for type '%s'\n",
				g_type_name (extension_type),
				mime_types[ii]);
		}
	}

	g_object_unref (extension);
}

/**
 * e_mail_extension_registry_get_for_mime_type:
 * @reg: An #EMailExtensionRegistry
 * @mime_type: A string with mime-type to look up
 *
 * Tries to lookup list of #EMailExtension<!-//>s that has registryed themselves
 * as handlers for the @mime_type.
 *
 * Return value: Returns #GQueue of #EMailExtension<!-//>s or %NULL when there
 * are no extension registryed for given @mime_type.
 */
GQueue *
e_mail_extension_registry_get_for_mime_type (EMailExtensionRegistry *reg,
                                             const gchar *mime_type)
{
	g_return_val_if_fail (E_IS_MAIL_EXTENSION_REGISTRY (reg), NULL);
	g_return_val_if_fail (mime_type && *mime_type, NULL);

	return g_hash_table_lookup (reg->priv->table, mime_type);
}

/**
 * e_mail_extension_registry_get_fallback:
 * @reg: An #EMailExtensionRegistry
 * @mime_type: A string with mime-type whose fallback to look up
 *
 * Tries to lookup fallback parsers for given mime type. For instance, for
 * multipart/alternative, it will try to lookup multipart/ * parser.
 *
 * Return Value: Returns #QGueue of #EMailExtension<!-//>>s or %NULL when there
 * are no extensions registryed for the fallback type.
 */
GQueue *
e_mail_extension_registry_get_fallback (EMailExtensionRegistry *reg,
                                        const gchar *mime_type)
{
	gchar *s, *type;
	gsize len;
	GQueue *parsers;

	g_return_val_if_fail (E_IS_MAIL_EXTENSION_REGISTRY (reg), NULL);
	g_return_val_if_fail (mime_type && *mime_type, NULL);

	s = strchr (mime_type, '/');
	if (!s)
		return NULL;

	len = s - mime_type;

	s = g_alloca (len);
	strncpy (s, mime_type, len);
	type = g_ascii_strdown (s, len);
	s = g_strdup_printf ("%s/*", type);

	parsers = g_hash_table_lookup (reg->priv->table, s);

	g_free (type);
	g_free (s);

	return parsers;
}

/******************************************************************************/

static void e_mail_parser_extension_registry_extensible_interface_init (EExtensibleInterface *iface);

G_DEFINE_TYPE_WITH_CODE (
	EMailParserExtensionRegistry,
	e_mail_parser_extension_registry,
	E_TYPE_MAIL_EXTENSION_REGISTRY,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_EXTENSIBLE,
		e_mail_parser_extension_registry_extensible_interface_init));

static void
e_mail_parser_extension_registry_init (EMailParserExtensionRegistry *parser_ereg)
{
}

static void
e_mail_parser_extension_registry_class_init (EMailParserExtensionRegistryClass *class)
{
}

static void
e_mail_parser_extension_registry_extensible_interface_init (EExtensibleInterface *interface)
{

}

/******************************************************************************/

static void e_mail_formatter_extension_registry_extensible_interface_init (EExtensibleInterface *iface);

G_DEFINE_TYPE_WITH_CODE (
	EMailFormatterExtensionRegistry,
	e_mail_formatter_extension_registry,
	E_TYPE_MAIL_EXTENSION_REGISTRY,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_EXTENSIBLE,
		e_mail_formatter_extension_registry_extensible_interface_init));

static void
e_mail_formatter_extension_registry_init (EMailFormatterExtensionRegistry *formatter_ereg)
{

}

static void
e_mail_formatter_extension_registry_class_init (EMailFormatterExtensionRegistryClass *class)
{
}

static void
e_mail_formatter_extension_registry_extensible_interface_init (EExtensibleInterface *interface)
{

}
