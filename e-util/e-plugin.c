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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 */

#include <config.h>

#include <sys/types.h>
#include <string.h>

#include <glib/gi18n.h>

#include <gconf/gconf-client.h>

#include <libedataserver/e-msgport.h>
#include <libedataserver/e-data-server-util.h>
#include <libedataserver/e-xml-utils.h>

#include "e-plugin.h"
#include "e-util-private.h"

/* plugin debug */
#define pd(x)
/* plugin hook debug */
#define phd(x)

/*
<camel-plugin
  class="org.gnome.camel.plugin.provider:1.0"
  id="org.gnome.camel.provider.imap:1.0"
  type="shlib"
  location="/opt/gnome2/lib/camel/1.0/libcamelimap.so"
  factory="camel_imap_provider_new">
 <name>imap</name>
 <description>IMAP4 and IMAP4v1 mail store</description>
 <class-data class="org.gnome.camel.plugin.provider:1.0"
   protocol="imap"
   domain="mail"
   flags="remote,source,storage,ssl"/>
</camel-plugin>

<camel-plugin
  class="org.gnome.camel.plugin.sasl:1.0"
  id="org.gnome.camel.sasl.plain:1.0"
  type="shlib"
  location="/opt/gnome2/lib/camel/1.0/libcamelsasl.so"
  factory="camel_sasl_plain_new">
 <name>PLAIN</name>
 <description>SASL PLAIN authentication mechanism</description>
</camel-plugin>
*/

/* EPlugin stuff */
static gpointer ep_parent_class;

/* global table of plugin types by pluginclass.type */
static GHashTable *ep_types;
/* plugin load path */
static GSList *ep_path;
/* global table of plugins by plugin.id */
static GHashTable *ep_plugins;
/* a table of GSLists of plugins by hook class for hooks not loadable yet */
static GHashTable *ep_plugins_pending_hooks;
/* list of all cached xml docs:struct _plugin_doc's */
static EDList ep_plugin_docs = E_DLIST_INITIALISER(ep_plugin_docs);
/* gconf client */
static GConfClient *ep_gconf;
/* the list of disabled plugins from gconf */
static GSList *ep_disabled;

/* All classes which implement EPluginHooks, by class.id */
static GHashTable *eph_types;

struct _plugin_doc {
	struct _plugin_doc *next;
	struct _plugin_doc *prev;

	gchar *filename;
	xmlDocPtr doc;

	GSList *plugin_hooks;	/* EPlugin objects with pending hooks */
	GSList *plugins;	/* xmlNodePtr's of plugins with unknown type (mono,etc) */
};

enum {
	EP_PROP_0,
	EP_PROP_ENABLED
};

static gboolean
ep_check_enabled (const gchar *id)
{
	/* Return TRUE if 'id' is NOT in the disabled list. */
	return !g_slist_find_custom (ep_disabled, id, (GCompareFunc) strcmp);
}

static void
ep_set_enabled (const gchar *id, gint state)
{
	/* Bail out if no change to state, when expressed as a boolean: */
	if ((state == 0) == (ep_check_enabled(id) == 0))
		return;

	if (state) {
		GSList *link;

		link = g_slist_find_custom (
			ep_disabled, id, (GCompareFunc) strcmp);
		if (link != NULL) {
			g_free (link->data);
			ep_disabled = g_slist_remove_link (ep_disabled, link);
		}
	} else
		ep_disabled = g_slist_prepend (ep_disabled, g_strdup (id));

	gconf_client_set_list(
		ep_gconf, "/apps/evolution/eplugin/disabled",
		GCONF_VALUE_STRING, ep_disabled, NULL);
}

static gint
ep_construct (EPlugin *ep, xmlNodePtr root)
{
	xmlNodePtr node;
	gint res = -1;
	gchar *localedir;

	ep->domain = e_plugin_xml_prop(root, "domain");
	if (ep->domain
	    && (localedir = e_plugin_xml_prop(root, "localedir"))) {
#ifdef G_OS_WIN32
		gchar *mapped_localedir =
			e_util_replace_prefix (EVOLUTION_PREFIX,
					       e_util_get_prefix (),
					       localedir);
		g_free (localedir);
		localedir = mapped_localedir;
#endif
		bindtextdomain(ep->domain, localedir);
		g_free(localedir);
	}

	ep->name = e_plugin_xml_prop_domain(root, "name", ep->domain);

	pd(printf("creating plugin '%s' '%s'\n", ep->name?ep->name:"un-named", ep->id));

	node = root->children;
	while (node) {
		if (strcmp((gchar *)node->name, "hook") == 0) {
			struct _EPluginHook *hook;
			EPluginHookClass *type;
			gchar *class = e_plugin_xml_prop(node, "class");

			if (class == NULL) {
				g_warning("Plugin '%s' load failed in '%s', missing class property for hook", ep->id, ep->path);
				goto fail;
			}

			if (ep->enabled
			    && eph_types != NULL
			    && (type = g_hash_table_lookup(eph_types, class)) != NULL) {
				g_free(class);
				hook = g_object_new(G_OBJECT_CLASS_TYPE(type), NULL);
				res = type->construct(hook, ep, node);
				if (res == -1) {
					g_warning("Plugin '%s' failed to load hook", ep->name);
					g_object_unref(hook);
					goto fail;
				} else {
					ep->hooks = g_slist_append(ep->hooks, hook);
				}
			} else {
				gpointer l, oldclass;

				if (ep_plugins_pending_hooks == NULL)
					ep_plugins_pending_hooks = g_hash_table_new(g_str_hash, g_str_equal);
				if (!g_hash_table_lookup_extended (ep_plugins_pending_hooks, class, &oldclass, &l)) {
					oldclass = class;
					l = NULL;
				}
				else {
					g_free(class);
				}
				l = g_slist_prepend (l, ep);
				g_hash_table_insert (ep_plugins_pending_hooks, oldclass, l);
				ep->hooks_pending = g_slist_prepend (ep->hooks_pending, node);
			}
		} else if (strcmp((gchar *)node->name, "description") == 0) {
			ep->description = e_plugin_xml_content_domain(node, ep->domain);
		} else if (strcmp((gchar *)node->name, "author") == 0) {
			gchar *name = e_plugin_xml_prop(node, "name");
			gchar *email = e_plugin_xml_prop(node, "email");

			if (name || email) {
				EPluginAuthor *epa = g_malloc0(sizeof(*epa));

				epa->name = name;
				epa->email = email;
				ep->authors = g_slist_append(ep->authors, epa);
			}
		}
		node = node->next;
	}
	res = 0;
fail:
	return res;
}

