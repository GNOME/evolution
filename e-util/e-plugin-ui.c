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

#include "e-plugin-ui.h"

#include <string.h>

#define E_PLUGIN_UI_HOOK_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_PLUGIN_UI_HOOK, EPluginUIHookPrivate))

#define E_PLUGIN_UI_DEFAULT_FUNC	"e_plugin_ui_init"
#define E_PLUGIN_UI_HOOK_CLASS_ID	"org.gnome.evolution.ui:1.0"

struct _EPluginUIHookPrivate {

	/* Table of GtkUIManager ID's to UI definitions.
	 *
	 * For example:
	 *
	 *     <hook class="org.gnome.evolution.ui:1.0">
	 *       <ui-manager id="org.gnome.evolution.foo">
	 *               ... UI definition ...
	 *       </ui-manager>
	 *     </hook>
	 *
	 * Results in:
	 *
	 *     g_hash_table_insert (
	 *             ui_definitions,
	 *             "org.gnome.evolution.foo",
	 *             "... UI definition ...");
	 *
	 * See http://library.gnome.org/devel/gtk/unstable/GtkUIManager.html
	 * for more information about UI definitions.  Note: the <ui> tag is
	 * optional.
	 */
	GHashTable *ui_definitions;

	/* Table of GtkUIManager ID's to callback function names.
	 *
	 * This stores the optional "callback" attribute in the <ui-manager>
	 * element.  If not specified, it defaults to "e_plugin_ui_init".
	 *
	 * This is useful when extending the UI of multiple GtkUIManager IDs
	 * from a single plugin.
	 *
	 * For example:
	 *
	 *     <hook class="org.gnome.evolution.ui:1.0">
	 *       <ui-manager id="org.gnome.evolution.foo" callback="init_foo">
	 *         ...
	 *       </ui-manager>
	 *       <ui-manager id="org.gnome.evolution.bar" callback="init_bar">
	 *         ...
	 *       </ui-manager>
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

	/* The registry is the heart of EPluginUI.  It tracks GtkUIManager
	 * instances, GtkUIManager IDs, and UI merge IDs as a hash table of
	 * hash tables:
	 *
	 *     GtkUIManager instance -> GtkUIManager ID -> UI Merge ID
	 *
	 * A GtkUIManager instance and ID form a unique key for looking up
	 * UI merge IDs.  The reason both are needed is because the same
	 * GtkUIManager instance can be registered under multiple IDs.
	 *
	 * This is done primarily to support shell views, which share a
	 * common GtkUIManager instance for a particular shell window.
	 * Each shell view registers the same GtkUIManager instance under
	 * a unique ID:
	 *
	 *     "org.gnome.evolution.mail"      }
	 *     "org.gnome.evolution.contacts"  }  aliases for a common
	 *     "org.gnome.evolution.calendar"  }  GtkUIManager instance
	 *     "org.gnome.evolution.memos"     }
	 *     "org.gnome.evolution.tasks"     }
	 *
	 * Note: The shell window also registers the same GtkUIManager
	 *       instance as "org.gnome.evolution.shell".
	 *
	 * This way, plugins that extend a shell view's UI will follow the
	 * merging and unmerging of the shell view automatically.
	 *
	 * The presence or absence of GtkUIManager IDs in the registry is
	 * significant.  Presence of a (instance, ID) pair indicates that
	 * UI manager is active, absence indicates inactive.  Furthermore,
	 * a non-zero merge ID for an active UI manager indicates the
	 * plugin is enabled.  Zero indicates disabled.
	 *
	 * Here's a quick scenario to illustrate:
	 *
	 * Suppose we have a plugin that extends the mail shell view UI.
	 * Its EPlugin definition file has this section:
	 *
	 *     <hook class="org.gnome.evolution.ui:1.0">
	 *       <ui-manager id="org.gnome.evolution.mail">
	 *               ... UI definition ...
	 *       </ui-manager>
	 *     </hook>
	 *
	 * The plugin is enabled and the active shell view is "mail".
	 * Let "ManagerA" denote the common GtkUIManager instance for
	 * this shell window.  Here's what happens to the registry as
	 * the user performs various actions;
	 *
	 *     - Initial State                            Merge ID
	 *                                                   V
	 *       { "ManagerA", { "org.gnome.evolution.mail", 3 } }
	 *
	 *     - User Disables the Plugin
	 *
	 *       { "ManagerA", { "org.gnome.evolution.mail", 0 } }
	 *
	 *     - User Enables the Plugin
	 *
	 *       { "ManagerA", { "org.gnome.evolution.mail", 4 } }
	 *
	 *     - User Switches to Calendar View
	 *
	 *       { "ManagerA", { } }
	 *
	 *     - User Disables the Plugin
	 *
	 *       { "ManagerA", { } }
	 *
	 *     - User Switches to Mail View
	 *
	 *       { "ManagerA", { "org.gnome.evolution.mail", 0 } }
	 *
	 *     - User Enables the Plugin
	 *
	 *       { "ManagerA", { "org.gnome.evolution.mail", 5 } }
	 */
	GHashTable *registry;
};

