/*
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
 */

#include "evolution-config.h"

#include <sys/types.h>
#include <string.h>

#include <glib/gi18n.h>

#include <libebackend/libebackend.h>

#include "e-plugin.h"
#include "e-util-private.h"
#include "e-misc-utils.h"

/* plugin debug */
#define pd(x)
/* plugin hook debug */
#define phd(x)

/*
 * <camel-plugin
 *   class="org.gnome.camel.plugin.provider:1.0"
 *   id="org.gnome.camel.provider.imap:1.0"
 *   type="shlib"
 *   location="/opt/gnome2/lib/camel/1.0/libcamelimap.so"
 *   factory="camel_imap_provider_new">
 *  <name>imap</name>
 *  <description>IMAP4 and IMAP4v1 mail store</description>
 *  <class-data class="org.gnome.camel.plugin.provider:1.0"
 *    protocol="imap"
 *    domain="mail"
 *    flags="remote,source,storage,ssl"/>
 * </camel-plugin>
 * 
 * <camel-plugin
 *   class="org.gnome.camel.plugin.sasl:1.0"
 *   id="org.gnome.camel.sasl.plain:1.0"
 *   type="shlib"
 *   location="/opt/gnome2/lib/camel/1.0/libcamelsasl.so"
 *   factory="camel_sasl_plain_new">
 *  <name>PLAIN</name>
 *  <description>SASL PLAIN authentication mechanism</description>
 * </camel-plugin>
*/

/* EPlugin stuff */

/* global table of plugin types by pluginclass.type */
static GHashTable *ep_types;
/* global table of plugins by plugin.id */
static GHashTable *ep_plugins;
/* the list of disabled plugins from GSettings */
static GSList *ep_disabled;

/* All classes which implement EPluginHooks, by class.id */
static GHashTable *eph_types;

struct _plugin_doc {
	struct _plugin_doc *next;
	struct _plugin_doc *prev;

	gchar *filename;
	xmlDocPtr doc;
};

enum {
	EP_PROP_0,
	EP_PROP_ENABLED
};

G_DEFINE_TYPE (
	EPlugin,
	e_plugin,
	G_TYPE_OBJECT)

static gboolean
ep_check_enabled (const gchar *id)
{
	/* Return TRUE if 'id' is NOT in the disabled list. */
	return !g_slist_find_custom (ep_disabled, id, (GCompareFunc) strcmp);
}

static void
ep_set_enabled (const gchar *id,
                gint state)
{
	GSettings *settings;
	GSList *link;
	GPtrArray *array;

	/* Bail out if no change to state, when expressed as a boolean: */
	if ((state == 0) == (ep_check_enabled (id) == 0))
		return;

	if (state) {
		link = g_slist_find_custom (
			ep_disabled, id, (GCompareFunc) strcmp);
		if (link != NULL) {
			g_free (link->data);
			ep_disabled = g_slist_remove_link (ep_disabled, link);
		}
	} else
		ep_disabled = g_slist_prepend (ep_disabled, g_strdup (id));

	settings = e_util_ref_settings ("org.gnome.evolution");
	array = g_ptr_array_new ();
	for (link = ep_disabled; link != NULL; link = link->next)
		g_ptr_array_add (array, link->data);
	g_ptr_array_add (array, NULL);
	g_settings_set_strv (
		settings, "disabled-eplugins",
		(const gchar * const *) array->pdata);
	g_ptr_array_free (array, TRUE);
	g_object_unref (settings);
}