static void
ep_enable (EPlugin *ep, gint state)
{
	GSList *iter;

	ep->enabled = state;
	for (iter = ep->hooks; iter != NULL; iter = iter->next) {
		EPluginHook *hook = iter->data;
		e_plugin_hook_enable (hook, state);
	}

	ep_set_enabled(ep->id, state);
}

static void
ep_set_property (GObject *object,
                 guint property_id,
                 const GValue *value,
                 GParamSpec *pspec)
{
	switch (property_id) {
		case EP_PROP_ENABLED:
			e_plugin_enable (
				E_PLUGIN (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
ep_get_property (GObject *object,
                 guint property_id,
                 GValue *value,
                 GParamSpec *pspec)
{
	EPlugin *ep = E_PLUGIN (object);

	switch (property_id) {
		case EP_PROP_ENABLED:
			g_value_set_boolean (value, ep->enabled);
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
ep_finalize (GObject *object)
{
	EPlugin *ep = E_PLUGIN (object);

	g_free (ep->id);
	g_free (ep->description);
	g_free (ep->name);
	g_free (ep->domain);
	g_slist_free (ep->hooks_pending);

	g_slist_foreach (ep->hooks, (GFunc) g_object_unref, NULL);
	g_slist_free (ep->hooks);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (ep_parent_class)->finalize (object);
}

static void
ep_class_init (EPluginClass *class)
{
	GObjectClass *object_class;

	ep_parent_class = g_type_class_peek_parent (class);

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = ep_set_property;
	object_class->get_property = ep_get_property;
	object_class->finalize = ep_finalize;

	class->construct = ep_construct;
	class->enable = ep_enable;

	g_object_class_install_property (
		object_class,
		EP_PROP_ENABLED,
		g_param_spec_boolean (
			"enabled",
			_("Enabled"),
			_("Whether the plugin is enabled"),
			TRUE,
			G_PARAM_READWRITE));
}

static void
ep_init (EPlugin *ep)
{
	ep->enabled = TRUE;
}

/**
 * e_plugin_get_type:
 *
 * Standard GObject type function.  This is only an abstract class, so
 * you can only use this to subclass EPlugin.
 *
 * Return value: The type.
 **/
GType
e_plugin_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		gchar *path, *col, *p;

		static const GTypeInfo type_info = {
			sizeof (EPluginClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) ep_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EPlugin),
			0,     /* n_preallocs */
			(GInstanceInitFunc) ep_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			G_TYPE_OBJECT, "EPlugin", &type_info, 0);

		/* Add paths in the environment variable or default global
		 * and user specific paths */
		path = g_strdup(g_getenv("EVOLUTION_PLUGIN_PATH"));
		if (path == NULL) {
			/* Add the global path */
			e_plugin_add_load_path(EVOLUTION_PLUGINDIR);

			path = g_build_filename(g_get_home_dir(), ".eplugins", NULL);
		}

		p = path;
		while ((col = strchr(p, G_SEARCHPATH_SEPARATOR))) {
			*col++ = 0;
			e_plugin_add_load_path(p);
			p = col;
		}
		e_plugin_add_load_path(p);
		g_free(path);
	}

	return type;
}

static EPlugin *
ep_load_plugin(xmlNodePtr root, struct _plugin_doc *pdoc)
{
	gchar *prop, *id;
	EPluginClass *class;
	EPlugin *ep;

	id = e_plugin_xml_prop(root, "id");
	if (id == NULL) {
		g_warning("Invalid e-plugin entry in '%s': no id", pdoc->filename);
		return NULL;
	}

	if (g_hash_table_lookup(ep_plugins, id)) {
		g_warning("Plugin '%s' already defined", id);
		g_free(id);
		return NULL;
	}

	prop = (gchar *)xmlGetProp(root, (const guchar *)"type");
	if (prop == NULL) {
		g_free(id);
		g_warning("Invalid e-plugin entry in '%s': no type", pdoc->filename);
		return NULL;
	}

	/* If we can't find a plugin, add it to a pending list which is checked when a new type is registered */
	class = g_hash_table_lookup(ep_types, prop);
	if (class == NULL) {
		pd(printf("Delaying loading of plugin '%s' unknown type '%s'\n", id, prop));
		g_free(id);
		xmlFree(prop);
		pdoc->plugins = g_slist_prepend(pdoc->plugins, root);
		return NULL;
	}
	xmlFree(prop);

	ep = g_object_new(G_TYPE_FROM_CLASS(class), NULL);
	ep->id = id;
	ep->path = g_strdup(pdoc->filename);
	ep->enabled = ep_check_enabled(id);
	if (e_plugin_construct(ep, root) == -1)
		e_plugin_enable(ep, FALSE);
	g_hash_table_insert(ep_plugins, ep->id, ep);

	return ep;
}

static gint
ep_load(const gchar *filename, gint load_level)
{
	xmlDocPtr doc;
	xmlNodePtr root;
	gint res = -1;
	EPlugin *ep = NULL;
	gint cache = FALSE;
	struct _plugin_doc *pdoc;

	doc = e_xml_parse_file (filename);
	if (doc == NULL)
		return -1;

	root = xmlDocGetRootElement(doc);
	if (strcmp((gchar *)root->name, "e-plugin-list") != 0) {
		g_warning("No <e-plugin-list> root element: %s", filename);
		xmlFreeDoc(doc);
		return -1;
	}

	pdoc = g_malloc0(sizeof(*pdoc));
	pdoc->doc = doc;
	pdoc->filename = g_strdup(filename);

	for (root = root->children; root; root = root->next) {
		if (strcmp((gchar *)root->name, "e-plugin") == 0) {
			gchar *plugin_load_level, *is_system_plugin;

			plugin_load_level = NULL;
			plugin_load_level = e_plugin_xml_prop (root, "load_level");
			if (plugin_load_level) {
				if ((atoi (plugin_load_level) == load_level) ) {
					ep = ep_load_plugin(root, pdoc);

					if (ep) {
						if (load_level == 1)
							e_plugin_invoke (ep, "load_plugin_type_register_function", NULL);
					}
				}
			} else if (load_level == 2) {
				ep = ep_load_plugin(root, pdoc);
			}

			if (ep) {
				pd(printf ("\nloading plugin [%s] at load_level [%d]\n", ep->name, load_level));

				/* README: May be we can use load_levels to achieve the same thing.
				   But it may be confusing for a plugin writer */
				is_system_plugin = e_plugin_xml_prop (root, "system_plugin");
				if (is_system_plugin && !strcmp (is_system_plugin, "true")) {
					e_plugin_enable (ep, TRUE);
					ep->flags |= E_PLUGIN_FLAGS_SYSTEM_PLUGIN;
				} else
					ep->flags &= ~E_PLUGIN_FLAGS_SYSTEM_PLUGIN;
				g_free (is_system_plugin);

				pdoc->plugin_hooks = g_slist_prepend(pdoc->plugin_hooks, ep);
				cache |= (ep->hooks_pending != NULL);
				ep = NULL;
			}
			cache |= pdoc->plugins != NULL;
		}
	}

	res = 0;

	if (cache) {
		pd(printf("Caching plugin description '%s' for unknown future hooks\n", filename));
		e_dlist_addtail(&ep_plugin_docs, (EDListNode *)pdoc);
	} else {
		pd(printf("freeing plugin description '%s', nothing uses it\n", filename));
		xmlFreeDoc(pdoc->doc);
		g_free(pdoc->filename);
		g_free(pdoc);
	}

	return res;
}

/* This loads a hook that was pending on a given plugin but the type wasn't registered yet */
/* This works in conjunction with ep_construct and e_plugin_hook_register_type to make sure
   everything works nicely together.  Apparently. */
static gint
ep_load_pending(EPlugin *ep, EPluginHookClass *type)
{
	gint res = 0;
	GSList *l, *p;

	phd(printf("New hook type registered '%s', loading pending hooks on plugin '%s'\n", type->id, ep->id));

	l = ep->hooks_pending;
	p = NULL;
	while (l) {
		GSList *n = l->next;
		xmlNodePtr node = l->data;
		gchar *class = (gchar *)xmlGetProp(node, (const guchar *)"class");
		EPluginHook *hook;

		phd(printf(" checking pending hook '%s'\n", class?class:"<unknown>"));

		if (class) {
			if (strcmp(class, type->id) == 0) {
				hook = g_object_new(G_OBJECT_CLASS_TYPE(type), NULL);

				/* Don't bother loading hooks for plugins that are not anyway enabled */
				if (ep->enabled) {
					res = type->construct(hook, ep, node);
					if (res == -1) {
						g_warning("Plugin '%s' failed to load hook '%s'", ep->name, type->id);
						g_object_unref(hook);
					} else {
						ep->hooks = g_slist_append(ep->hooks, hook);
					}
				}

				if (p)
					p->next = n;
				else
					ep->hooks_pending = n;
				g_slist_free_1(l);
				l = p;
			}

			xmlFree(class);
		}

		p = l;
		l = n;
	}

	return res;
}

/**
 * e_plugin_add_load_path:
 * @path: The path to add to search for plugins.
 *
 * Add a path to be searched when e_plugin_load_plugins() is called.
 * By default the system plugin directory and ~/.eplugins is used as
 * the search path unless overriden by the environmental variable
 * %EVOLUTION_PLUGIN_PATH.
 *
 * %EVOLUTION_PLUGIN_PATH is a : separated list of paths to search for
 * plugin definitions in order.
 *
 * Plugin definitions are XML files ending in the extension ".eplug".
 **/
void
e_plugin_add_load_path(const gchar *path)
{
	ep_path = g_slist_append(ep_path, g_strdup(path));
}

/**
 * e_plugin_load_plugins:
 *
 * Scan the search path, looking for plugin definitions, and load them
 * into memory.
 *
 * Return value: Returns -1 if an error occurred.
 **/
gint
e_plugin_load_plugins(void)
{
	GSList *l;
	gint i;

	if (ep_types == NULL) {
		g_warning("no plugin types defined");
		return 0;
	}

	for (i=0; i < 3; i++) {
		for (l = ep_path;l;l = g_slist_next(l)) {
			GDir *dir;
			const gchar *d;
			gchar *path = l->data;

			pd(printf("scanning plugin dir '%s'\n", path));

			dir = g_dir_open(path, 0, NULL);
			if (dir == NULL) {
				/*g_warning("Could not find plugin path: %s", path);*/
				continue;
			}

			while ( (d = g_dir_read_name(dir)) ) {
				if (g_str_has_suffix  (d, ".eplug")) {
					gchar * name = g_build_filename(path, d, NULL);

					ep_load(name, i);
					g_free(name);
				}
			}

			g_dir_close(dir);
		}
	}

	return 0;
}

/**
 * e_plugin_register_type:
 * @type: The GObject type of the plugin loader.
 *
 * Register a new plugin type with the plugin system.  Each type must
 * subclass EPlugin and must override the type member of the
 * EPluginClass with a unique name.
 **/
void
e_plugin_register_type(GType type)
{
	EPluginClass *class;
	struct _plugin_doc *pdoc, *ndoc;

	if (ep_types == NULL) {
		ep_types = g_hash_table_new(g_str_hash, g_str_equal);
		ep_plugins = g_hash_table_new(g_str_hash, g_str_equal);
		/* TODO: notify listening */
		ep_gconf = gconf_client_get_default();
		ep_disabled = gconf_client_get_list(ep_gconf, "/apps/evolution/eplugin/disabled", GCONF_VALUE_STRING, NULL);
	}

	class = g_type_class_ref(type);

	pd(printf("register plugin type '%s'\n", class->type));

	g_hash_table_insert(ep_types, (gpointer)class->type, class);

	/* check for pending plugins */
	pdoc = (struct _plugin_doc *)ep_plugin_docs.head;
	ndoc = pdoc->next;
	while (ndoc) {
		if (pdoc->plugins) {
			GSList *l, *add = NULL;

			for (l=pdoc->plugins;l;l=g_slist_next(l)) {
				xmlNodePtr root = l->data;
				gchar *prop_type;

				prop_type = (gchar *)xmlGetProp(root, (const guchar *)"type");
				if (!strcmp(prop_type, class->type))
					add = g_slist_append(add, l->data);
				xmlFree(prop_type);
			}

			for (l=add;l;l=g_slist_next(l)) {
				xmlNodePtr root = l->data;
				EPlugin *ep;

				pdoc->plugins = g_slist_remove(pdoc->plugins, root);
				ep = ep_load_plugin(root, pdoc);
				if (ep)
					pdoc->plugin_hooks = g_slist_prepend(pdoc->plugin_hooks, ep);
				/* TODO: garbage collect plugin doc? */
			}

			g_slist_free(add);
		}

		pdoc = ndoc;
		ndoc = ndoc->next;
	}
}

static void
ep_list_plugin(gpointer key, gpointer val, gpointer dat)
{
	GSList **l = (GSList **)dat;

	*l = g_slist_prepend(*l, g_object_ref(val));
}

/**
 * e_plugin_list_plugins: List all plugins.
 *
 * Static class method to retrieve a list of all current plugins.  They
 * are listed in no particular order.
 *
 * Return value: A GSList of all plugins, they must be
 * g_object_unref'd and the list freed.
 **/
GSList *
e_plugin_list_plugins(void)
{
	GSList *l = NULL;

	if (ep_plugins)
		g_hash_table_foreach(ep_plugins, ep_list_plugin, &l);

	return l;
}

/**
 * e_plugin_construct:
 * @ep: An EPlugin derived object.
 * @root: The XML root node of the sub-tree containing the plugin
 * definition.
 *
 * Helper to invoke the construct virtual method.
 *
 * Return value: The return from the construct virtual method.
 **/
gint
e_plugin_construct(EPlugin *ep, xmlNodePtr root)
{
	EPluginClass *class;

	g_return_val_if_fail (E_IS_PLUGIN (ep), -1);

	class = E_PLUGIN_GET_CLASS (ep);
	g_return_val_if_fail (class->construct != NULL, -1);

	return class->construct (ep, root);
}

/**
 * e_plugin_invoke:
 * @ep:
 * @name: The name of the function to invoke. The format of this name
 * will depend on the EPlugin type and its language conventions.
 * @data: The argument to the function. Its actual type depends on
 * the hook on which the function resides. It is up to the called
 * function to get this right.
 *
 * Helper to invoke the invoke virtual method.
 *
 * Return value: The return of the plugin invocation.
 **/
gpointer
e_plugin_invoke(EPlugin *ep, const gchar *name, gpointer data)
{
	EPluginClass *class;

	g_return_val_if_fail (E_IS_PLUGIN (ep), NULL);
	g_return_val_if_fail (name != NULL, NULL);

	/* Prevent invocation on a disabled plugin. */
	g_return_val_if_fail (ep->enabled, NULL);

	class = E_PLUGIN_GET_CLASS (ep);
	g_return_val_if_fail (class->invoke != NULL, NULL);

	return class->invoke (ep, name, data);
}

/**
 * e_plugin_get_symbol:
 * @ep: an #EPlugin
 * @name: The name of the symbol to fetch. The format of this name
 * will depend on the EPlugin type and its language conventions.
 *
 * Helper to fetch a symbol name from a plugin.
 *
 * Return value: the symbol value, or %NULL if not found
 **/
gpointer
e_plugin_get_symbol(EPlugin *ep, const gchar *name)
{
	EPluginClass *class;

	g_return_val_if_fail (E_IS_PLUGIN (ep), NULL);

	class = E_PLUGIN_GET_CLASS (ep);
	g_return_val_if_fail (class->get_symbol != NULL, NULL);

	return class->get_symbol (ep, name);
}

/**
 * e_plugin_enable:
 * @ep:
 * @state:
 *
 * Set the enable state of a plugin.
 *
 * THIS IS NOT FULLY IMPLEMENTED YET
 **/
void
e_plugin_enable(EPlugin *ep, gint state)
{
	EPluginClass *class;

	g_return_if_fail (E_IS_PLUGIN (ep));

	if ((ep->enabled == 0) == (state == 0))
		return;

	class = E_PLUGIN_GET_CLASS (ep);
	g_return_if_fail (class->enable != NULL);

	class->enable (ep, state);
	g_object_notify (G_OBJECT (ep), "enabled");
}

/**
 * e_plugin_get_configure_widget
 * @ep: an #EPlugin
 *
 * Plugin itself should have implemented "e_plugin_lib_get_configure_widget"
 * function * of prototype EPluginLibGetConfigureWidgetFunc.
 *
 * Returns: Configure widget or %NULL
 **/
GtkWidget *
e_plugin_get_configure_widget (EPlugin *ep)
{
	EPluginClass *class;

	g_return_val_if_fail (E_IS_PLUGIN (ep), NULL);

	class = E_PLUGIN_GET_CLASS (ep);
	if (class->get_configure_widget == NULL)
		return NULL;

	return class->get_configure_widget (ep);
}

/**
 * e_plugin_xml_prop:
 * @node: An XML node.
 * @id: The name of the property to retrieve.
 *
 * A static helper function to look up a property on an XML node, and
 * ensure it is allocated in GLib system memory.  If GLib isn't using
 * the system malloc then it must copy the property value.
 *
 * Return value: The property, allocated in GLib memory, or NULL if no
 * such property exists.
 **/
gchar *
e_plugin_xml_prop(xmlNodePtr node, const gchar *id)
{
	gchar *p = (gchar *)xmlGetProp(node, (const guchar *)id);

	if (g_mem_is_system_malloc()) {
		return p;
	} else {
		gchar * out = g_strdup(p);

		if (p)
			xmlFree(p);
		return out;
	}
}

/**
 * e_plugin_xml_prop_domain:
 * @node: An XML node.
 * @id: The name of the property to retrieve.
 * @domain: The translation domain for this string.
 *
 * A static helper function to look up a property on an XML node, and
 * translate it based on @domain.
 *
 * Return value: The property, allocated in GLib memory, or NULL if no
 * such property exists.
 **/
gchar *
e_plugin_xml_prop_domain(xmlNodePtr node, const gchar *id, const gchar *domain)
{
	gchar *p, *out;

	p = (gchar *)xmlGetProp(node, (const guchar *)id);
	if (p == NULL)
		return NULL;

	out = g_strdup(dgettext(domain, p));
	xmlFree(p);

	return out;
}

/**
 * e_plugin_xml_int:
 * @node: An XML node.
 * @id: The name of the property to retrieve.
 * @def: A default value if the property doesn't exist.  Can be used
 * to determine if the property isn't set.
 *
 * A static helper function to look up a property on an XML node as an
 * integer.  If the property doesn't exist, then @def is returned as a
 * default value instead.
 *
 * Return value: The value if set, or @def if not.
 **/
gint
e_plugin_xml_int(xmlNodePtr node, const gchar *id, gint def)
{
	gchar *p = (gchar *)xmlGetProp(node, (const guchar *)id);

	if (p)
		return atoi(p);
	else
		return def;
}

/**
 * e_plugin_xml_content:
 * @node:
 *
 * A static helper function to retrieve the entire textual content of
 * an XML node, and ensure it is allocated in GLib system memory.  If
 * GLib isn't using the system malloc them it must copy the content.
 *
 * Return value: The node content, allocated in GLib memory.
 **/
gchar *
e_plugin_xml_content(xmlNodePtr node)
{
	gchar *p = (gchar *)xmlNodeGetContent(node);

	if (g_mem_is_system_malloc()) {
		return p;
	} else {
		gchar * out = g_strdup(p);

		if (p)
			xmlFree(p);
		return out;
	}
}

/**
 * e_plugin_xml_content_domain:
 * @node:
 * @domain:
 *
 * A static helper function to retrieve the entire textual content of
 * an XML node, and ensure it is allocated in GLib system memory.  If
 * GLib isn't using the system malloc them it must copy the content.
 *
 * Return value: The node content, allocated in GLib memory.
 **/
gchar *
e_plugin_xml_content_domain(xmlNodePtr node, const gchar *domain)
{
	gchar *p, *out;

	p = (gchar *)xmlNodeGetContent(node);
	if (p == NULL)
		return NULL;

	out = g_strdup(dgettext(domain, p));
	xmlFree(p);

	return out;
}

/* ********************************************************************** */

static gpointer epl_parent_class;

/* TODO:
   We need some way to manage lifecycle.
   We need some way to manage state.

   Maybe just the g module init method will do, or we could add
   another which returns context.

   There is also the question of per-instance context, e.g. for config
   pages.
*/

static GList *missing_symbols = NULL;

static gint
epl_loadmodule(EPlugin *ep, gboolean fatal)
{
	EPluginLib *epl = E_PLUGIN_LIB (ep);
	EPluginLibEnableFunc enable;

	if (epl->module != NULL)
		return 0;

	if ((epl->module = g_module_open(epl->location, 0)) == NULL) {
		if (fatal) {
			g_warning("can't load plugin '%s': %s", epl->location, g_module_error());
			ep->enabled = FALSE;
		} else
			missing_symbols = g_list_prepend (missing_symbols, g_object_ref (ep));
		return -1;
	}

	if (g_module_symbol(epl->module, "e_plugin_lib_enable", (gpointer)&enable)) {
		if (enable(epl, TRUE) != 0) {
			ep->enabled = FALSE;
			g_module_close(epl->module);
			epl->module = NULL;
			return -1;
		}
	}

	return 0;
}

void
e_plugin_load_plugins_with_missing_symbols (void)
{
	GList *list = missing_symbols;

	while (list) {
		EPlugin *ep = list->data;
		epl_loadmodule (ep, TRUE);
		g_object_unref (ep);
		list = g_list_next (list);
	}

	g_list_free (missing_symbols);
	missing_symbols = NULL;
}

static gpointer
epl_invoke(EPlugin *ep, const gchar *name, gpointer data)
{
	EPluginLib *epl = E_PLUGIN_LIB (ep);
	EPluginLibFunc cb;

	if (!ep->enabled) {
		g_warning("trying to invoke '%s' on disabled plugin '%s'", name, ep->id);
		return NULL;
	}

	if (epl_loadmodule(ep, FALSE) != 0)
		return NULL;

	if (!g_module_symbol(epl->module, name, (gpointer)&cb)) {
		g_warning("Cannot resolve symbol '%s' in plugin '%s' (not exported?)", name, epl->location);
		return NULL;
	}

	return cb(epl, data);
}

static gpointer
epl_get_symbol(EPlugin *ep, const gchar *name)
{
	EPluginLib *epl = E_PLUGIN_LIB (ep);
	gpointer symbol;

	if (epl_loadmodule (ep, FALSE) != 0)
		return NULL;

	if (!g_module_symbol (epl->module, name, &symbol))
		return NULL;

	return symbol;
}

static gint
epl_construct(EPlugin *ep, xmlNodePtr root)
{
	EPluginLib *epl = E_PLUGIN_LIB (ep);

	if (E_PLUGIN_CLASS (epl_parent_class)->construct (ep, root) == -1)
		return -1;

	epl->location = e_plugin_xml_prop(root, "location");

	if (epl->location == NULL) {
		g_warning("Library plugin '%s' has no location", ep->id);
		return -1;
	}
#ifdef G_OS_WIN32
	{
		gchar *mapped_location =
			e_util_replace_prefix (EVOLUTION_PREFIX,
					       e_util_get_prefix (),
					       epl->location);
		g_free (epl->location);
		epl->location = mapped_location;
	}
#endif
	/* If we're enabled, check for the load-on-startup property */
	if (ep->enabled) {
		xmlChar *tmp;

		tmp = xmlGetProp(root, (const guchar *)"load-on-startup");
		if (tmp) {
			if (strcmp ((const gchar *)tmp, "after-ui") == 0) {
				missing_symbols = g_list_prepend (missing_symbols, g_object_ref (ep));
			} else {
				if (epl_loadmodule(ep, FALSE) != 0) {
					xmlFree(tmp);
					return -1;
				}
			}
			xmlFree(tmp);
		}
	}

	return 0;
}

static GtkWidget *
epl_get_configure_widget (EPlugin *ep)
{
	EPluginLib *epl = E_PLUGIN_LIB (ep);
	EPluginLibGetConfigureWidgetFunc get_configure_widget;

	pd (printf ("\n epl_get_configure_widget \n"));

	if (epl_loadmodule (ep, FALSE) != 0) {
		pd (printf ("\n epl_loadmodule  \n"));
		return NULL;
	}

	if (g_module_symbol (epl->module, "e_plugin_lib_get_configure_widget", (gpointer)&get_configure_widget)) {
		pd (printf ("\n g_module_symbol is loaded\n"));
		return (GtkWidget*) get_configure_widget (epl);
	}
	return NULL;
}

static void
epl_enable(EPlugin *ep, gint state)
{
	EPluginLib *epl = E_PLUGIN_LIB (ep);
	EPluginLibEnableFunc enable;

	E_PLUGIN_CLASS (epl_parent_class)->enable (ep, state);

	/* if we're disabling and it isn't loaded, nothing to do */
	if (!state && epl->module == NULL)
		return;

	/* this will noop if we're disabling since we tested it above */
	if (epl_loadmodule(ep, FALSE) != 0)
		return;

	if (g_module_symbol(epl->module, "e_plugin_lib_enable", (gpointer)&enable)) {
		if (enable(epl, state) != 0)
			return;
	}
#if 0
	if (!state) {
		g_module_close(epl->module);
		epl->module = NULL;
	}
#endif
}

static void
epl_finalize (GObject *object)
{
	EPluginLib *epl = E_PLUGIN_LIB (object);

	g_free (epl->location);

	if (epl->module)
		g_module_close (epl->module);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (epl_parent_class)->finalize (object);
}

static void
epl_class_init (EPluginClass *class)
{
	GObjectClass *object_class;

	epl_parent_class = g_type_class_peek_parent (class);

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = epl_finalize;

	class->construct = epl_construct;
	class->invoke = epl_invoke;
	class->get_symbol = epl_get_symbol;
	class->enable = epl_enable;
	class->get_configure_widget = epl_get_configure_widget;
	class->type = "shlib";
}

/**
 * e_plugin_lib_get_type:
 *
 * Standard GObject function to retrieve the EPluginLib type.  Use to
 * register the type with the plugin system if you want to use shared
 * library plugins.
 *
 * Return value: The EPluginLib type.
 **/
GType
e_plugin_lib_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EPluginLibClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) epl_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EPluginLib),
			0,     /* n_preallocs */
			(GInstanceInitFunc) NULL,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			e_plugin_get_type (), "EPluginLib", &type_info, 0);
	}

	return type;
}