G_DEFINE_TYPE (
	EPluginUIHook,
	e_plugin_ui_hook,
	E_TYPE_PLUGIN_HOOK)

static void
plugin_ui_hook_unregister_manager (EPluginUIHook *hook,
                                   GtkUIManager *ui_manager)
{
	GHashTable *registry;

	/* Note: Manager may already be finalized. */
	registry = hook->priv->registry;
	g_hash_table_remove (registry, ui_manager);
}

static void
plugin_ui_hook_register_manager (EPluginUIHook *hook,
                                 GtkUIManager *ui_manager,
                                 const gchar *id,
                                 gpointer user_data)
{
	EPlugin *plugin;
	EPluginUIInitFunc func;
	GHashTable *registry;
	GHashTable *hash_table;
	const gchar *func_name;

	plugin = ((EPluginHook *) hook)->plugin;

	hash_table = hook->priv->callbacks;
	func_name = g_hash_table_lookup (hash_table, id);

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
	 * The plugin should install whatever GtkActions and GtkActionGroups
	 * are neccessary to implement the actions in its UI definition. */
	if (!func (ui_manager, user_data))
		return;

	g_object_weak_ref (
		G_OBJECT (ui_manager), (GWeakNotify)
		plugin_ui_hook_unregister_manager, hook);

	registry = hook->priv->registry;
	hash_table = g_hash_table_lookup (registry, ui_manager);

	if (hash_table == NULL) {
		hash_table = g_hash_table_new_full (
			g_str_hash, g_str_equal,
			(GDestroyNotify) g_free,
			(GDestroyNotify) NULL);
		g_hash_table_insert (registry, ui_manager, hash_table);
	}
}

static guint
plugin_ui_hook_merge_ui (EPluginUIHook *hook,
                         GtkUIManager *ui_manager,
                         const gchar *id)
{
	GHashTable *hash_table;
	const gchar *ui_definition;
	guint merge_id;
	GError *error = NULL;

	hash_table = hook->priv->ui_definitions;
	ui_definition = g_hash_table_lookup (hash_table, id);
	g_return_val_if_fail (ui_definition != NULL, 0);

	merge_id = gtk_ui_manager_add_ui_from_string (
		ui_manager, ui_definition, -1, &error);

	if (error != NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}

	return merge_id;
}

