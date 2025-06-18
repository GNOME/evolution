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

#include "e-mail-remote-content.h"

#define CURRENT_VERSION 1

#define RECENT_CACHE_SIZE 10

typedef struct _RecentData {
	gchar *value;
	gboolean result;
} RecentData;

struct _EMailRemoteContentPrivate {
	CamelDB *db;

	GMutex recent_lock;
	RecentData recent_mails[RECENT_CACHE_SIZE];
	RecentData recent_sites[RECENT_CACHE_SIZE];
	guint recent_last_mails;
	guint recent_last_sites;
};

G_DEFINE_TYPE_WITH_PRIVATE (EMailRemoteContent, e_mail_remote_content, G_TYPE_OBJECT)

static void
e_mail_remote_content_add_to_recent_cache (EMailRemoteContent *content,
					   const gchar *value,
					   gboolean result,
					   RecentData *recent_cache,
					   guint *recent_last)
{
	gint ii, first_free = -1, index;

	g_return_if_fail (E_IS_MAIL_REMOTE_CONTENT (content));
	g_return_if_fail (value != NULL);
	g_return_if_fail (recent_cache != NULL);
	g_return_if_fail (recent_last != NULL);

	g_mutex_lock (&content->priv->recent_lock);

	for (ii = 0; ii < RECENT_CACHE_SIZE; ii++) {
		index = (*recent_last + ii) % RECENT_CACHE_SIZE;

		if (!recent_cache[index].value) {
			if (first_free == -1)
				first_free = index;
		} else if (g_ascii_strcasecmp (recent_cache[index].value, value) == 0) {
			recent_cache[index].result = result;
			break;
		}
	}

	if (ii == RECENT_CACHE_SIZE) {
		if (first_free != -1) {
			g_warn_if_fail (recent_cache[first_free].value == NULL);
			recent_cache[first_free].value = g_strdup (value);
			recent_cache[first_free].result = result;

			if (first_free == (*recent_last + 1) % RECENT_CACHE_SIZE)
				*recent_last = first_free;
		} else {
			index = (*recent_last + 1) % RECENT_CACHE_SIZE;

			g_free (recent_cache[index].value);
			recent_cache[index].value = g_strdup (value);
			recent_cache[index].result = result;

			*recent_last = index;
		}
	}

	g_mutex_unlock (&content->priv->recent_lock);
}

static void
e_mail_remote_content_add (EMailRemoteContent *content,
			   const gchar *table,
			   const gchar *value,
			   RecentData *recent_cache,
			   guint *recent_last)
{
	gchar *stmt;
	GError *error = NULL;

	g_return_if_fail (E_IS_MAIL_REMOTE_CONTENT (content));
	g_return_if_fail (table != NULL);
	g_return_if_fail (value != NULL);
	g_return_if_fail (recent_cache != NULL);
	g_return_if_fail (recent_last != NULL);

	e_mail_remote_content_add_to_recent_cache (content, value, TRUE, recent_cache, recent_last);

	if (!content->priv->db)
		return;

	stmt = sqlite3_mprintf ("INSERT OR IGNORE INTO %Q ('value') VALUES (lower(%Q))", table, value);
	camel_db_exec_statement (content->priv->db, stmt, &error);
	sqlite3_free (stmt);

	if (error) {
		g_warning ("%s: Failed to add to '%s' value '%s': %s", G_STRFUNC, table, value, error->message);
		g_clear_error (&error);
	}
}

static void
e_mail_remote_content_remove (EMailRemoteContent *content,
			      const gchar *table,
			      const gchar *value,
			      RecentData *recent_cache,
			      guint *recent_last)
{
	gchar *stmt;
	gint ii;
	GError *error = NULL;

	g_return_if_fail (E_IS_MAIL_REMOTE_CONTENT (content));
	g_return_if_fail (table != NULL);
	g_return_if_fail (value != NULL);
	g_return_if_fail (recent_cache != NULL);
	g_return_if_fail (recent_last != NULL);

	g_mutex_lock (&content->priv->recent_lock);

	for (ii = 0; ii < RECENT_CACHE_SIZE; ii++) {
		gint index = (*recent_last + ii) % RECENT_CACHE_SIZE;

		if (recent_cache[index].value && g_ascii_strcasecmp (recent_cache[index].value, value) == 0) {
			g_free (recent_cache[index].value);
			recent_cache[index].value = NULL;
			break;
		}
	}

	g_mutex_unlock (&content->priv->recent_lock);

	if (!content->priv->db)
		return;

	stmt = sqlite3_mprintf ("DELETE FROM %Q WHERE value=lower(%Q)", table, value);
	camel_db_exec_statement (content->priv->db, stmt, &error);
	sqlite3_free (stmt);

	if (error) {
		g_warning ("%s: Failed to remove from '%s' value '%s': %s", G_STRFUNC, table, value, error->message);
		g_clear_error (&error);
	}
}