/* ********************************************************************** */

static gpointer eph_parent_class;

static gint
eph_construct(EPluginHook *eph, EPlugin *ep, xmlNodePtr root)
{
	eph->plugin = ep;

	return 0;
}

static void
eph_enable(EPluginHook *eph, gint state)
{
	/* NOOP */
}

static void
eph_class_init(EPluginHookClass *class)
{
	eph_parent_class = g_type_class_peek_parent (class);

	class->construct = eph_construct;
	class->enable = eph_enable;
}

/**
 * e_plugin_hook_get_type:
 *
 * Standard GObject function to retrieve the EPluginHook type.  Since
 * EPluginHook is an abstract class, this is only used to subclass it.
 *
 * Return value: The EPluginHook type.
 **/
GType
e_plugin_hook_get_type(void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EPluginHookClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) eph_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EPluginHook),
			0,     /* n_preallocs */
			(GInstanceInitFunc) NULL,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			G_TYPE_OBJECT, "EPluginHook", &type_info, 0);
	}

	return type;
}

/**
 * e_plugin_hook_enable: Set hook enabled state.
 * @eph:
 * @state:
 *
 * Set the enabled state of the plugin hook.  This is called by the
 * plugin code.
 *
 * THIS IS NOT FULY IMEPLEMENTED YET
 **/