static gint
ep_construct (EPlugin *ep,
              xmlNodePtr root)
{
	xmlNodePtr node;
	gint res = -1;
	gchar *localedir;

	ep->domain = e_plugin_xml_prop (root, "domain");
	if (ep->domain
	    && (localedir = e_plugin_xml_prop (root, "localedir"))) {
#ifdef G_OS_WIN32
		gchar *mapped_localedir =
			e_util_replace_prefix (
				EVOLUTION_PREFIX,
				e_util_get_prefix (),
				localedir);
		g_free (localedir);
		localedir = mapped_localedir;
#endif
		bindtextdomain (ep->domain, localedir);
		g_free (localedir);
	}

	ep->name = e_plugin_xml_prop_domain (root, "name", ep->domain);

	node = root->children;
	while (node) {
		if (strcmp ((gchar *) node->name, "hook") == 0) {
			EPluginHook *hook;
			EPluginHookClass *type;
			gchar *class = e_plugin_xml_prop (node, "class");

			if (class == NULL) {
				g_warning (
					"Plugin '%s' load failed in '%s', "
					"missing class property for hook",
					ep->id, ep->path);
				goto fail;
			}

			if (ep->enabled
			    && eph_types != NULL
				&& (type = g_hash_table_lookup (
					eph_types, class)) != NULL) {
				g_free (class);
				hook = g_object_new (G_OBJECT_CLASS_TYPE (type), NULL);
				res = type->construct (hook, ep, node);
				if (res == -1) {
					g_warning (
						"Plugin '%s' failed to "
						"load hook", ep->name);
					g_object_unref (hook);
					goto fail;
				} else {
					ep->hooks = g_slist_append (ep->hooks, hook);
				}
			} else {
				g_free (class);
			}
		} else if (strcmp ((gchar *) node->name, "description") == 0) {
			ep->description =
				e_plugin_xml_content_domain (node, ep->domain);
		} else if (strcmp ((gchar *) node->name, "author") == 0) {
			gchar *name = e_plugin_xml_prop (node, "name");
			gchar *email = e_plugin_xml_prop (node, "email");

			if (name || email) {
				EPluginAuthor *epa = g_malloc0 (sizeof (*epa));

				epa->name = name;
				epa->email = email;
				ep->authors = g_slist_append (ep->authors, epa);
			}
		}
		node = node->next;
	}
	res = 0;
fail:
	return res;
}

static void
ep_enable (EPlugin *ep,
           gint state)
{
	GSList *iter;

	ep->enabled = state;
	for (iter = ep->hooks; iter != NULL; iter = iter->next) {
		EPluginHook *hook = iter->data;
		e_plugin_hook_enable (hook, state);
	}

	ep_set_enabled (ep->id, state);
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

	g_slist_foreach (ep->hooks, (GFunc) g_object_unref, NULL);
	g_slist_free (ep->hooks);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_plugin_parent_class)->finalize (object);
}

static void
e_plugin_class_init (EPluginClass *class)
{
	GObjectClass *object_class;

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
			"Enabled",
			"Whether the plugin is enabled",
			TRUE,
			G_PARAM_READWRITE));
}

static void
e_plugin_init (EPlugin *ep)
{
	ep->enabled = TRUE;
}

static EPlugin *
ep_load_plugin (xmlNodePtr root,
                struct _plugin_doc *pdoc)
{
	gchar *prop, *id;
	EPluginClass *class;
	EPlugin *ep;

	id = e_plugin_xml_prop (root, "id");
	if (id == NULL) {
		g_warning ("Invalid e-plugin entry in '%s': no id", pdoc->filename);
		return NULL;
	}

	if (g_hash_table_lookup (ep_plugins, id)) {
		g_warning ("Plugin '%s' already defined", id);
		g_free (id);
		return NULL;
	}

	prop = (gchar *) xmlGetProp (root, (const guchar *)"type");
	if (prop == NULL) {
		g_free (id);
		g_warning ("Invalid e-plugin entry in '%s': no type", pdoc->filename);
		return NULL;
	}

	/* If we can't find a plugin, add it to a pending list
	 * which is checked when a new type is registered. */
	class = g_hash_table_lookup (ep_types, prop);
	if (class == NULL) {
		g_free (id);
		xmlFree (prop);
		return NULL;
	}
	xmlFree (prop);

	ep = g_object_new (G_TYPE_FROM_CLASS (class), NULL);
	ep->id = id;
	ep->path = g_strdup (pdoc->filename);
	ep->enabled = ep_check_enabled (id);
	if (e_plugin_construct (ep, root) == -1)
		e_plugin_enable (ep, FALSE);
	g_hash_table_insert (ep_plugins, ep->id, ep);

	return ep;
}

