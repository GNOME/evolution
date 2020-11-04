/*
 * Copyright (C) 2016 Red Hat, Inc. (www.redhat.com)
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "evolution-config.h"

#include <stdio.h>
#include <gio/gio.h>

#define G_SETTINGS_ENABLE_BACKEND
#include <gio/gsettingsbackend.h>
#undef G_SETTINGS_ENABLE_BACKEND

#include <libedataserver/libedataserver.h>

#include "test-keyfile-settings-backend.h"

#define TEST_TYPE_KEYFILE_SETTINGS_BACKEND  (test_keyfile_settings_backend_get_type())
#define TEST_KEYFILE_SETTINGS_BACKEND(inst) (G_TYPE_CHECK_INSTANCE_CAST ((inst), \
                                         TEST_TYPE_KEYFILE_SETTINGS_BACKEND,     \
                                         TestKeyfileSettingsBackend))

typedef GSettingsBackendClass TestKeyfileSettingsBackendClass;
typedef struct {
	GSettingsBackend parent_instance;

	GSettingsBackend *kf_backend;
	GHashTable *change_listeners; /* GSettings * ~> gchar *path */
} TestKeyfileSettingsBackend;

static GType test_keyfile_settings_backend_get_type (void);

G_DEFINE_TYPE_WITH_CODE (TestKeyfileSettingsBackend,
                         test_keyfile_settings_backend,
                         G_TYPE_SETTINGS_BACKEND,
                         g_io_extension_point_implement (G_SETTINGS_BACKEND_EXTENSION_POINT_NAME,
                                                         g_define_type_id, TEST_KEYFILE_SETTINGS_BACKEND_NAME, 5))

static void
test_keyfile_settings_backend_changed_cb (GSettings *settings,
					  const gchar *key,
					  gpointer user_data)
{
	TestKeyfileSettingsBackend *tk_backend = user_data;
	const gchar *path;
	gchar *key_path;

	g_return_if_fail (tk_backend != NULL);

	path = g_hash_table_lookup (tk_backend->change_listeners, settings);
	g_return_if_fail (path != NULL);

	key_path = g_strconcat (path, key, NULL);

	g_settings_backend_changed (G_SETTINGS_BACKEND (tk_backend), key_path, NULL);

	g_free (key_path);
}

static void
test_keyfile_settings_backend_writable_changed_cb (GSettings *settings,
						   const gchar *key,
						   gpointer user_data)
{
	TestKeyfileSettingsBackend *tk_backend = user_data;
	const gchar *path;
	gchar *key_path;

	g_return_if_fail (tk_backend != NULL);

	path = g_hash_table_lookup (tk_backend->change_listeners, settings);
	g_return_if_fail (path != NULL);

	key_path = g_strconcat (path, key, NULL);

	g_settings_backend_writable_changed (G_SETTINGS_BACKEND (tk_backend), key_path);

	g_free (key_path);
}

static void
test_keyfile_settings_backend_add_change_listener (TestKeyfileSettingsBackend *tk_backend,
						   const gchar *schema_id)
{
	GSettings *settings;
	gchar *path;
	gint ii;

	g_return_if_fail (tk_backend != NULL);
	g_return_if_fail (schema_id != NULL);

	path = g_strconcat ("/", schema_id, "/", NULL);
	for (ii = 0; path[ii]; ii++) {
		if (path[ii] == '.')
			path[ii] = '/';
	}

	settings = g_settings_new_with_backend (schema_id, tk_backend->kf_backend);

	g_hash_table_insert (tk_backend->change_listeners, settings, path);

	g_signal_connect (settings, "changed", G_CALLBACK (test_keyfile_settings_backend_changed_cb), tk_backend);
	g_signal_connect (settings, "writable-changed", G_CALLBACK (test_keyfile_settings_backend_writable_changed_cb), tk_backend);
}

static GVariant *
test_keyfile_settings_backend_read (GSettingsBackend *backend,
				    const gchar *key,
				    const GVariantType *expected_type,
				    gboolean default_value)
{
	TestKeyfileSettingsBackend *tk_backend = TEST_KEYFILE_SETTINGS_BACKEND (backend);
	GSettingsBackendClass *klass;

	klass = G_SETTINGS_BACKEND_GET_CLASS (tk_backend->kf_backend);
	g_return_val_if_fail (klass != NULL, NULL);
	g_return_val_if_fail (klass->read != NULL, NULL);

	return klass->read (tk_backend->kf_backend, key, expected_type, default_value);
}

static gboolean
test_keyfile_settings_backend_get_writable (GSettingsBackend *backend,
					    const gchar *name)
{
	TestKeyfileSettingsBackend *tk_backend = TEST_KEYFILE_SETTINGS_BACKEND (backend);
	GSettingsBackendClass *klass;

	klass = G_SETTINGS_BACKEND_GET_CLASS (tk_backend->kf_backend);
	g_return_val_if_fail (klass != NULL, FALSE);
	g_return_val_if_fail (klass->get_writable != NULL, FALSE);

	return klass->get_writable (tk_backend->kf_backend, name);
}