void
e_plugin_hook_enable (EPluginHook *eph, gint state)
{
	EPluginHookClass *class;

	g_return_if_fail (E_IS_PLUGIN_HOOK (eph));

	class = E_PLUGIN_HOOK_GET_CLASS (eph);
	g_return_if_fail (class->enable != NULL);

	class->enable (eph, state);
}

/**
 * e_plugin_hook_register_type:
 * @type:
 *
 * Register a new plugin hook type with the plugin system.  Each type
 * must subclass EPluginHook and must override the id member of the
 * EPluginHookClass with a unique identification string.
 **/
void
e_plugin_hook_register_type(GType type)
{
	EPluginHookClass *klass, *oldklass;
	GSList *l;

	gpointer plugins; /* GSList */
	gpointer class;

	if (eph_types == NULL)
		eph_types = g_hash_table_new(g_str_hash, g_str_equal);

	klass = g_type_class_ref(type);

	oldklass = g_hash_table_lookup(eph_types, (gpointer)klass->id);
	if (oldklass == klass) {
		g_type_class_unref(klass);
		return;
	} else if (oldklass != NULL) {
		g_warning("Trying to re-register hook type '%s'", klass->id);
		return;
	}

	phd(printf("register plugin hook type '%s'\n", klass->id));
	g_hash_table_insert(eph_types, (gpointer)klass->id, klass);

	/* if we've already loaded a plugin that needed this hook but it didn't exist, re-load it now */

	if (ep_plugins_pending_hooks
	    && g_hash_table_lookup_extended (ep_plugins_pending_hooks, klass->id, &class, &plugins)) {
		struct _plugin_doc *pdoc, *ndoc;

		g_hash_table_remove (ep_plugins_pending_hooks, class);
		g_free (class);
		for (l = plugins; l; l = g_slist_next(l)) {
			EPlugin *ep = l->data;

			ep_load_pending (ep, klass);
		}
		g_slist_free (plugins);

		/* See if we can now garbage collect the xml definition since its been fully loaded */

		/* This is all because libxml doesn't refcount! */

		pdoc = (struct _plugin_doc *)ep_plugin_docs.head;
		ndoc = pdoc->next;
		while (ndoc) {
			if (pdoc->doc) {
				gint cache = pdoc->plugins != NULL;

				for (l=pdoc->plugin_hooks;!cache && l;l=g_slist_next(l))
					cache |= (((EPlugin *)l->data)->hooks_pending != NULL);

				if (!cache) {
					pd(printf("Gargabe collecting plugin description '%s'\n", pdoc->filename));
					e_dlist_remove((EDListNode *)pdoc);
					xmlFreeDoc(pdoc->doc);
					g_free(pdoc->filename);
					g_free(pdoc);
				}
			}

			pdoc = ndoc;
			ndoc = ndoc->next;
		}
	}
}