static gint
ep_load (const gchar *filename,
         gint load_level)
{
	xmlDocPtr doc;
	xmlNodePtr root;
	EPlugin *ep = NULL;
	struct _plugin_doc *pdoc;

	doc = e_xml_parse_file (filename);
	if (doc == NULL)
		return -1;

	root = xmlDocGetRootElement (doc);
	if (strcmp ((gchar *) root->name, "e-plugin-list") != 0) {
		g_warning ("No <e-plugin-list> root element: %s", filename);
		xmlFreeDoc (doc);
		return -1;
	}

	pdoc = g_malloc0 (sizeof (*pdoc));
	pdoc->doc = doc;
	pdoc->filename = g_strdup (filename);

	for (root = root->children; root; root = root->next) {
		if (strcmp ((gchar *) root->name, "e-plugin") == 0) {
			gchar *plugin_load_level, *is_system_plugin;

			plugin_load_level = NULL;
			plugin_load_level = e_plugin_xml_prop (root, "load_level");
			if (plugin_load_level) {
				if ((atoi (plugin_load_level) == load_level)) {
					ep = ep_load_plugin (root, pdoc);

					if (ep && load_level == 1)
						e_plugin_invoke (
							ep, "load_plugin_type_register_function", NULL);
				}
			} else if (load_level == 2) {
				ep = ep_load_plugin (root, pdoc);
			}

			if (ep) {
				/* README: Maybe we can use load_levels to
				 * achieve the same thing.  But it may be
				 * confusing for a plugin writer. */
				is_system_plugin =
					e_plugin_xml_prop (root, "system_plugin");
				if (g_strcmp0 (is_system_plugin, "true") == 0) {
					e_plugin_enable (ep, TRUE);
					ep->flags |= E_PLUGIN_FLAGS_SYSTEM_PLUGIN;
				} else
					ep->flags &= ~E_PLUGIN_FLAGS_SYSTEM_PLUGIN;
				g_free (is_system_plugin);

				ep = NULL;
			}
		}
	}

	xmlFreeDoc (pdoc->doc);
	g_free (pdoc->filename);
	g_free (pdoc);

	return 0;
}

static void
plugin_load_subclass (GType type,
                      GHashTable *hash_table)
{
	EPluginClass *class;

	class = g_type_class_ref (type);
	g_hash_table_insert (hash_table, (gpointer) class->type, class);
}

static void
plugin_hook_load_subclass (GType type,
                           GHashTable *hash_table)
{
	EPluginHookClass *hook_class;
	EPluginHookClass *dupe_class;
	gpointer key;

	hook_class = g_type_class_ref (type);

	/* Sanity check the hook class. */
	if (hook_class->id == NULL || *hook_class->id == '\0') {
		g_warning (
			"%s has no hook ID, so skipping",
			G_OBJECT_CLASS_NAME (hook_class));
		g_type_class_unref (hook_class);
		return;
	}

	/* Check for class ID collisions. */
	dupe_class = g_hash_table_lookup (hash_table, hook_class->id);
	if (dupe_class != NULL) {
		g_warning (
			"%s and %s have the same hook "
			"ID ('%s'), so skipping %s",
			G_OBJECT_CLASS_NAME (dupe_class),
			G_OBJECT_CLASS_NAME (hook_class),
			hook_class->id,
			G_OBJECT_CLASS_NAME (hook_class));
		g_type_class_unref (hook_class);
		return;
	}

	key = (gpointer) hook_class->id;
	g_hash_table_insert (hash_table, key, hook_class);
}

