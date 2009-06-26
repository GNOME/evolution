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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 */

#include "e-plugin-ui.h"

#include <string.h>

#define E_PLUGIN_UI_HOOK_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_PLUGIN_UI_HOOK, EPluginUIHookPrivate))

#define E_PLUGIN_UI_INIT_FUNC		"e_plugin_ui_init"
#define E_PLUGIN_UI_HOOK_CLASS_ID       "org.gnome.evolution.ui:1.0"
#define E_PLUGIN_UI_MANAGER_ID_KEY	"e-plugin-ui-manager-id"

struct _EPluginUIHookPrivate {

	/* Table of GtkUIManager ID's to UI definitions.
	 *
	 * For example:
	 *
	 *     <ui-manager id="org.gnome.evolution.sample">
	 *             ... UI definition ...
	 *     </ui-manager>
	 *
	 * Results in:
	 *
	 *     g_hash_table_insert (
	 *             ui_definitions,
	 *             "org.gnome.evolution.sample",
	 *             "... UI definition ...");
	 *
	 * See http://library.gnome.org/devel/gtk/unstable/GtkUIManager.html
	 * for more information about UI definitions.  Note: the <ui> tag is
	 * optional.
	 */
	GHashTable *ui_definitions;
};

/* The registry is a hash table of hash tables.  It maps
 *
 *    EPluginUIHook instance --> GtkUIManager instance --> UI merge id
 *
 * GtkUIManager instances are automatically removed when finalized.
 */
static GHashTable *registry;
static gpointer parent_class;

static void
plugin_ui_registry_remove (EPluginUIHook *hook,
                           GtkUIManager *ui_manager)
{
	GHashTable *hash_table;

	/* Note: Manager may already be finalized. */

	hash_table = g_hash_table_lookup (registry, hook);
	g_return_if_fail (hash_table != NULL);

	g_hash_table_remove (hash_table, ui_manager);
	if (g_hash_table_size (hash_table) == 0)
		g_hash_table_remove (registry, hook);
}

static void
plugin_ui_registry_insert (EPluginUIHook *hook,
                           GtkUIManager *ui_manager,
                           guint merge_id)
{
	GHashTable *hash_table;

	hash_table = g_hash_table_lookup (registry, hook);
	if (hash_table == NULL) {
		hash_table = g_hash_table_new (g_direct_hash, g_direct_equal);
		g_hash_table_insert (registry, hook, hash_table);
	}

	g_object_weak_ref (
		G_OBJECT (ui_manager), (GWeakNotify)
		plugin_ui_registry_remove, hook);

	g_hash_table_insert (
		hash_table, ui_manager, GUINT_TO_POINTER (merge_id));
}

/* Helper for plugin_ui_hook_merge_ui() */
static void
plugin_ui_hook_merge_foreach (GtkUIManager *ui_manager,
                              const gchar *ui_definition,
                              GHashTable *hash_table)
{
	guint merge_id;
	GError *error = NULL;

	/* Merge the UI definition into the manager. */
	merge_id = gtk_ui_manager_add_ui_from_string (
		ui_manager, ui_definition, -1, &error);
	gtk_ui_manager_ensure_update (ui_manager);
	if (error != NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}

	/* Merge ID will be 0 on error, which is what we want. */
	g_hash_table_insert (
		hash_table, ui_manager, GUINT_TO_POINTER (merge_id));
}

static void
plugin_ui_hook_merge_ui (EPluginUIHook *hook)
{
	GHashTable *old_merge_ids;
	GHashTable *new_merge_ids;
	GHashTable *intermediate;
	GList *keys;

	old_merge_ids = g_hash_table_lookup (registry, hook);
	if (old_merge_ids == NULL)
		return;

	/* The GtkUIManager instances and UI definitions live in separate
	 * tables, so we need to build an intermediate table that we can
	 * easily iterate over. */
	keys = g_hash_table_get_keys (old_merge_ids);
	intermediate = g_hash_table_new (g_direct_hash, g_direct_equal);

	while (keys != NULL) {
		GtkUIManager *ui_manager = keys->data;
		gchar *ui_definition;

		ui_definition = g_hash_table_lookup (
			hook->priv->ui_definitions,
			e_plugin_ui_get_manager_id (ui_manager));

		g_hash_table_insert (intermediate, ui_manager, ui_definition);

		keys = g_list_delete_link (keys, keys);
	}

	new_merge_ids = g_hash_table_new (g_direct_hash, g_direct_equal);

	g_hash_table_foreach (
		intermediate, (GHFunc)
		plugin_ui_hook_merge_foreach, new_merge_ids);

	g_hash_table_insert (registry, hook, new_merge_ids);

	g_hash_table_destroy (intermediate);
}

/* Helper for plugin_ui_hook_unmerge_ui() */
static void
plugin_ui_hook_unmerge_foreach (GtkUIManager *ui_manager,
                                gpointer value,
                                GHashTable *hash_table)
{
	guint merge_id;

	merge_id = GPOINTER_TO_UINT (value);
	gtk_ui_manager_remove_ui (ui_manager, merge_id);

	g_hash_table_insert (hash_table, ui_manager, GUINT_TO_POINTER (0));
}

static void
plugin_ui_hook_unmerge_ui (EPluginUIHook *hook)
{
	GHashTable *old_merge_ids;
	GHashTable *new_merge_ids;

	old_merge_ids = g_hash_table_lookup (registry, hook);
	if (old_merge_ids == NULL)
		return;

	new_merge_ids = g_hash_table_new (g_direct_hash, g_direct_equal);

	g_hash_table_foreach (
		old_merge_ids, (GHFunc)
		plugin_ui_hook_unmerge_foreach, new_merge_ids);

	g_hash_table_insert (registry, hook, new_merge_ids);
}

