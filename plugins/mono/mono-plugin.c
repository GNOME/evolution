/*
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
 * Authors:
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <sys/types.h>
#include <string.h>

#include "mono-plugin.h"

#include <mono/metadata/debug-helpers.h>
#include <mono/metadata/object.h>
#include <mono/metadata/appdomain.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/threads.h>
#include <mono/metadata/mono-config.h>
#include <mono/jit/jit.h>

#define d(x) (x)

static MonoDomain *domain;

/* ********************************************************************** */
static gpointer epm_parent_class;

typedef struct _EPluginMonoPrivate {
	MonoAssembly *assembly;
	MonoClass *klass;
	MonoObject *plugin;
	GHashTable *methods;
} EPluginMonoPrivate;

#define epm ((EPluginMono *)ep)

gpointer  load_plugin_type_register_function (gpointer a, gpointer b);

static gchar *
get_xml_prop(xmlNodePtr node, const gchar *id)
{
	gchar *p = xmlGetProp(node, id);
	gchar *out = NULL;

	if (p) {
		out = g_strdup(p);
		xmlFree(p);
	}

	return out;
}

/*
  Two approaches:
   You can have a Evolution.Plugin implementation which has every callback as methods on it.
   Or you can just use static methods for everything.

   All methods take a single (structured) argument.
*/

static gpointer
epm_invoke(EPlugin *ep, const gchar *name, gpointer data)
{
	EPluginMonoPrivate *p = epm->priv;
	MonoMethodDesc *d;
	MonoMethod *m;
	MonoObject *x = NULL, *res;
	gpointer *params;

	g_print ("\n\a epm_invoke in mono-plugin.c in mono plugin loader is called \n\a");

	/* we need to do this every time since we may be called from any thread for some uses */
	mono_thread_attach(domain);

	if (p->assembly == NULL) {
		p->assembly = mono_domain_assembly_open(domain, epm->location);
		if (p->assembly == NULL) {
			g_warning("can't load assembly '%s'", epm->location);
			return NULL;
		}

		if (epm->handler == NULL
		    || (p->klass = mono_class_from_name(mono_assembly_get_image(p->assembly), "", epm->handler)) == NULL) {
			d(printf("Using static callbacks only"));
		} else {
			p->plugin = mono_object_new(domain, p->klass);
			/* could conceivably init with some context too */
			mono_runtime_object_init(p->plugin);
		}
	}

	m = g_hash_table_lookup(p->methods, name);
	if (m == NULL) {
		if (p->klass) {
			d(printf("looking up method '%s' in class\n", name));
			/* class method */

			d = mono_method_desc_new(name, FALSE);
			/*if (d == NULL) {
				g_warning("Can't create method descriptor for '%s'", name);
				return NULL;
			}*/

			gpointer iter = NULL;
			MonoMethod* mono_method;

			d(printf ("\n\a About to get methods in klass\n\a"));

			while ((mono_method = mono_class_get_methods (p->klass, &iter))) {
				g_print ("\n\a Method name is : <%s>\n\a", mono_method_get_name(mono_method));
			}
			d(printf ("\n\a Got methods in klass \n\a"));
//mono_class_get_method_from_name
			m = mono_class_get_method_from_name (p->klass, name, -1);
			if (m == NULL) {
				g_warning("Can't find method callback '%s'", name);
				return NULL;
			}
		} else {
			d(printf("looking up static method '%s'\n", name));
			/* static method */
			d = mono_method_desc_new(name, FALSE);
			if (d == NULL) {
				g_warning("Can't create method descriptor for '%s'", name);
				return NULL;
			}

			m = mono_method_desc_search_in_image(d, mono_assembly_get_image(p->assembly));
			if (m == NULL) {
				g_warning("Can't find method callback '%s'", name);
				return NULL;
			}
		}

		g_hash_table_insert(p->methods, g_strdup(name), m);
	}

	params = g_malloc0(sizeof(*params)*1);
	params[0] = &data;
	res = mono_runtime_invoke(m, p->plugin, params, &x);
	/* do i need to free params?? */

	if (x)
		mono_print_unhandled_exception(x);

	if (res) {
		gpointer *p = mono_object_unbox(res);
		d(printf("mono method returned '%p' %ld\n", *p, (glong)*p));
		return *p;
	} else
		return NULL;
}

static gint
epm_construct(EPlugin *ep, xmlNodePtr root)
{
	if (((EPluginClass *)epm_parent_class)->construct(ep, root) == -1)
		return -1;

	epm->location = get_xml_prop(root, "location");
	epm->handler = get_xml_prop(root, "handler");

	if (epm->location == NULL)
		return -1;

	return 0;
}

static void
epm_finalise(GObject *o)
{
	EPlugin *ep = (EPlugin *)o;
	EPluginMonoPrivate *p = epm->priv;

	g_free(epm->location);
	g_free(epm->handler);

	g_hash_table_destroy(p->methods);

	g_free(epm->priv);

	((GObjectClass *)epm_parent_class)->finalize(o);
}

static void
epm_class_init(EPluginClass *klass)
{
	((GObjectClass *)klass)->finalize = epm_finalise;
	klass->construct = epm_construct;
	klass->invoke = epm_invoke;
	klass->type = "mono";
}

static void
epm_init(GObject *o)
{
	EPlugin *ep = (EPlugin *)o;

	epm->priv = g_malloc0(sizeof(*epm->priv));
	epm->priv->methods = g_hash_table_new_full(
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) NULL);
}

gpointer
load_plugin_type_register_function (gpointer a, gpointer b)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof(EPluginMonoClass), NULL, NULL, (GClassInitFunc) epm_class_init, NULL, NULL,
			sizeof(EPluginMono), 0, (GInstanceInitFunc) epm_init,
		};

		epm_parent_class = g_type_class_ref(e_plugin_get_type());
		type = g_type_register_static(e_plugin_get_type(), "EPluginMono", &info, 0);
		e_plugin_register_type (type);
		d(printf("\nType EPluginMono registered from the mono-plugin-loader\n"));
		domain = mono_jit_init("Evolution");
		mono_thread_attach(domain);
	}

	return GUINT_TO_POINTER(type);
}