typedef struct _CheckFoundData {
	gboolean found;
	gboolean added_generic;
	gboolean check_for_generic;
	EMailRemoteContent *content;
	RecentData *recent_cache;
	guint *recent_last;
} CheckFoundData;

static gboolean
e_mail_remote_content_check_found_cb (gpointer data,
				      gint ncol,
				      gchar **colvalues,
				      gchar **colnames)
{
	CheckFoundData *cfd = data;

	if (cfd) {
		cfd->found = TRUE;

		if (colvalues && *colvalues && **colvalues) {
			if (cfd->check_for_generic && *colvalues[0] == '@')
				cfd->added_generic = TRUE;

			e_mail_remote_content_add_to_recent_cache (cfd->content, colvalues[0], TRUE, cfd->recent_cache, cfd->recent_last);
		}
	}

	return TRUE;
}

static gboolean
e_mail_remote_content_has (EMailRemoteContent *content,
			   const gchar *table,
			   const GSList *values,
			   RecentData *recent_cache,
			   guint *recent_last)
{
	GString *stmt;
	gint ii;
	gchar *tmp;
	const GSList *link;
	gboolean found = FALSE, recent_cache_found = FALSE, added_generic = FALSE;

	g_return_val_if_fail (E_IS_MAIL_REMOTE_CONTENT (content), FALSE);
	g_return_val_if_fail (table != NULL, FALSE);
	g_return_val_if_fail (values != NULL, FALSE);
	g_return_val_if_fail (recent_cache != NULL, FALSE);
	g_return_val_if_fail (recent_last != NULL, FALSE);

	g_mutex_lock (&content->priv->recent_lock);

	for (link = values; link && !found; link = g_slist_next (link)) {
		const gchar *value = link->data;

		for (ii = 0; ii < RECENT_CACHE_SIZE; ii++) {
			gint index = (*recent_last + ii) % RECENT_CACHE_SIZE;

			if (recent_cache[index].value && g_ascii_strcasecmp (recent_cache[index].value, value) == 0) {
				recent_cache_found = TRUE;
				found = recent_cache[index].result;
				if (found)
					break;
			}
		}
	}

	g_mutex_unlock (&content->priv->recent_lock);

	if (recent_cache_found)
		return found;

	if (!content->priv->db)
		return FALSE;

	stmt = g_string_new ("");

	for (link = values; link; link = g_slist_next (link)) {
		const gchar *value = link->data;

		if (!value || !*value)
			continue;

		if (stmt->len)
			g_string_append (stmt, " OR ");

		tmp = sqlite3_mprintf ("value=lower(%Q)", value);
		g_string_append (stmt, tmp);
		sqlite3_free (tmp);
	}

	if (stmt->len) {
		CheckFoundData cfd;

		cfd.found = FALSE;
		cfd.added_generic = FALSE;
		cfd.check_for_generic = g_strcmp0 (table, "mail");
		cfd.content = content;
		cfd.recent_cache = recent_cache;
		cfd.recent_last = recent_last;

		tmp = sqlite3_mprintf ("SELECT value FROM %Q WHERE ", table);
		g_string_prepend (stmt, tmp);
		sqlite3_free (tmp);

		camel_db_exec_select (content->priv->db, stmt->str, e_mail_remote_content_check_found_cb, &cfd, NULL);

		found = cfd.found;
		added_generic = cfd.added_generic;
	}

	g_string_free (stmt, TRUE);

	if (!added_generic)
		e_mail_remote_content_add_to_recent_cache (content, values->data, found, recent_cache, recent_last);

	return found;
}

static gboolean
e_mail_remote_content_get_values_cb (gpointer data,
				     gint ncol,
				     gchar **colvalues,
				     gchar **colnames)
{
	GHashTable *values_hash = data;

	if (values_hash && colvalues && colvalues[0])
		g_hash_table_insert (values_hash, g_strdup (colvalues[0]), NULL);

	return TRUE;
}