/**
 * e_plugin_hook_mask:
 * @root: An XML node.
 * @map: A zero-fill terminated array of EPluginHookTargeKeys used to
 * map a string with a bit value.
 * @prop: The property name.
 *
 * This is a static helper function which looks up a property @prop on
 * the XML node @root, and then uses the @map table to convert it into
 * a bitmask.  The property value is a comma separated list of
 * enumeration strings which are indexed into the @map table.
 *
 * Return value: A bitmask representing the inclusive-or of all of the
 * integer values of the corresponding string id's stored in the @map.
 **/
guint32
e_plugin_hook_mask(xmlNodePtr root, const struct _EPluginHookTargetKey *map, const gchar *prop)
{
	gchar *val, *p, *start, c;
	guint32 mask = 0;

	val = (gchar *)xmlGetProp(root, (const guchar *)prop);
	if (val == NULL)
		return 0;

	p = val;
	do {
		start = p;
		while (*p && *p != ',')
			p++;
		c = *p;
		*p = 0;
		if (start != p) {
			gint i;

			for (i=0;map[i].key;i++) {
				if (!strcmp(map[i].key, start)) {
					mask |= map[i].value;
					break;
				}
			}
		}
		*p++ = c;
	} while (c);

	xmlFree(val);

	return mask;
}

