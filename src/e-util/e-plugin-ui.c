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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 */

#include "evolution-config.h"

#include <string.h>

#include "e-ui-manager.h"

#include "e-plugin-ui.h"

#define E_PLUGIN_UI_DEFAULT_FUNC	"e_plugin_ui_init"
#define E_PLUGIN_UI_HOOK_CLASS_ID	"org.gnome.evolution.ui:1.0"

struct _EPluginUIHookPrivate {

	/* Table of EUIManager ID's to callback function names.
	 *
	 * This stores the optional "callback" attribute in the <ui-manager>
	 * element.  If not specified, it defaults to "e_plugin_ui_init".
	 *
	 * This is useful when extending the UI of multiple EUIManager IDs
	 * from a single plugin.
	 *
	 * For example:
	 *
	 *     <hook class="org.gnome.evolution.ui:1.0">
	 *       <ui-manager id="org.gnome.evolution.foo" callback="init_foo"/>
	 *       <ui-manager id="org.gnome.evolution.bar" callback="init_bar"/>
	 *     </hook>
	 *
	 * Results in:
	 *
	 *     g_hash_table_insert (
	 *             callbacks, "org.gnome.evolution.foo", "init_foo");
	 *
	 *     g_hash_table_insert (
	 *             callbacks, "org.gnome.evolution.bar", "init_bar");
	 */
	GHashTable *callbacks;

	/* The registry is the heart of EPluginUI.  It tracks EUIManager
	 * instances.
	 */
	GPtrArray *registry;
};

G_DEFINE_TYPE_WITH_PRIVATE (EPluginUIHook, e_plugin_ui_hook, E_TYPE_PLUGIN_HOOK)

static void
plugin_ui_hook_unregister_manager (EPluginUIHook *hook,
                                   EUIManager *ui_manager)
{
	/* Note: Manager may already be finalized. */
	g_ptr_array_remove (hook->priv->registry, ui_manager);
}

static void
plugin_ui_hook_register_manager (EPluginUIHook *hook,
                                 EUIManager *ui_manager,
                                 const gchar *id,
                                 gpointer user_data)
{
	EPlugin *plugin;
	EPluginUIInitFunc func;
	const gchar *func_name;

	plugin = ((EPluginHook *) hook)->plugin;

	if (!g_hash_table_contains (hook->priv->callbacks, id))
		return;

	func_name = g_hash_table_lookup (hook->priv->callbacks, id);

	if (func_name == NULL)
		func_name = E_PLUGIN_UI_DEFAULT_FUNC;

	func = e_plugin_get_symbol (plugin, func_name);

	if (func == NULL) {
		g_critical (
			"Plugin \"%s\" is missing a function named %s()",
			plugin->name, func_name);
		return;
	}

	/* Pass the manager and user_data to the plugin's callback function.
	 * The plugin should install whatever EUIAction-s and UI definitions
	 * are neccessary to implement the actions in its UI definition. */
	if (!func (ui_manager, user_data))
		return;

	g_object_weak_ref (
		G_OBJECT (ui_manager), (GWeakNotify)
		plugin_ui_hook_unregister_manager, hook);

	if (!g_ptr_array_find (hook->priv->registry, ui_manager, NULL))
		g_ptr_array_add (hook->priv->registry, ui_manager);
}

static void
plugin_ui_hook_finalize (GObject *object)
{
	EPluginUIHook *self = E_PLUGIN_UI_HOOK (object);
	guint ii;

	/* Remove weak reference callbacks to EUIManager-s. */
	for (ii = 0; ii < self->priv->registry->len; ii++) {
		EUIManager *ui_manager = g_ptr_array_index (self->priv->registry, ii);
		g_object_weak_unref (
			G_OBJECT (ui_manager), (GWeakNotify)
			plugin_ui_hook_unregister_manager, object);
	}

	g_hash_table_destroy (self->priv->callbacks);
	g_ptr_array_free (self->priv->registry, TRUE);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_plugin_ui_hook_parent_class)->finalize (object);
}

