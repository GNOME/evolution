/*
 * e-plugin-mono.c
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

#include "e-plugin-mono.h"

#include <sys/types.h>
#include <string.h>

#include "e-plugin-mono.h"

#include <mono/metadata/debug-helpers.h>
#include <mono/metadata/object.h>
#include <mono/metadata/appdomain.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/threads.h>
#include <mono/metadata/mono-config.h>
#include <mono/jit/jit.h>

#define E_PLUGIN_MONO_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_PLUGIN_MONO, EPluginMonoPrivate))

struct _EPluginMonoPrivate {
	MonoAssembly *assembly;
	MonoClass *class;
	MonoObject *plugin;
	GHashTable *methods;
};

static MonoDomain *domain;
static gpointer parent_class;
static GType plugin_mono_type;

static gchar *
get_xml_prop (xmlNodePtr node, const gchar *id)
{
	xmlChar *prop;
	gchar *out = NULL;

	prop = xmlGetProp (node, (xmlChar *) id);

	if (prop != NULL) {
		out = g_strdup ((gchar *) prop);
		xmlFree (prop);
	}

	return out;
}

static void
plugin_mono_finalize (GObject *object)
{
	EPluginMono *plugin_mono;

	plugin_mono = E_PLUGIN_MONO (object);

	g_free (plugin_mono->location);
	g_free (plugin_mono->handler);

	g_hash_table_destroy (plugin_mono->priv->methods);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gint
plugin_mono_construct (EPlugin *plugin, xmlNodePtr root)
{
	EPluginMono *plugin_mono;

	/* Chain up to parent's construct() method. */
	if (E_PLUGIN_CLASS (parent_class)->construct (plugin, root) == -1)
		return -1;

	plugin_mono = E_PLUGIN_MONO (plugin);
	plugin_mono->location = get_xml_prop (root, "location");
	plugin_mono->handler = get_xml_prop (root, "handler");

	return (plugin_mono->location != NULL) ? 0 : -1;
}

/*
  Two approaches:
    You can have a Evolution.Plugin implementation which has every
    callback as methods on it.  Or you can just use static methods
    for everything.

   All methods take a single (structured) argument.
*/

static gpointer
plugin_mono_invoke (EPlugin *plugin,
                    const gchar *name,
                    gpointer data)
{
	EPluginMono *plugin_mono;
	EPluginMonoPrivate *priv;
	MonoMethodDesc *d;
	MonoMethod *m;
	MonoObject *x = NULL, *res;
	gpointer *params;

	plugin_mono = E_PLUGIN_MONO (plugin);
	priv = plugin_mono->priv;

	if (!domain) {
		mono_config_parse (NULL);
		domain = mono_jit_init (plugin_mono->location);
	}

	/* We need to do this every time since we may
	 * be called from any thread for some uses. */
	mono_thread_attach (domain);

	if (priv->assembly == NULL) {
		priv->assembly = mono_domain_assembly_open (
			domain, plugin_mono->location);
		if (priv->assembly == NULL) {
			g_warning (
				"Can't load assembly '%s'",
				plugin_mono->location);
			return NULL;
		}

		if (plugin_mono->handler == NULL
		    || (priv->class = mono_class_from_name (
				mono_assembly_get_image (priv->assembly),
				"", plugin_mono->handler)) == NULL) {
		} else {
			priv->plugin = mono_object_new (domain, priv->class);
			/* could conceivably init with some context too */
			mono_runtime_object_init (priv->plugin);
		}
	}

	m = g_hash_table_lookup (priv->methods, name);
	if (m == NULL) {
		if (priv->class) {
			/* class method */
			MonoMethod* mono_method;
			gpointer iter = NULL;

			d = mono_method_desc_new (name, FALSE);
			/*if (d == NULL) {
				g_warning ("Can't create method descriptor for '%s'", name);
				return NULL;
			}*/

			while ((mono_method = mono_class_get_methods (priv->class, &iter))) {
				g_print ("\n\a Method name is : <%s>\n\a", mono_method_get_name (mono_method));
			}
//mono_class_get_method_from_name
			m = mono_class_get_method_from_name (priv->class, name, -1);
			if (m == NULL) {
				g_warning ("Can't find method callback '%s'", name);
				return NULL;
			}
		} else {
			/* static method */
			d = mono_method_desc_new (name, FALSE);
			if (d == NULL) {
				g_warning ("Can't create method descriptor for '%s'", name);
				return NULL;
			}

			m = mono_method_desc_search_in_image (d, mono_assembly_get_image (priv->assembly));
			if (m == NULL) {
				g_warning ("Can't find method callback '%s'", name);
				return NULL;
			}
		}

		g_hash_table_insert (priv->methods, g_strdup (name), m);
	}

	params = g_malloc0 (sizeof (*params)*1);
	params[0] = &data;
	res = mono_runtime_invoke (m, priv->plugin, params, &x);
	/* do i need to free params?? */

	if (x)
		mono_print_unhandled_exception (x);

	if (res) {
		gpointer *p = mono_object_unbox (res);
		return *p;
	} else
		return NULL;
}

static void
plugin_mono_class_init (EPluginMonoClass *class)
{
	GObjectClass *object_class;
	EPluginClass *plugin_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EPluginMonoPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = plugin_mono_finalize;

	plugin_class = E_PLUGIN_CLASS (class);
	plugin_class->construct = plugin_mono_construct;
	plugin_class->invoke = plugin_mono_invoke;
	plugin_class->type = "mono";
}

static void
plugin_mono_init (EPluginMono *plugin_mono)
{
	GHashTable *methods;

	methods = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) NULL);

	plugin_mono->priv = E_PLUGIN_MONO_GET_PRIVATE (plugin_mono);
	plugin_mono->priv->methods = methods;
}

GType
e_plugin_mono_get_type (void)
{
	return plugin_mono_type;
}

void
e_plugin_mono_register_type (GTypeModule *type_module)
{
	static const GTypeInfo type_info = {
		sizeof (EPluginMonoClass),
		(GBaseInitFunc) NULL,
		(GBaseFinalizeFunc) NULL,
		(GClassInitFunc) plugin_mono_class_init,
		(GClassFinalizeFunc) NULL,
		NULL,  /* class_data */
		sizeof (EPluginMono),
		0,     /* n_preallocs */
		(GInstanceInitFunc) plugin_mono_init,
		NULL   /* value_table */
	};

	plugin_mono_type = g_type_module_register_type (
		type_module, E_TYPE_PLUGIN,
		"EPluginMono", &type_info, 0);
}
