/*
 * Copyright (C) 2015 Red Hat Inc. (www.redhat.com)
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of version 2.1. of the GNU Lesser General Public License
 * as published by the Free Software Foundation.
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
#include <string.h>

#include <camel/camel.h>
#include <sqlite3.h>

#include <libemail-engine/libemail-engine.h>

#include "e-mail-properties.h"

#define CURRENT_VERSION 1

struct _EMailPropertiesPrivate {
	CamelDB *db;
};

G_DEFINE_TYPE_WITH_PRIVATE (EMailProperties, e_mail_properties, G_TYPE_OBJECT)

static gboolean
e_mail_properties_get_value_cb (gpointer data,
				gint ncol,
				gchar **colvalues,
				gchar **colnames)
{
	gchar **value = data;

	if (value && colvalues && colvalues[0]) {
		g_return_val_if_fail (*value == NULL, FALSE);

		*value = g_strdup (colvalues[0]);
	}

	return TRUE;
}

static gchar *
e_mail_properties_get (EMailProperties *properties,
		       const gchar *table,
		       const gchar *id,
		       const gchar *key)
{
	gchar *value = NULL;
	gchar *stmt;

	g_return_val_if_fail (E_IS_MAIL_PROPERTIES (properties), NULL);
	g_return_val_if_fail (table != NULL, NULL);
	g_return_val_if_fail (id != NULL, NULL);
	g_return_val_if_fail (key != NULL, NULL);
	g_return_val_if_fail (properties->priv->db != NULL, NULL);

	stmt = sqlite3_mprintf ("SELECT value FROM %Q WHERE id=%Q AND key=%Q", table, id, key);
	camel_db_exec_select (properties->priv->db, stmt, e_mail_properties_get_value_cb, &value, NULL);
	sqlite3_free (stmt);

	return value;
}

static void
e_mail_properties_add (EMailProperties *properties,
		       const gchar *table,
		       const gchar *id,
		       const gchar *key,
		       const gchar *value)
{
	gchar *stmt, *stored;
	GError *error = NULL;

	g_return_if_fail (E_IS_MAIL_PROPERTIES (properties));
	g_return_if_fail (table != NULL);
	g_return_if_fail (id != NULL);
	g_return_if_fail (key != NULL);
	g_return_if_fail (value != NULL);
	g_return_if_fail (properties->priv->db != NULL);

	stored = e_mail_properties_get (properties, table, id, key);
	if (stored)
		stmt = sqlite3_mprintf ("UPDATE %Q SET id=%Q,key=%Q,value=%Q WHERE id=%Q AND key=%Q", table, id, key, value, id, key);
	else
		stmt = sqlite3_mprintf ("INSERT INTO %Q (id,key,value) VALUES (%Q,%Q,%Q)", table, id, key, value, id, key);
	camel_db_exec_statement (properties->priv->db, stmt, &error);
	sqlite3_free (stmt);
	g_free (stored);

	if (error) {
		g_warning ("%s: Failed to add to '%s' for '%s|%s|%s': %s", G_STRFUNC, table, id, key, value, error->message);
		g_clear_error (&error);
	}
}

static void
e_mail_properties_remove (EMailProperties *properties,
			  const gchar *table,
			  const gchar *id,
			  const gchar *key)
{
	gchar *stmt;
	GError *error = NULL;

	g_return_if_fail (E_IS_MAIL_PROPERTIES (properties));
	g_return_if_fail (table != NULL);
	g_return_if_fail (id != NULL);
	g_return_if_fail (key != NULL);
	g_return_if_fail (properties->priv->db != NULL);

	stmt = sqlite3_mprintf ("DELETE FROM %Q WHERE id=%Q AND key=%Q", table, id, key);
	camel_db_exec_statement (properties->priv->db, stmt, &error);
	sqlite3_free (stmt);

	if (error) {
		g_warning ("%s: Failed to remove from '%s' value '%s|%s': %s", G_STRFUNC, table, id, key, error->message);
		g_clear_error (&error);
	}
}

static gboolean
e_mail_properties_get_version_cb (gpointer data,
				  gint ncol,
				  gchar **colvalues,
				  gchar **colnames)
{
	gint *pversion = data;

	if (pversion && ncol == 1 && colvalues && colvalues[0])
		*pversion = (gint) g_ascii_strtoll (colvalues[0], NULL, 10);

	return TRUE;
}

static void
e_mail_properties_set_config_filename (EMailProperties *properties,
				       const gchar *config_filename)
{
	GError *error = NULL;

	g_return_if_fail (E_IS_MAIL_PROPERTIES (properties));
	g_return_if_fail (config_filename != NULL);
	g_return_if_fail (properties->priv->db == NULL);

	properties->priv->db = camel_db_new (config_filename, &error);

	if (error) {
		g_warning ("%s: Failed to open '%s': %s", G_STRFUNC, config_filename, error->message);
		g_clear_error (&error);
	}

	if (properties->priv->db) {
		#define ctb(stmt) G_STMT_START { \
			if (properties->priv->db) { \
				camel_db_exec_statement (properties->priv->db, stmt, &error); \
				if (error) { \
					g_warning ("%s: Failed to execute '%s' on '%s': %s", \
						G_STRFUNC, stmt, config_filename, error->message); \
					g_clear_error (&error); \
				} \
			} \
		} G_STMT_END

		ctb ("CREATE TABLE IF NOT EXISTS version (current INT)");
		ctb ("CREATE TABLE IF NOT EXISTS folders ('id' TEXT, 'key' TEXT, 'value' TEXT)");
		ctb ("CREATE INDEX IF NOT EXISTS 'folders_index' ON 'folders' (id,key)");

		#undef ctb
	}

	if (properties->priv->db) {
		gint version = -1;
		gchar *stmt;

		camel_db_exec_select (properties->priv->db, "SELECT 'current' FROM 'version'", e_mail_properties_get_version_cb, &version, NULL);

		if (version != -1 && version < CURRENT_VERSION) {
			/* Here will be added migration code, if needed in the future */
		}

		if (version < CURRENT_VERSION) {
			stmt = sqlite3_mprintf ("DELETE FROM %Q", "version");
			camel_db_exec_statement (properties->priv->db, stmt, NULL);
			sqlite3_free (stmt);

			stmt = sqlite3_mprintf ("INSERT INTO %Q (current) VALUES (%d);", "version", CURRENT_VERSION);
			camel_db_exec_statement (properties->priv->db, stmt, NULL);
			sqlite3_free (stmt);
		}
	}
}