static gint
plugin_ui_hook_construct (EPluginHook *hook,
                          EPlugin *plugin,
                          xmlNodePtr node)
{
	EPluginUIHook *self = E_PLUGIN_UI_HOOK (hook);

	/* XXX The EPlugin should be a property of EPluginHookClass.
	 *     Then it could be passed directly to g_object_new() and
	 *     we wouldn't have to chain up here. */

	/* Chain up to parent's construct() method. */
	E_PLUGIN_HOOK_CLASS (e_plugin_ui_hook_parent_class)->construct (hook, plugin, node);

	for (node = xmlFirstElementChild (node); node != NULL; node = xmlNextElementSibling (node)) {
		xmlNodePtr child;
		xmlBufferPtr buffer;
		GString *content;
		const gchar *temp;
		gchar *callback;
		gchar *id;

		if (strcmp ((gchar *) node->name, "ui-manager") != 0)
			continue;

		id = e_plugin_xml_prop (node, "id");
		if (id == NULL) {
			g_warning ("<ui-manager> requires 'id' property");
			continue;
		}

		callback = e_plugin_xml_prop (node, "callback");
		g_hash_table_insert (self->priv->callbacks, id, callback);

		content = g_string_sized_new (1024);

		/* Extract the XML content below <ui-manager>. There cannot be any since 3.56,
		   thus claim a warning if there is */
		buffer = xmlBufferCreate ();
		for (child = node->children; child != NULL; child = child->next) {
			xmlNodeDump (buffer, node->doc, child, 2, 1);
			temp = (const gchar *) xmlBufferContent (buffer);
			g_string_append (content, temp);
		}

		g_string_replace (content, "\r" ,"", 0);
		g_string_replace (content, "\n" ,"", 0);
		g_string_replace (content, " " ,"", 0);

		if (content->len) {
			g_warning ("UI definitions cannot be part of .eplug files anymore. Add your UI with actions in the e_plugin_ui_init() instead. Plugin: %s",
				plugin ? plugin->id : "Unknown");
		}

		g_string_free (content, TRUE);
		xmlBufferFree (buffer);
	}

	return 0;
}

static void
plugin_ui_hook_enable (EPluginHook *hook,
                       gint state)
{
}

static void
e_plugin_ui_hook_class_init (EPluginUIHookClass *class)
{
	GObjectClass *object_class;
	EPluginHookClass *plugin_hook_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = plugin_ui_hook_finalize;

	plugin_hook_class = E_PLUGIN_HOOK_CLASS (class);
	plugin_hook_class->id = E_PLUGIN_UI_HOOK_CLASS_ID;
	plugin_hook_class->construct = plugin_ui_hook_construct;
	plugin_hook_class->enable = plugin_ui_hook_enable;
}

static void
e_plugin_ui_hook_init (EPluginUIHook *hook)
{
	GHashTable *callbacks;

	callbacks = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_free);

	hook->priv = e_plugin_ui_hook_get_instance_private (hook);
	hook->priv->callbacks = callbacks;
	hook->priv->registry = g_ptr_array_new ();
}

void
e_plugin_ui_register_manager (EUIManager *ui_manager,
                              const gchar *id,
                              gpointer user_data)
{
	GSList *plugin_list;

	g_return_if_fail (E_IS_UI_MANAGER (ui_manager));
	g_return_if_fail (id != NULL);

	/* Loop over all installed plugins. */
	plugin_list = e_plugin_list_plugins ();
	while (plugin_list != NULL) {
		EPlugin *plugin = plugin_list->data;
		GSList *iter;

		plugin_list = g_slist_remove (plugin_list, plugin);

		/* Look for hooks of type EPluginUIHook. */
		for (iter = plugin->hooks; iter != NULL; iter = iter->next) {
			EPluginUIHook *hook = iter->data;

			if (!E_IS_PLUGIN_UI_HOOK (hook))
				continue;

			/* Register the manager with the hook. */
			plugin_ui_hook_register_manager (hook, ui_manager, id, user_data);
		}

		g_object_unref (plugin);
	}
}