/**
 * e_plugin_hook_id:
 * @root:
 * @map:
 * @prop:
 *
 * This is a static helper function which looks up a property @prop on
 * the XML node @root, and then uses the @map table to convert it into
 * an integer.
 *
 * This is used as a helper wherever you need to represent an
 * enumerated value in the XML.
 *
 * Return value: If the @prop value is in @map, then the corresponding
 * integer value, if not, then ~0.
 **/
guint32
e_plugin_hook_id(xmlNodePtr root, const struct _EPluginHookTargetKey *map, const gchar *prop)
{
	gchar *val;
	gint i;

	val = (gchar *)xmlGetProp(root, (const guchar *)prop);
	if (val == NULL)
		return ~0;

	for (i=0;map[i].key;i++) {
		if (!strcmp(map[i].key, val)) {
			xmlFree(val);
			return map[i].value;
		}
	}

	xmlFree(val);

	return ~0;
}

/* ********************************************************************** */
/* Plugin plugin */

static gpointer epth_parent_class;

static gint
epth_load_plugin(gpointer d)
{
	EPluginHook *eph = d;
	EPluginTypeHook *epth = d;
	GType type;

	epth->idle = 0;

	type = GPOINTER_TO_UINT(e_plugin_invoke(eph->plugin, epth->get_type, eph->plugin));
	if (type != 0)
		e_plugin_register_type(type);

	return FALSE;
}