static void
mail_properties_finalize (GObject *object)
{
	EMailProperties *properties;

	properties = E_MAIL_PROPERTIES (object);

	if (properties->priv->db) {
		GError *error = NULL;

		camel_db_maybe_run_maintenance (properties->priv->db, &error);

		if (error) {
			g_warning ("%s: Failed to run maintenance: %s", G_STRFUNC, error->message);
			g_clear_error (&error);
		}

		g_clear_object (&properties->priv->db);
	}

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_mail_properties_parent_class)->finalize (object);
}

static void
e_mail_properties_class_init (EMailPropertiesClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = mail_properties_finalize;
}

static void
e_mail_properties_init (EMailProperties *properties)
{
	properties->priv = e_mail_properties_get_instance_private (properties);
}

EMailProperties *
e_mail_properties_new (const gchar *config_filename)
{
	EMailProperties *properties;

	properties = g_object_new (E_TYPE_MAIL_PROPERTIES, NULL);

	if (config_filename != NULL)
		e_mail_properties_set_config_filename (properties, config_filename);

	return properties;
}

void
e_mail_properties_set_for_folder (EMailProperties *properties,
				  CamelFolder *folder,
				  const gchar *key,
				  const gchar *value)
{
	gchar *folder_uri;

	g_return_if_fail (E_IS_MAIL_PROPERTIES (properties));
	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (key != NULL);

	folder_uri = e_mail_folder_uri_build (
		camel_folder_get_parent_store (folder),
		camel_folder_get_full_name (folder));

	g_return_if_fail (folder_uri != NULL);

	e_mail_properties_set_for_folder_uri (properties, folder_uri, key, value);

	g_free (folder_uri);
}

void
e_mail_properties_set_for_folder_uri (EMailProperties *properties,
				      const gchar *folder_uri,
				      const gchar *key,
				      const gchar *value)
{
	g_return_if_fail (E_IS_MAIL_PROPERTIES (properties));
	g_return_if_fail (folder_uri != NULL);
	g_return_if_fail (key != NULL);

	if (value)
		e_mail_properties_add (properties, "folders", folder_uri, key, value);
	else
		e_mail_properties_remove (properties, "folders", folder_uri, key);
}

/* Free returned pointer with g_free() */
gchar *
e_mail_properties_get_for_folder (EMailProperties *properties,
				  CamelFolder *folder,
				  const gchar *key)
{
	gchar *folder_uri, *value;

	g_return_val_if_fail (E_IS_MAIL_PROPERTIES (properties), NULL);
	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), NULL);
	g_return_val_if_fail (key != NULL, NULL);

	folder_uri = e_mail_folder_uri_build (
		camel_folder_get_parent_store (folder),
		camel_folder_get_full_name (folder));

	g_return_val_if_fail (folder_uri != NULL, NULL);

	value = e_mail_properties_get_for_folder_uri (properties, folder_uri, key);

	g_free (folder_uri);

	return value;
}

/* Free returned pointer with g_free() */
gchar *
e_mail_properties_get_for_folder_uri (EMailProperties *properties,
				      const gchar *folder_uri,
				      const gchar *key)
{
	g_return_val_if_fail (E_IS_MAIL_PROPERTIES (properties), NULL);
	g_return_val_if_fail (folder_uri != NULL, NULL);
	g_return_val_if_fail (key != NULL, NULL);

	return e_mail_properties_get (properties, "folders", folder_uri, key);
}