static GSList *
e_mail_remote_content_get (EMailRemoteContent *content,
			   const gchar *table,
			   RecentData *recent_cache,
			   guint *recent_last)
{
	GHashTable *values_hash;
	GHashTableIter iter;
	GSList *values = NULL;
	gpointer itr_key, itr_value;
	gint ii;

	g_return_val_if_fail (E_IS_MAIL_REMOTE_CONTENT (content), NULL);
	g_return_val_if_fail (table != NULL, NULL);
	g_return_val_if_fail (recent_cache != NULL, NULL);
	g_return_val_if_fail (recent_last != NULL, NULL);

	values_hash = g_hash_table_new_full (camel_strcase_hash, camel_strcase_equal, g_free, NULL);

	g_mutex_lock (&content->priv->recent_lock);

	for (ii = 0; ii < RECENT_CACHE_SIZE; ii++) {
		gint index = (*recent_last + ii) % RECENT_CACHE_SIZE;

		if (recent_cache[index].value && recent_cache[index].result) {
			g_hash_table_insert (values_hash, g_strdup (recent_cache[index].value), NULL);
		}
	}

	g_mutex_unlock (&content->priv->recent_lock);

	if (content->priv->db) {
		gchar *stmt;

		stmt = sqlite3_mprintf ("SELECT value FROM %Q ORDER BY value", table);
		camel_db_exec_select (content->priv->db, stmt, e_mail_remote_content_get_values_cb, values_hash, NULL);
		sqlite3_free (stmt);
	}

	g_hash_table_iter_init (&iter, values_hash);

	while (g_hash_table_iter_next (&iter, &itr_key, &itr_value)) {
		const gchar *value = itr_key;

		if (value && *value)
			values = g_slist_prepend (values, g_strdup (value));
	}

	g_hash_table_destroy (values_hash);

	return g_slist_reverse (values);
}

static gboolean
e_mail_remote_content_get_version_cb (gpointer data,
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
e_mail_remote_content_set_config_filename (EMailRemoteContent *content,
					   const gchar *config_filename)
{
	GError *error = NULL;

	g_return_if_fail (E_IS_MAIL_REMOTE_CONTENT (content));
	g_return_if_fail (config_filename != NULL);
	g_return_if_fail (content->priv->db == NULL);

	content->priv->db = camel_db_new (config_filename, &error);

	if (error) {
		g_warning ("%s: Failed to open '%s': %s", G_STRFUNC, config_filename, error->message);
		g_clear_error (&error);
	}

	if (content->priv->db) {
		#define ctb(stmt) G_STMT_START { \
			if (content->priv->db) { \
				camel_db_exec_statement (content->priv->db, stmt, &error); \
				if (error) { \
					g_warning ("%s: Failed to execute '%s' on '%s': %s", \
						G_STRFUNC, stmt, config_filename, error->message); \
					g_clear_error (&error); \
				} \
			} \
		} G_STMT_END

		ctb ("CREATE TABLE IF NOT EXISTS version (current INT)");
		ctb ("CREATE TABLE IF NOT EXISTS sites (value TEXT PRIMARY KEY)");
		ctb ("CREATE TABLE IF NOT EXISTS mails (value TEXT PRIMARY KEY)");

		#undef ctb
	}

	if (content->priv->db) {
		gint version = -1;
		gchar *stmt;

		camel_db_exec_select (content->priv->db, "SELECT 'current' FROM 'version'", e_mail_remote_content_get_version_cb, &version, NULL);

		if (version != -1 && version < CURRENT_VERSION) {
			/* Here will be added migration code, if needed in the future */
		}

		stmt = sqlite3_mprintf ("DELETE FROM %Q", "version");
		camel_db_exec_statement (content->priv->db, stmt, NULL);
		sqlite3_free (stmt);

		stmt = sqlite3_mprintf ("INSERT INTO %Q ('current') VALUES (%d);", "version", CURRENT_VERSION);
		camel_db_exec_statement (content->priv->db, stmt, NULL);
		sqlite3_free (stmt);
	}
}

static void
mail_remote_content_finalize (GObject *object)
{
	EMailRemoteContent *content;
	gint ii;

	content = E_MAIL_REMOTE_CONTENT (object);

	if (content->priv->db) {
		GError *error = NULL;

		camel_db_maybe_run_maintenance (content->priv->db, &error);

		if (error) {
			g_warning ("%s: Failed to run maintenance: %s", G_STRFUNC, error->message);
			g_clear_error (&error);
		}

		g_clear_object (&content->priv->db);
	}

	g_mutex_lock (&content->priv->recent_lock);

	for (ii = 0; ii < RECENT_CACHE_SIZE; ii++) {
		g_free (content->priv->recent_sites[ii].value);
		g_free (content->priv->recent_mails[ii].value);

		content->priv->recent_sites[ii].value = NULL;
		content->priv->recent_mails[ii].value = NULL;
	}

	g_mutex_unlock (&content->priv->recent_lock);
	g_mutex_clear (&content->priv->recent_lock);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_mail_remote_content_parent_class)->finalize (object);
}