static gint
epth_construct(EPluginHook *eph, EPlugin *ep, xmlNodePtr root)
{
	EPluginTypeHook *epth = E_PLUGIN_TYPE_HOOK (eph);
	xmlNodePtr node;

	phd(printf("loading plugin hook\n"));

	if (((EPluginHookClass *)epth_parent_class)->construct(eph, ep, root) == -1)
		return -1;

	node = root->children;
	while (node) {
		if (strcmp((gchar *)node->name, "plugin-type") == 0) {
			epth->get_type = e_plugin_xml_prop(node, "get-type");
			/* We need to run this in an idle handler,
			 * since at this point the parent EPlugin wont
			 * be fully initialised ... darn */
			if (epth->get_type)
				epth->idle = g_idle_add(epth_load_plugin, epth);
			else
				g_warning("Plugin type plugin missing get-type callback");
		}
		node = node->next;
	}

	eph->plugin = ep;

	return 0;
}

static void
epth_finalize (GObject *object)
{
	EPluginTypeHook *epth = E_PLUGIN_TYPE_HOOK (object);

	if (epth->idle != 0)
		g_source_remove (epth->idle);

	g_free (epth->get_type);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (epth_parent_class)->finalize (object);
}

static void
epth_class_init (EPluginTypeHookClass *class)
{
	GObjectClass *object_class;
	EPluginHookClass *hook_class;

	epth_parent_class = g_type_class_peek_parent (class);

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = epth_finalize;

	hook_class = E_PLUGIN_HOOK_CLASS (class);
	hook_class->construct = epth_construct;
	hook_class->id = "org.gnome.evolution.plugin.type:1.0";
}

/**
 * e_plugin_type_hook_get_type:
 *
 * Get the type for the plugin plugin hook.
 *
 * Return value: The type of the plugin type hook.
 **/
GType
e_plugin_type_hook_get_type(void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EPluginTypeHookClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) epth_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EPluginTypeHook),
			0,     /* n_preallocs */
			(GInstanceInitFunc) NULL,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			E_TYPE_PLUGIN_HOOK, "EPluginTypeHook", &type_info, 0);
	}

	return type;
}