static void
plugin_ui_hook_register_manager (EPluginUIHook *hook,
                                 GtkUIManager *ui_manager,
                                 const gchar *ui_definition,
                                 gpointer user_data)
{
	EPlugin *plugin;
	EPluginUIInitFunc func;
	guint merge_id = 0;

	plugin = ((EPluginHook *) hook)->plugin;
	func = e_plugin_get_symbol (plugin, E_PLUGIN_UI_INIT_FUNC);

	/* Pass the manager and user_data to the plugin's e_plugin_ui_init()
	 * function (if it defined one).  The plugin should install whatever
	 * GtkActions and GtkActionGroups are neccessary to implement the
	 * action names in its UI definition. */
	if (func != NULL && !func (ui_manager, user_data))
		return;

	if (plugin->enabled) {
		GError *error = NULL;

		/* Merge the UI definition into the manager. */
		merge_id = gtk_ui_manager_add_ui_from_string (
			ui_manager, ui_definition, -1, &error);
		gtk_ui_manager_ensure_update (ui_manager);
		if (error != NULL) {
			g_warning ("%s", error->message);
			g_error_free (error);
		}
	}

	/* Save merge ID's for later use. */
	plugin_ui_registry_insert (hook, ui_manager, merge_id);
}

static void
plugin_ui_hook_finalize (GObject *object)
{
	EPluginUIHookPrivate *priv;

	priv = E_PLUGIN_UI_HOOK_GET_PRIVATE (object);

	g_hash_table_destroy (priv->ui_definitions);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
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
	E_PLUGIN_HOOK_CLASS (parent_class)->construct (hook, plugin, node);

	for (node = xmlFirstElementChild (node); node != NULL;
		node = xmlNextElementSibling (node)) {

		xmlNodePtr child;
		xmlBufferPtr buffer;
		GString *content;
		const gchar *temp;
		gchar *id;

		if (strcmp ((gchar *) node->name, "ui-manager") != 0)
			continue;

		id = e_plugin_xml_prop (node, "id");
		if (id == NULL) {
			g_warning ("<ui-manager> requires 'id' property");
			continue;
		}

		content = g_string_sized_new (1024);

		/* Extract the XML content below <ui-manager> */
		buffer = xmlBufferCreate ();
		for (child = xmlFirstElementChild (node); child != NULL;
			child = xmlNextElementSibling (child)) {

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
		plugin_ui_hook_merge_ui (E_PLUGIN_UI_HOOK (hook));
	else
		plugin_ui_hook_unmerge_ui (E_PLUGIN_UI_HOOK (hook));
}

static void
plugin_ui_hook_class_init (EPluginUIHookClass *class)
{
	GObjectClass *object_class;
	EPluginHookClass *plugin_hook_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EPluginUIHookPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = plugin_ui_hook_finalize;

	plugin_hook_class = E_PLUGIN_HOOK_CLASS (class);
	plugin_hook_class->id = E_PLUGIN_UI_HOOK_CLASS_ID;
	plugin_hook_class->construct = plugin_ui_hook_construct;
	plugin_hook_class->enable = plugin_ui_hook_enable;

	registry = g_hash_table_new_full (
		g_direct_hash, g_direct_equal,
		(GDestroyNotify) NULL,
		(GDestroyNotify) g_hash_table_destroy);
}

static void
plugin_ui_hook_init (EPluginUIHook *hook)
{
	GHashTable *ui_definitions;

	ui_definitions = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_free);

	hook->priv = E_PLUGIN_UI_HOOK_GET_PRIVATE (hook);
	hook->priv->ui_definitions = ui_definitions;
}

GType
e_plugin_ui_hook_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EPluginUIHookClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) plugin_ui_hook_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EPluginUIHook),
			0,     /* n_preallocs */
			(GInstanceInitFunc) plugin_ui_hook_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			E_TYPE_PLUGIN_HOOK, "EPluginUIHook", &type_info, 0);
	}

	return type;
}

void
e_plugin_ui_register_manager (const gchar *id,
                              GtkUIManager *ui_manager,
                              gpointer user_data)
{
	const gchar *key = E_PLUGIN_UI_MANAGER_ID_KEY;
	GSList *plugin_list;

	g_return_if_fail (id != NULL);
	g_return_if_fail (GTK_IS_UI_MANAGER (ui_manager));

	g_object_set_data (G_OBJECT (ui_manager), key, (gpointer) id);

	/* Loop over all installed plugins. */
	plugin_list = e_plugin_list_plugins ();
	while (plugin_list != NULL) {
		EPlugin *plugin = plugin_list->data;
		GSList *iter;

		/* Look for hooks of type EPluginUIHook. */
		for (iter = plugin->hooks; iter != NULL; iter = iter->next) {
			EPluginUIHook *hook = iter->data;
			const gchar *ui_definition;

			if (!E_IS_PLUGIN_UI_HOOK (hook))
				continue;

			/* Check if the hook has a UI definition
			 * for the GtkUIManager being registered. */
			ui_definition = g_hash_table_lookup (
				hook->priv->ui_definitions, id);
			if (ui_definition == NULL)
				continue;

			/* Register the manager with the hook. */
			plugin_ui_hook_register_manager (
				hook, ui_manager, ui_definition, user_data);
		}

		plugin_list = g_slist_next (plugin_list);
	}
}

const gchar *
e_plugin_ui_get_manager_id (GtkUIManager *ui_manager)
{
	const gchar *key = E_PLUGIN_UI_MANAGER_ID_KEY;

	g_return_val_if_fail (GTK_IS_UI_MANAGER (ui_manager), NULL);

	return g_object_get_data (G_OBJECT (ui_manager), key);
}