static void
e_mail_remote_content_class_init (EMailRemoteContentClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = mail_remote_content_finalize;
}

static void
e_mail_remote_content_init (EMailRemoteContent *content)
{
	content->priv = e_mail_remote_content_get_instance_private (content);

	g_mutex_init (&content->priv->recent_lock);
}

EMailRemoteContent *
e_mail_remote_content_new (const gchar *config_filename)
{
	EMailRemoteContent *content;

	content = g_object_new (E_TYPE_MAIL_REMOTE_CONTENT, NULL);

	if (config_filename != NULL)
		e_mail_remote_content_set_config_filename (content, config_filename);

	return content;
}

void
e_mail_remote_content_add_site (EMailRemoteContent *content,
				const gchar *site)
{
	g_return_if_fail (E_IS_MAIL_REMOTE_CONTENT (content));
	g_return_if_fail (site != NULL);

	e_mail_remote_content_add (content, "sites", site, content->priv->recent_sites, &content->priv->recent_last_sites);
}

void
e_mail_remote_content_remove_site (EMailRemoteContent *content,
				   const gchar *site)
{
	g_return_if_fail (E_IS_MAIL_REMOTE_CONTENT (content));
	g_return_if_fail (site != NULL);

	e_mail_remote_content_remove (content, "sites", site, content->priv->recent_sites, &content->priv->recent_last_sites);
}

gboolean
e_mail_remote_content_has_site (EMailRemoteContent *content,
				const gchar *site)
{
	GSList *values = NULL;
	gboolean result;

	g_return_val_if_fail (E_IS_MAIL_REMOTE_CONTENT (content), FALSE);
	g_return_val_if_fail (site != NULL, FALSE);

	values = g_slist_prepend (values, (gpointer) site);

	result = e_mail_remote_content_has (content, "sites", values, content->priv->recent_sites, &content->priv->recent_last_sites);

	g_slist_free (values);

	return result;
}

/* Free the result with g_slist_free_full (values, g_free); */
GSList *
e_mail_remote_content_get_sites (EMailRemoteContent *content)
{
	g_return_val_if_fail (E_IS_MAIL_REMOTE_CONTENT (content), NULL);

	return e_mail_remote_content_get (content, "sites", content->priv->recent_sites, &content->priv->recent_last_sites);
}

void
e_mail_remote_content_add_mail (EMailRemoteContent *content,
				const gchar *mail)
{
	g_return_if_fail (E_IS_MAIL_REMOTE_CONTENT (content));
	g_return_if_fail (mail != NULL);

	e_mail_remote_content_add (content, "mails", mail, content->priv->recent_mails, &content->priv->recent_last_mails);
}

void
e_mail_remote_content_remove_mail (EMailRemoteContent *content,
				   const gchar *mail)
{
	g_return_if_fail (E_IS_MAIL_REMOTE_CONTENT (content));
	g_return_if_fail (mail != NULL);

	e_mail_remote_content_remove (content, "mails", mail, content->priv->recent_mails, &content->priv->recent_last_mails);
}

gboolean
e_mail_remote_content_has_mail (EMailRemoteContent *content,
				const gchar *mail)
{
	GSList *values = NULL;
	const gchar *at;
	gboolean result;

	g_return_val_if_fail (E_IS_MAIL_REMOTE_CONTENT (content), FALSE);
	g_return_val_if_fail (mail != NULL, FALSE);

	at = strchr (mail, '@');
	if (at)
		values = g_slist_prepend (values, (gpointer) at);
	values = g_slist_prepend (values, (gpointer) mail);

	result = e_mail_remote_content_has (content, "mails", values, content->priv->recent_mails, &content->priv->recent_last_mails);

	g_slist_free (values);

	return result;
}

/* Free the result with g_slist_free_full (values, g_free); */
GSList *
e_mail_remote_content_get_mails (EMailRemoteContent *content)
{
	g_return_val_if_fail (E_IS_MAIL_REMOTE_CONTENT (content), NULL);

	return e_mail_remote_content_get (content, "mails", content->priv->recent_mails, &content->priv->recent_last_mails);
}