static void
e_plugin_traverse_directory (const gchar *dirname,
			     gint index)
{
	GDir *dir;
	const gchar *d;

	pd (printf ("scanning plugin dir '%s'\n", dirname));

	dir = g_dir_open (dirname, 0, NULL);

	if (!dir)
		return;

	while ((d = g_dir_read_name (dir))) {
		if (g_str_has_suffix  (d, ".eplug")) {
			gchar *name;

			name = g_build_filename (dirname, d, NULL);
			ep_load (name, index);
			g_free (name);
		}
	}

	g_dir_close (dir);
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
e_plugin_load_plugins (void)
{
	GSettings *settings;
	GPtrArray *variants;
	gchar **strv;
	gint i;

	if (eph_types != NULL)
		return 0;

	ep_types = g_hash_table_new (g_str_hash, g_str_equal);
	eph_types = g_hash_table_new (g_str_hash, g_str_equal);
	ep_plugins = g_hash_table_new (g_str_hash, g_str_equal);

	/* We require that all GTypes for EPlugin and EPluginHook
	 * subclasses be registered prior to loading any plugins.
	 * It greatly simplifies the loading process. */
	e_type_traverse (
		E_TYPE_PLUGIN, (ETypeFunc)
		plugin_load_subclass, ep_types);
	e_type_traverse (
		E_TYPE_PLUGIN_HOOK, (ETypeFunc)
		plugin_hook_load_subclass, eph_types);

	settings = e_util_ref_settings ("org.gnome.evolution");
	strv = g_settings_get_strv (settings, "disabled-eplugins");
	for (i = 0, ep_disabled = NULL; strv[i] != NULL; i++)
		ep_disabled = g_slist_append (ep_disabled, g_strdup (strv[i]));
	g_strfreev (strv);
	g_object_unref (settings);

	variants = e_util_get_directory_variants (EVOLUTION_PLUGINDIR, EVOLUTION_PREFIX, TRUE);

	for (i = 0; i < 3; i++) {
		if (variants) {
			guint jj;

			for (jj = 0; jj < variants->len; jj++) {
				const gchar *dirname = g_ptr_array_index (variants, jj);

				if (dirname && *dirname)
					e_plugin_traverse_directory (dirname, i);
			}
		} else {
			e_plugin_traverse_directory (EVOLUTION_PLUGINDIR, i);
		}
	}

	if (variants)
		g_ptr_array_unref (variants);

	return 0;
}

static void
ep_list_plugin (gpointer key,
                gpointer val,
                gpointer dat)
{
	GSList **l = (GSList **) dat;

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
e_plugin_list_plugins (void)
{
	GSList *l = NULL;

	if (ep_plugins)
		g_hash_table_foreach (ep_plugins, ep_list_plugin, &l);

	return l;
}

/**
 * e_plugin_construct:
 * @plugin: an #EPlugin
 * @root: The XML root node of the sub-tree containing the plugin
 * definition.
 *
 * Helper to invoke the construct virtual method.
 *
 * Return value: The return from the construct virtual method.
 **/
gint
e_plugin_construct (EPlugin *plugin,
                    xmlNodePtr root)
{
	EPluginClass *class;

	g_return_val_if_fail (E_IS_PLUGIN (plugin), -1);

	class = E_PLUGIN_GET_CLASS (plugin);
	g_return_val_if_fail (class != NULL, -1);
	g_return_val_if_fail (class->construct != NULL, -1);

	return class->construct (plugin, root);
}

/**
 * e_plugin_invoke:
 * @plugin: an #EPlugin
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
e_plugin_invoke (EPlugin *plugin,
                 const gchar *name,
                 gpointer data)
{
	EPluginClass *class;

	g_return_val_if_fail (E_IS_PLUGIN (plugin), NULL);
	g_return_val_if_fail (name != NULL, NULL);

	/* Prevent invocation on a disabled plugin. */
	g_return_val_if_fail (plugin->enabled, NULL);

	class = E_PLUGIN_GET_CLASS (plugin);
	g_return_val_if_fail (class != NULL, NULL);
	g_return_val_if_fail (class->invoke != NULL, NULL);

	return class->invoke (plugin, name, data);
}

/**
 * e_plugin_get_symbol:
 * @plugin: an #EPlugin
 * @name: The name of the symbol to fetch. The format of this name
 * will depend on the EPlugin type and its language conventions.
 *
 * Helper to fetch a symbol name from a plugin.
 *
 * Return value: the symbol value, or %NULL if not found
 **/
gpointer
e_plugin_get_symbol (EPlugin *plugin,
                     const gchar *name)
{
	EPluginClass *class;

	g_return_val_if_fail (E_IS_PLUGIN (plugin), NULL);

	class = E_PLUGIN_GET_CLASS (plugin);
	g_return_val_if_fail (class != NULL, NULL);
	g_return_val_if_fail (class->get_symbol != NULL, NULL);

	return class->get_symbol (plugin, name);
}

/**
 * e_plugin_enable:
 * @plugin: an #EPlugin
 * @state: %TRUE to enable, %FALSE to disable
 *
 * Set the enable state of a plugin.
 *
 * THIS IS NOT FULLY IMPLEMENTED YET
 **/
void
e_plugin_enable (EPlugin *plugin,
                 gint state)
{
	EPluginClass *class;

	g_return_if_fail (E_IS_PLUGIN (plugin));

	if ((plugin->enabled == 0) == (state == 0))
		return;

	class = E_PLUGIN_GET_CLASS (plugin);
	g_return_if_fail (class != NULL);
	g_return_if_fail (class->enable != NULL);

	class->enable (plugin, state);

	g_object_notify (G_OBJECT (plugin), "enabled");
}

/**
 * e_plugin_get_configure_widget
 * @plugin: an #EPlugin
 *
 * Plugin itself should have implemented "e_plugin_lib_get_configure_widget"
 * function * of prototype EPluginLibGetConfigureWidgetFunc.
 *
 * Returns: Configure widget or %NULL
 **/
GtkWidget *
e_plugin_get_configure_widget (EPlugin *plugin)
{
	EPluginClass *class;

	g_return_val_if_fail (E_IS_PLUGIN (plugin), NULL);

	class = E_PLUGIN_GET_CLASS (plugin);
	g_return_val_if_fail (class != NULL, NULL);

	if (class->get_configure_widget == NULL)
		return NULL;

	return class->get_configure_widget (plugin);
}

/**
 * e_plugin_xml_prop:
 * @node: An XML node.
 * @id: The name of the property to retrieve.
 *
 * A static helper function to look up a property on an XML node, and
 * ensure it is allocated in GLib system memory.
 *
 * Return value: The property, allocated in GLib memory, or NULL if no
 * such property exists.
 **/
gchar *
e_plugin_xml_prop (xmlNodePtr node,
                   const gchar *id)
{
	xmlChar *xml_prop;
	gchar *glib_prop = NULL;

	xml_prop = xmlGetProp (node, (xmlChar *) id);

	if (xml_prop != NULL) {
		glib_prop = g_strdup ((gchar *) xml_prop);
		xmlFree (xml_prop);
	}

	return glib_prop;
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
e_plugin_xml_prop_domain (xmlNodePtr node,
                          const gchar *id,
                          const gchar *domain)
{
	gchar *p, *out;

	p = (gchar *) xmlGetProp (node, (const guchar *) id);
	if (p == NULL)
		return NULL;

	out = g_strdup (dgettext (domain, p));
	xmlFree (p);

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
e_plugin_xml_int (xmlNodePtr node,
                  const gchar *id,
                  gint def)
{
	gchar *p = (gchar *) xmlGetProp (node, (const guchar *) id);

	if (p)
		return atoi (p);
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
e_plugin_xml_content (xmlNodePtr node)
{
	gchar *p = (gchar *) xmlNodeGetContent (node);

	if (g_mem_is_system_malloc ()) {
		return p;
	} else {
		gchar * out = g_strdup (p);

		if (p)
			xmlFree (p);
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
e_plugin_xml_content_domain (xmlNodePtr node,
                             const gchar *domain)
{
	gchar *p, *out;

	p = (gchar *) xmlNodeGetContent (node);
	if (p == NULL)
		return NULL;

	out = g_strdup (dgettext (domain, p));
	xmlFree (p);

	return out;
}

/* ********************************************************************** */

G_DEFINE_TYPE (
	EPluginHook,
	e_plugin_hook,
	G_TYPE_OBJECT)

static gint
plugin_hook_construct (EPluginHook *plugin_hook,
                       EPlugin *plugin,
                       xmlNodePtr root)
{
	plugin_hook->plugin = plugin;

	return 0;
}

static void
plugin_hook_enable (EPluginHook *plugin_hook,
                    gint state)
{
	/* NOOP */
}

static void
e_plugin_hook_class_init (EPluginHookClass *class)
{
	class->construct = plugin_hook_construct;
	class->enable = plugin_hook_enable;
}

static void
e_plugin_hook_init (EPluginHook *hook)
{
}

/**
 * e_plugin_hook_enable:
 * @plugin_hook:
 * @state:
 *
 * Set the enabled state of the plugin hook.  This is called by the
 * plugin code.
 *
 * THIS IS NOT FULY IMEPLEMENTED YET
 **/
void
e_plugin_hook_enable (EPluginHook *plugin_hook,
                      gint state)
{
	EPluginHookClass *class;

	g_return_if_fail (E_IS_PLUGIN_HOOK (plugin_hook));

	class = E_PLUGIN_HOOK_GET_CLASS (plugin_hook);
	g_return_if_fail (class != NULL);
	g_return_if_fail (class->enable != NULL);

	class->enable (plugin_hook, state);
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
e_plugin_hook_mask (xmlNodePtr root,
                    const EPluginHookTargetKey *map,
                    const gchar *prop)
{
	gchar *val, *p, *start, c;
	guint32 mask = 0;

	val = (gchar *) xmlGetProp (root, (const guchar *) prop);
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

			for (i = 0; map[i].key; i++) {
				if (!strcmp (map[i].key, start)) {
					mask |= map[i].value;
					break;
				}
			}
		}
		*p++ = c;
	} while (c);

	xmlFree (val);

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
e_plugin_hook_id (xmlNodePtr root,
                  const EPluginHookTargetKey *map,
                  const gchar *prop)
{
	gchar *val;
	gint i;

	val = (gchar *) xmlGetProp (root, (const guchar *) prop);
	if (val == NULL)
		return ~0;

	for (i = 0; map[i].key; i++) {
		if (!strcmp (map[i].key, val)) {
			xmlFree (val);
			return map[i].value;
		}
	}

	xmlFree (val);

	return ~0;
}