static void
plugin_ui_enable_manager (EPluginUIHook *hook,
                          GtkUIManager *ui_manager,
                          const gchar *id)
{
	GHashTable *hash_table;
	GHashTable *ui_definitions;
	GList *keys;

	hash_table = hook->priv->registry;
	hash_table = g_hash_table_lookup (hash_table, ui_manager);

	if (hash_table == NULL)
		return;

	if (id != NULL)
		keys = g_list_prepend (NULL, (gpointer) id);
	else
		keys = g_hash_table_get_keys (hash_table);

	ui_definitions = hook->priv->ui_definitions;

	while (keys != NULL) {
		guint merge_id;
		gpointer data;

		id = keys->data;
		keys = g_list_delete_link (keys, keys);

		if (g_hash_table_lookup (ui_definitions, id) == NULL)
			continue;

		data = g_hash_table_lookup (hash_table, id);
		merge_id = GPOINTER_TO_UINT (data);

		if (merge_id > 0)
			continue;

		if (((EPluginHook *) hook)->plugin->enabled)
			merge_id = plugin_ui_hook_merge_ui (
				hook, ui_manager, id);

		/* Merge ID will be 0 on error, which is what we want. */
		data = GUINT_TO_POINTER (merge_id);
		g_hash_table_insert (hash_table, g_strdup (id), data);
	}
}

static void
plugin_ui_disable_manager (EPluginUIHook *hook,
                           GtkUIManager *ui_manager,
                           const gchar *id,
                           gboolean remove)
{
	GHashTable *hash_table;
	GHashTable *ui_definitions;
	GList *keys;

	hash_table = hook->priv->registry;
	hash_table = g_hash_table_lookup (hash_table, ui_manager);

	if (hash_table == NULL)
		return;

	if (id != NULL)
		keys = g_list_prepend (NULL, (gpointer) id);
	else
		keys = g_hash_table_get_keys (hash_table);

	ui_definitions = hook->priv->ui_definitions;

	while (keys != NULL) {
		guint merge_id;
		gpointer data;

		id = keys->data;
		keys = g_list_delete_link (keys, keys);

		if (g_hash_table_lookup (ui_definitions, id) == NULL)
			continue;

		data = g_hash_table_lookup (hash_table, id);
		merge_id = GPOINTER_TO_UINT (data);

		/* Merge ID could be 0 if the plugin is disabled. */
		if (merge_id > 0) {
			gtk_ui_manager_remove_ui (ui_manager, merge_id);
			gtk_ui_manager_ensure_update (ui_manager);
		}

		if (remove)
			g_hash_table_remove (hash_table, id);
		else
			g_hash_table_insert (hash_table, g_strdup (id), NULL);
	}
}

static void
plugin_ui_enable_hook (EPluginUIHook *hook)
{
	GHashTable *hash_table;
	GHashTableIter iter;
	gpointer key;

	/* Enable all GtkUIManagers for this hook. */

	hash_table = hook->priv->registry;
	g_hash_table_iter_init (&iter, hash_table);

	while (g_hash_table_iter_next (&iter, &key, NULL)) {
		GtkUIManager *ui_manager = key;
		plugin_ui_enable_manager (hook, ui_manager, NULL);
	}
}

static void
plugin_ui_disable_hook (EPluginUIHook *hook)
{
	GHashTable *hash_table;
	GHashTableIter iter;
	gpointer key;

	/* Disable all GtkUIManagers for this hook. */

	hash_table = hook->priv->registry;
	g_hash_table_iter_init (&iter, hash_table);

	while (g_hash_table_iter_next (&iter, &key, NULL)) {
		GtkUIManager *ui_manager = key;
		plugin_ui_disable_manager (hook, ui_manager, NULL, FALSE);
	}
}

static void
plugin_ui_hook_finalize (GObject *object)
{
	EPluginUIHookPrivate *priv;
	GHashTableIter iter;
	gpointer ui_manager;

	priv = E_PLUGIN_UI_HOOK_GET_PRIVATE (object);

	/* Remove weak reference callbacks to GtkUIManagers. */
	g_hash_table_iter_init (&iter, priv->registry);
	while (g_hash_table_iter_next (&iter, &ui_manager, NULL))
		g_object_weak_unref (
			G_OBJECT (ui_manager), (GWeakNotify)
			plugin_ui_hook_unregister_manager, object);

	g_hash_table_destroy (priv->ui_definitions);
	g_hash_table_destroy (priv->callbacks);
	g_hash_table_destroy (priv->registry);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_plugin_ui_hook_parent_class)->dispose (object);
}