static gboolean
test_keyfile_settings_backend_write (GSettingsBackend *backend,
				     const gchar *key,
				     GVariant *value,
				     gpointer origin_tag)
{
	TestKeyfileSettingsBackend *tk_backend = TEST_KEYFILE_SETTINGS_BACKEND (backend);
	GSettingsBackendClass *klass;
	gboolean success;

	klass = G_SETTINGS_BACKEND_GET_CLASS (tk_backend->kf_backend);
	g_return_val_if_fail (klass != NULL, FALSE);
	g_return_val_if_fail (klass->write != NULL, FALSE);

	success = klass->write (tk_backend->kf_backend, key, value, origin_tag);

	g_settings_backend_changed (backend, key, origin_tag);

	return success;
}

static gboolean
test_keyfile_settings_backend_write_tree (GSettingsBackend *backend,
					  GTree *tree,
					  gpointer origin_tag)
{
	TestKeyfileSettingsBackend *tk_backend = TEST_KEYFILE_SETTINGS_BACKEND (backend);
	GSettingsBackendClass *klass;
	gboolean success;

	klass = G_SETTINGS_BACKEND_GET_CLASS (tk_backend->kf_backend);
	g_return_val_if_fail (klass != NULL, FALSE);
	g_return_val_if_fail (klass->write_tree != NULL, FALSE);

	success = klass->write_tree (tk_backend->kf_backend, tree, origin_tag);

	g_settings_backend_changed_tree (backend, tree, origin_tag);

	return success;
}

static void
test_keyfile_settings_backend_reset (GSettingsBackend *backend,
				     const gchar *key,
				     gpointer origin_tag)
{
	TestKeyfileSettingsBackend *tk_backend = TEST_KEYFILE_SETTINGS_BACKEND (backend);
	GSettingsBackendClass *klass;

	klass = G_SETTINGS_BACKEND_GET_CLASS (tk_backend->kf_backend);
	g_return_if_fail (klass != NULL);
	g_return_if_fail (klass->reset != NULL);

	klass->reset (tk_backend->kf_backend, key, origin_tag);

	g_settings_backend_changed (backend, key, origin_tag);
}

static GPermission *
test_keyfile_settings_backend_get_permission (GSettingsBackend *backend,
					      const gchar *path)
{
	TestKeyfileSettingsBackend *tk_backend = TEST_KEYFILE_SETTINGS_BACKEND (backend);
	GSettingsBackendClass *klass;

	klass = G_SETTINGS_BACKEND_GET_CLASS (tk_backend->kf_backend);
	g_return_val_if_fail (klass != NULL, NULL);
	g_return_val_if_fail (klass->get_permission != NULL, NULL);

	return klass->get_permission (tk_backend->kf_backend, path);
}

static void
test_keyfile_settings_backend_finalize (GObject *object)
{
	TestKeyfileSettingsBackend *tk_backend = TEST_KEYFILE_SETTINGS_BACKEND (object);

	g_clear_object (&tk_backend->kf_backend);
	g_clear_pointer (&tk_backend->change_listeners, g_hash_table_destroy);

	G_OBJECT_CLASS (test_keyfile_settings_backend_parent_class)->finalize (object);
}

static void
test_keyfile_settings_backend_init (TestKeyfileSettingsBackend *tk_backend)
{
	gchar **non_relocatable_schemas = NULL, **relocatable_schemas = NULL;
	gint ii;

	tk_backend->kf_backend = g_keyfile_settings_backend_new (g_getenv (TEST_KEYFILE_SETTINGS_FILENAME_ENVVAR), "/", "root");
	tk_backend->change_listeners = g_hash_table_new_full (g_direct_hash, g_direct_equal, g_object_unref, g_free);

	g_settings_schema_source_list_schemas (g_settings_schema_source_get_default (), TRUE, &non_relocatable_schemas, &relocatable_schemas);

	for (ii = 0; non_relocatable_schemas && non_relocatable_schemas[ii]; ii++) {
		if (e_util_strstrcase (non_relocatable_schemas[ii], "evolution")) {
			test_keyfile_settings_backend_add_change_listener (tk_backend, non_relocatable_schemas[ii]);
		}
	}
	g_strfreev (non_relocatable_schemas);
	g_strfreev (relocatable_schemas);
}

static void
test_keyfile_settings_backend_class_init (TestKeyfileSettingsBackendClass *class)
{
	GSettingsBackendClass *backend_class;
	GObjectClass *object_class;

	backend_class = G_SETTINGS_BACKEND_CLASS (class);
	backend_class->read = test_keyfile_settings_backend_read;
	backend_class->get_writable = test_keyfile_settings_backend_get_writable;
	backend_class->write = test_keyfile_settings_backend_write;
	backend_class->write_tree = test_keyfile_settings_backend_write_tree;
	backend_class->reset = test_keyfile_settings_backend_reset;
	backend_class->get_permission = test_keyfile_settings_backend_get_permission;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = test_keyfile_settings_backend_finalize;
}

void
g_io_module_load (GIOModule *module)
{
	g_type_module_use (G_TYPE_MODULE (module));
	g_type_ensure (test_keyfile_settings_backend_get_type ());
}

void
g_io_module_unload (GIOModule *module)
{
	g_warn_if_reached ();
}

gchar **
g_io_module_query (void)
{
	return g_strsplit (G_SETTINGS_BACKEND_EXTENSION_POINT_NAME, "!", 0);
}
