/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-registry.c
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "e-shell-registry.h"

static GList *loaded_modules;
static GHashTable *modules_by_name;
static GHashTable *modules_by_schema;

static void
shell_registry_insert_items (GHashTable *hash_table,
                             const gchar *items,
                             EShellModule *shell_module)
{
	gpointer key;
	gchar **strv;
	gint ii;

	strv = g_strsplit_set (items, ":", -1);

	for (ii = 0; strv[ii] != NULL; ii++) {
		key = (gpointer) g_intern_string (strv[ii]);
		g_hash_table_insert (hash_table, key, shell_module);
	}

	g_strfreev (strv);
}

static void
shell_registry_query_module (const gchar *filename)
{
	EShellModule *shell_module;
	EShellModuleInfo *info;
	const gchar *string;

	shell_module = e_shell_module_new (filename);

	if (!g_type_module_use (G_TYPE_MODULE (shell_module))) {
		g_critical ("Failed to load module: %s", filename);
		g_object_unref (shell_module);
		return;
	}

	g_type_module_unuse (G_TYPE_MODULE (shell_module));

	loaded_modules = g_list_insert_sorted (
		loaded_modules, shell_module,
		(GCompareFunc) e_shell_module_compare);

	/* Bookkeeping */

	info = (EShellModuleInfo *) shell_module->priv;

	if ((string = G_TYPE_MODULE (shell_module)->name) != NULL)
		g_hash_table_insert (
			modules_by_name, (gpointer)
			g_intern_string (string), shell_module);

	if ((string = info->aliases) != NULL)
		shell_registry_insert_items (
			modules_by_name, string, shell_module);

	if ((string = info->schemas) != NULL)
		shell_registry_insert_items (
			modules_by_schema, string, shell_module);
}

void
e_shell_registry_init (void)
{
	GDir *dir;
	const gchar *dirname;
	const gchar *basename;
	GError *error = NULL;

	g_return_if_fail (loaded_modules == NULL);

	modules_by_name = g_hash_table_new (g_str_hash, g_str_equal);
	modules_by_schema = g_hash_table_new (g_str_hash, g_str_equal);

	dirname = EVOLUTION_MODULEDIR;

	dir = g_dir_open (dirname, 0, &error);
	if (dir == NULL) {
		g_critical ("%s", error->message);
		g_error_free (error);
		return;
	}

	while ((basename = g_dir_read_name (dir)) != NULL) {
		gchar *filename;

		if (!g_str_has_suffix (basename, "." G_MODULE_SUFFIX))
			continue;

		filename = g_build_filename (dirname, basename, NULL);
		shell_registry_query_module (filename);
		g_free (filename);
	}

	g_dir_close (dir);
}

GType *
e_shell_registry_get_view_types (guint *n_types)
{
	GType *types;
	GList *iter;
	guint ii = 0;

	types = g_new0 (GType, g_list_length (loaded_modules) + 1);

	for (iter = loaded_modules; iter != NULL; iter = iter->next) {
		EShellModule *shell_module = iter->data;
		EShellModuleInfo *info;

		info = (EShellModuleInfo *) shell_module->priv;

		/* Allow for modules with no corresponding view type. */
		if (!g_type_is_a (info->shell_view_type, E_TYPE_SHELL_VIEW))
			continue;

		types[ii++] = info->shell_view_type;
	}

	if (n_types != NULL)
		*n_types = ii;

	return types;
}

void
e_shell_registry_foreach_module (GFunc func,
                                 gpointer user_data)
{
	g_list_foreach (loaded_modules, func, user_data);
}

EShellModule *
e_shell_registry_get_module_by_name (const gchar *name)
{
	g_return_val_if_fail (name != NULL, NULL);

	return g_hash_table_lookup (modules_by_name, name);
}

EShellModule *
e_shell_registry_get_module_by_schema (const gchar *schema)
{
	g_return_val_if_fail (schema != NULL, NULL);

	return g_hash_table_lookup (modules_by_schema, schema);
}