static gint
plugin_ui_hook_construct (EPluginHook *hook,
                          EPlugin *plugin,
                          xmlNodePtr node)
{
	EPluginUIHookPrivate *priv;

	priv = E_PLUGIN_UI_HOOK_GET_PRIVATE (hook);

	/* XXX The EPlugin should be a property of EPluginHookClass.
	 *     Then it could be passed directly to g_object_new() and
	 *     we wouldn't have to chain up here. */

	/* Chain up to parent's construct() method. */
	E_PLUGIN_HOOK_CLASS (e_plugin_ui_hook_parent_class)->
		construct (hook, plugin, node);

	for (node = xmlFirstElementChild (node); node != NULL;
		node = xmlNextElementSibling (node)) {

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
		if (callback != NULL)
			g_hash_table_insert (
				priv->callbacks,
				g_strdup (id), callback);

		content = g_string_sized_new (1024);

		/* Extract the XML content below <ui-manager> */
		buffer = xmlBufferCreate ();
		for (child = node->children; child != NULL; child = child->next) {
			xmlNodeDump (buffer, node->doc, child, 2, 1);
			temp = (const gchar *) xmlBufferContent (buffer);
			g_string_append (content, temp);
		}

		g_hash_table_insert (
			priv->ui_definitions,
			id, g_string_free (content, FALSE));

		xmlBufferFree (buffer);
	}

	return 0;
}

static void
plugin_ui_hook_enable (EPluginHook *hook,
                       gint state)
{
	if (state)
		plugin_ui_enable_hook (E_PLUGIN_UI_HOOK (hook));
	else
		plugin_ui_disable_hook (E_PLUGIN_UI_HOOK (hook));
}

static void
e_plugin_ui_hook_class_init (EPluginUIHookClass *class)
{
	GObjectClass *object_class;
	EPluginHookClass *plugin_hook_class;

	g_type_class_add_private (class, sizeof (EPluginUIHookPrivate));

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
	GHashTable *ui_definitions;
	GHashTable *callbacks;
	GHashTable *registry;

	ui_definitions = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_free);

	callbacks = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_free);

	registry = g_hash_table_new_full (
		g_direct_hash, g_direct_equal,
		(GDestroyNotify) NULL,
		(GDestroyNotify) g_hash_table_destroy);

	hook->priv = E_PLUGIN_UI_HOOK_GET_PRIVATE (hook);
	hook->priv->ui_definitions = ui_definitions;
	hook->priv->callbacks = callbacks;
	hook->priv->registry = registry;
}

void
e_plugin_ui_register_manager (GtkUIManager *ui_manager,
                              const gchar *id,
                              gpointer user_data)
{
	GSList *plugin_list;

	g_return_if_fail (GTK_IS_UI_MANAGER (ui_manager));
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
			GHashTable *hash_table;

			if (!E_IS_PLUGIN_UI_HOOK (hook))
				continue;

			hash_table = hook->priv->ui_definitions;

			/* Check if the hook has a UI definition
			 * for the GtkUIManager being registered. */
			if (g_hash_table_lookup (hash_table, id) == NULL)
				continue;

			/* Register the manager with the hook. */
			plugin_ui_hook_register_manager (
				hook, ui_manager, id, user_data);
		}

		g_object_unref (plugin);
	}
}

void
e_plugin_ui_enable_manager (GtkUIManager *ui_manager,
                            const gchar *id)
{
	GSList *plugin_list;

	g_return_if_fail (GTK_IS_UI_MANAGER (ui_manager));
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

			plugin_ui_enable_manager (hook, ui_manager, id);
		}

		g_object_unref (plugin);
	}
}

void
e_plugin_ui_disable_manager (GtkUIManager *ui_manager,
                             const gchar *id)
{
	GSList *plugin_list;

	g_return_if_fail (GTK_IS_UI_MANAGER (ui_manager));
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

			plugin_ui_disable_manager (hook, ui_manager, id, TRUE);
		}

		g_object_unref (plugin);
	}
}
