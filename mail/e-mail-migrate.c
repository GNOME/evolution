/*
 * e-mail-migrate.c
 *
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
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-mail-migrate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <utime.h>
#include <unistd.h>
#include <dirent.h>
#include <regex.h>
#include <errno.h>
#include <ctype.h>

#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include <gtk/gtk.h>

#include <gconf/gconf-client.h>

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

#include <libedataserver/e-xml-utils.h>
#include <libedataserver/e-data-server-util.h>

#include <shell/e-shell.h>
#include <shell/e-shell-migrate.h>

#include <e-util/e-util.h>
#include <libevolution-utils/e-xml-utils.h>

#include <libevolution-utils/e-alert-dialog.h>
#include <e-util/e-util-private.h>
#include <e-util/e-plugin.h>

#include <libemail-utils/e-account-utils.h>
#include <libemail-utils/e-signature-utils.h>

#include <libemail-engine/e-mail-folder-utils.h>

#include "e-mail-backend.h"
#include "em-utils.h"

#define d(x) x

/* 1.4 upgrade functions */

static GtkProgressBar *progress;

static void
em_migrate_set_progress (double percent)
{
	gchar text[5];

	snprintf (text, sizeof (text), "%d%%", (gint) (percent * 100.0f));

	gtk_progress_bar_set_fraction (progress, percent);
	gtk_progress_bar_set_text (progress, text);

	while (gtk_events_pending ())
		gtk_main_iteration ();
}

enum {
	CP_UNIQUE = 0,
	CP_OVERWRITE,
	CP_APPEND
};

static gint open_flags[3] = {
	O_WRONLY | O_CREAT | O_TRUNC,
	O_WRONLY | O_CREAT | O_TRUNC,
	O_WRONLY | O_CREAT | O_APPEND,
};

static gboolean
cp (const gchar *src,
    const gchar *dest,
    gboolean show_progress,
    gint mode)
{
	guchar readbuf[65536];
	gssize nread, nwritten;
	gint errnosav, readfd, writefd;
	gsize total = 0;
	struct stat st;
	struct utimbuf ut;

	/* if the dest file exists and has content, abort - we don't
	 * want to corrupt their existing data */
	if (g_stat (dest, &st) == 0 && st.st_size > 0 && mode == CP_UNIQUE) {
		errno = EEXIST;
		return FALSE;
	}

	if (g_stat (src, &st) == -1
	    || (readfd = g_open (src, O_RDONLY | O_BINARY, 0)) == -1)
		return FALSE;

	if ((writefd = g_open (dest, open_flags[mode] | O_BINARY, 0666)) == -1) {
		errnosav = errno;
		close (readfd);
		errno = errnosav;
		return FALSE;
	}

	do {
		do {
			nread = read (readfd, readbuf, sizeof (readbuf));
		} while (nread == -1 && errno == EINTR);

		if (nread == 0)
			break;
		else if (nread < 0)
			goto exception;

		do {
			nwritten = write (writefd, readbuf, nread);
		} while (nwritten == -1 && errno == EINTR);

		if (nwritten < nread)
			goto exception;

		total += nwritten;
		if (show_progress)
			em_migrate_set_progress (((gdouble) total) / ((gdouble) st.st_size));
	} while (total < st.st_size);

	if (fsync (writefd) == -1)
		goto exception;

	close (readfd);
	if (close (writefd) == -1)
		goto failclose;

	ut.actime = st.st_atime;
	ut.modtime = st.st_mtime;
	utime (dest, &ut);
	chmod (dest, st.st_mode);

	return TRUE;

 exception:

	errnosav = errno;
	close (readfd);
	close (writefd);
	errno = errnosav;

 failclose:

	errnosav = errno;
	unlink (dest);
	errno = errnosav;

	return FALSE;
}

#ifndef G_OS_WIN32

#define SUBFOLDER_DIR_NAME     "subfolders"
#define SUBFOLDER_DIR_NAME_LEN 10

static void
em_update_accounts_2_11 (void)
{
	EAccountList *accounts;
	EIterator *iter;
	gboolean changed = FALSE;

	if (!(accounts = e_get_account_list ()))
		return;

	iter = e_list_get_iterator ((EList *) accounts);
	while (e_iterator_is_valid (iter)) {
		EAccount *account = (EAccount *) e_iterator_get (iter);

		if (g_str_has_prefix (account->source->url, "spool://")) {
			if (g_file_test (account->source->url + 8, G_FILE_TEST_IS_DIR)) {
				gchar *str;

				str = g_strdup_printf (
					"spooldir://%s",
					account->source->url + 8);

				g_free (account->source->url);
				account->source->url = str;
				changed = TRUE;
			}
		}

		e_iterator_next (iter);
	}

	g_object_unref (iter);

	if (changed)
		e_account_list_save (accounts);
}

#endif	/* !G_OS_WIN32 */

static gboolean
emm_setup_initial (const gchar *data_dir)
{
	GDir *dir;
	const gchar *d;
	gchar *local = NULL, *base;
	const gchar * const *language_names;

	/* special-case - this means brand new install of evolution */
	/* FIXME: create default folders and stuff... */

	d(printf("Setting up initial mail tree\n"));

	base = g_build_filename (data_dir, "local", NULL);
	if (g_mkdir_with_parents (base, 0700) == -1 && errno != EEXIST) {
		g_free (base);
		return FALSE;
	}

	/* e.g. try en-AU then en, etc */
	language_names = g_get_language_names ();
	while (*language_names != NULL) {
		local = g_build_filename (
			EVOLUTION_PRIVDATADIR, "default",
			*language_names, "mail", "local", NULL);
		if (g_file_test (local, G_FILE_TEST_EXISTS))
			break;
		g_free (local);
		language_names++;
	}

	/* Make sure we found one. */
	g_return_val_if_fail (*language_names != NULL, FALSE);

	dir = g_dir_open (local, 0, NULL);
	if (dir) {
		while ((d = g_dir_read_name (dir))) {
			gchar *src, *dest;

			src = g_build_filename (local, d, NULL);
			dest = g_build_filename (base, d, NULL);

			cp (src, dest, FALSE, CP_UNIQUE);
			g_free (dest);
			g_free (src);
		}
		g_dir_close (dir);
	}

	g_free (local);
	g_free (base);

	return TRUE;
}

static gboolean
is_in_plugs_list (GSList *list,
                  const gchar *value)
{
	GSList *l;

	for (l = list; l; l = l->next) {
		if (l->data && !strcmp (l->data, value))
			return TRUE;
	}

	return FALSE;
}

/*
 * em_update_message_notify_settings_2_21
 * DBus plugin and sound email notification was merged to
 * mail-notification plugin, so move the options to new locations.
 */
static void
em_update_message_notify_settings_2_21 (void)
{
	GConfClient *client;
	GConfValue  *is_key;
	gboolean dbus, status;
	GSList *list;
	gchar *str;
	gint val;

	client = gconf_client_get_default ();

	is_key = gconf_client_get (
		client,
		"/apps/evolution/eplugin/mail-notification/dbus-enabled", NULL);
	if (is_key) {
		/* already migrated, so do not migrate again */
		gconf_value_free (is_key);
		g_object_unref (client);

		return;
	}

	gconf_client_set_bool (
		client,
		"/apps/evolution/eplugin/mail-notification/status-blink-icon",
		gconf_client_get_bool (
			client,
			"/apps/evolution/mail/notification/blink-status-icon",
			NULL), NULL);
	gconf_client_set_bool (
		client,
		"/apps/evolution/eplugin/mail-notification/status-notification",
		gconf_client_get_bool (
			client,
			"/apps/evolution/mail/notification/notification",
			NULL), NULL);

	list = gconf_client_get_list (
		client, "/apps/evolution/eplugin/disabled",
		GCONF_VALUE_STRING, NULL);
	dbus = !is_in_plugs_list (list, "org.gnome.evolution.new_mail_notify");
	status = !is_in_plugs_list (
		list, "org.gnome.evolution.mail_notification");

	gconf_client_set_bool (
		client,
		"/apps/evolution/eplugin/mail-notification/dbus-enabled",
		dbus, NULL);
	gconf_client_set_bool (
		client,
		"/apps/evolution/eplugin/mail-notification/status-enabled",
		status, NULL);

	if (!status) {
		GSList *plugins, *l;

		plugins = e_plugin_list_plugins ();

		for (l = plugins; l; l = l->next) {
			EPlugin *p = l->data;

			if (p && p->id && !strcmp (p->id,
				"org.gnome.evolution.mail_notification")) {
				e_plugin_enable (p, 1);
				break;
			}
		}

		g_slist_foreach (plugins, (GFunc) g_object_unref, NULL);
		g_slist_free (plugins);
	}

	g_slist_foreach (list, (GFunc) g_free, NULL);
	g_slist_free (list);

	val = gconf_client_get_int (
		client, "/apps/evolution/mail/notify/type", NULL);
	gconf_client_set_bool (
		client,
		"/apps/evolution/eplugin/mail-notification/sound-enabled",
		val == 1 || val == 2, NULL);
	gconf_client_set_bool (
		client,
		"/apps/evolution/eplugin/mail-notification/sound-beep",
		val == 0 || val == 1, NULL);

	str = gconf_client_get_string (
		client, "/apps/evolution/mail/notify/sound", NULL);
	gconf_client_set_string (
		client,
		"/apps/evolution/eplugin/mail-notification/sound-file",
		str ? str : "", NULL);
	g_free (str);

	g_object_unref (client);
}

/* fixing typo in SpamAssassin name */
static void
em_update_sa_junk_setting_2_23 (void)
{
	GConfClient *client;
	GConfValue  *key;

	client = gconf_client_get_default ();

	key = gconf_client_get (
		client, "/apps/evolution/mail/junk/default_plugin", NULL);
	if (key) {
		const gchar *str = gconf_value_get_string (key);

		if (str && strcmp (str, "Spamassasin") == 0)
			gconf_client_set_string (
				client,
				"/apps/evolution/mail/junk/default_plugin",
				"SpamAssassin", NULL);

		gconf_value_free (key);
		g_object_unref (client);

		return;
	}

	g_object_unref (client);
}

static void
em_rename_camel_url_params (CamelURL *url)
{
	/* This list includes known URL parameters from built-in providers
	 * in Camel, as well as from evolution-exchange, evolution-groupwise,
	 * and evolution-mapi.  Add more as needed. */
	static struct {
		const gchar *url_parameter;
		const gchar *property_name;
	} camel_url_conversion[] = {
		{ "account_uid",		"account-uid" },
		{ "ad_auth",			"gc-auth-method" },
		{ "ad_browse",			"gc-allow-browse" },
		{ "ad_expand_groups",		"gc-expand-groups" },
		{ "ad_limit",			"gc-results-limit" },
		{ "ad_server",			"gc-server-name" },
		{ "all_headers",		"fetch-headers" },
		{ "basic_headers",		"fetch-headers" },
		{ "cachedconn"			"concurrent-connections" },
		{ "check_all",			"check-all" },
		{ "check_lsub",			"check-subscribed" },
		{ "command",			"shell-command" },
		{ "delete_after",		"delete-after-days" },
		{ "delete_expunged",		"delete-expunged" },
		{ "disable_extensions",		"disable-extensions" },
		{ "dotfolders",			"use-dot-folders" },
		{ "filter",			"filter-inbox" },
		{ "filter_junk",		"filter-junk" },
		{ "filter_junk_inbox",		"filter-junk-inbox" },
		{ "folder_hierarchy_relative",	"folder-hierarchy-relative" },
		{ "imap_custom_headers",	"fetch-headers-extra" },
		{ "keep_on_server",		"keep-on-server" },
		{ "oab_offline",		"oab-offline" },
		{ "oal_selected",		"oal-selected" },
		{ "offline_sync",		"stay-synchronized" },
		{ "override_namespace",		"use-namespace" },
		{ "owa_path",			"owa-path" },
		{ "owa_url",			"owa-url" },
		{ "password_exp_warn_period",	"password-exp-warn-period" },
		{ "real_junk_path",		"real-junk-path" },
		{ "real_trash_path",		"real-trash-path" },
		{ "show_short_notation",	"short-folder-names" },
		{ "soap_port",			"soap-port" },
		{ "ssl",			"security-method" },
		{ "sync_offline",		"stay-synchronized" },
		{ "use_command",		"use-shell-command" },
		{ "use_idle",			"use-idle" },
		{ "use_lsub",			"use-subscriptions" },
		{ "use_qresync",		"use-qresync" },
		{ "use_ssl",			"security-method" },
		{ "xstatus",			"use-xstatus-headers" }
	};

	const gchar *param;
	const gchar *use_param;
	gint ii;

	for (ii = 0; ii < G_N_ELEMENTS (camel_url_conversion); ii++) {
		const gchar *key;
		gpointer value;

		key = camel_url_conversion[ii].url_parameter;
		value = g_datalist_get_data (&url->params, key);

		if (value == NULL)
			continue;

		g_datalist_remove_no_notify (&url->params, key);

		key = camel_url_conversion[ii].property_name;

		/* Deal with a few special enum cases where
		 * the parameter value also needs renamed. */

		if (strcmp (key, "all_headers") == 0) {
			GEnumClass *enum_class;
			GEnumValue *enum_value;

			enum_class = g_type_class_ref (
				CAMEL_TYPE_FETCH_HEADERS_TYPE);
			enum_value = g_enum_get_value (
				enum_class, CAMEL_FETCH_HEADERS_ALL);
			if (enum_value != NULL) {
				g_free (value);
				value = g_strdup (enum_value->value_nick);
			} else
				g_warn_if_reached ();
			g_type_class_unref (enum_class);
		}

		if (strcmp (key, "basic_headers") == 0) {
			GEnumClass *enum_class;
			GEnumValue *enum_value;

			enum_class = g_type_class_ref (
				CAMEL_TYPE_FETCH_HEADERS_TYPE);
			enum_value = g_enum_get_value (
				enum_class, CAMEL_FETCH_HEADERS_BASIC);
			if (enum_value != NULL) {
				g_free (value);
				value = g_strdup (enum_value->value_nick);
			} else
				g_warn_if_reached ();
			g_type_class_unref (enum_class);
		}

		if (strcmp (key, "imap_custom_headers") == 0)
			g_strdelimit (value, " ", ',');

		if (strcmp (key, "security-method") == 0) {
			CamelNetworkSecurityMethod method;
			GEnumClass *enum_class;
			GEnumValue *enum_value;

			if (strcmp (value, "always") == 0)
				method = CAMEL_NETWORK_SECURITY_METHOD_SSL_ON_ALTERNATE_PORT;
			else if (strcmp (value, "1") == 0)
				method = CAMEL_NETWORK_SECURITY_METHOD_SSL_ON_ALTERNATE_PORT;
			else if (strcmp (value, "when-possible") == 0)
				method = CAMEL_NETWORK_SECURITY_METHOD_STARTTLS_ON_STANDARD_PORT;
			else
				method = CAMEL_NETWORK_SECURITY_METHOD_NONE;

			enum_class = g_type_class_ref (
				CAMEL_TYPE_NETWORK_SECURITY_METHOD);
			enum_value = g_enum_get_value (enum_class, method);
			if (enum_value != NULL) {
				g_free (value);
				value = g_strdup (enum_value->value_nick);
			} else
				g_warn_if_reached ();
			g_type_class_unref (enum_class);
		}

		g_datalist_set_data_full (&url->params, key, value, g_free);
	}

	/* A few more adjustments...
	 *
	 * These are all CAMEL_PROVIDER_CONF_CHECKSPIN settings.  The spin
	 * button value is bound to "param" and the checkbox state is bound
	 * to "use-param".  The "use-param" settings are new.  If "param"
	 * exists but no "use-param", then set "use-param" to "true". */

	param = g_datalist_get_data (&url->params, "gc-results-limit");
	use_param = g_datalist_get_data (&url->params, "use-gc-results-limit");
	if (param != NULL && *param != '\0' && use_param == NULL) {
		g_datalist_set_data_full (
			&url->params, "use-gc-results-limit",
			g_strdup ("true"), (GDestroyNotify) g_free);
	}

	param = g_datalist_get_data (&url->params, "kerberos");
	if (g_strcmp0 (param, "required") == 0) {
		g_datalist_set_data_full (
			&url->params, "kerberos",
			g_strdup ("true"), (GDestroyNotify) g_free);
	}

	param = g_datalist_get_data (
		&url->params, "password-exp-warn-period");
	use_param = g_datalist_get_data (
		&url->params, "use-password-exp-warn-period");
	if (param != NULL && *param != '\0' && use_param == NULL) {
		g_datalist_set_data_full (
			&url->params, "use-password-exp-warn-period",
			g_strdup ("true"), (GDestroyNotify) g_free);
	}

	param = g_datalist_get_data (&url->params, "real-junk-path");
	use_param = g_datalist_get_data (&url->params, "use-real-junk-path");
	if (param != NULL && *param != '\0' && use_param == NULL) {
		g_datalist_set_data_full (
			&url->params, "use-real-junk-path",
			g_strdup ("true"), (GDestroyNotify) g_free);
	}

	param = g_datalist_get_data (&url->params, "real-trash-path");
	use_param = g_datalist_get_data (&url->params, "use-real-trash-path");
	if (param != NULL && *param != '\0' && use_param == NULL) {
		g_datalist_set_data_full (
			&url->params, "use-real-trash-path",
			g_strdup ("true"), (GDestroyNotify) g_free);
	}
}

static void
em_rename_account_params (void)
{
	EAccountList *account_list;
	EIterator *iterator;

	/* XXX As of 3.2, CamelServices store settings in GObject properties,
	 *     not CamelURL parameters.  CamelURL parameters are still used
	 *     for storage in GConf until we can move account information to
	 *     key files, but this is only within Evolution.  Some of the new
	 *     GObject property names differ from the old CamelURL parameter
	 *     names.  This routine renames the CamelURL parameter names to
	 *     the GObject property names for all accounts, both the source
	 *     and tranport URLs. */

	account_list = e_get_account_list ();
	iterator = e_list_get_iterator (E_LIST (account_list));

	while (e_iterator_is_valid (iterator)) {
		EAccount *account;
		CamelURL *url = NULL;

		/* XXX EIterator misuses const. */
		account = (EAccount *) e_iterator_get (iterator);

		if (account->source->url != NULL)
			url = camel_url_new (account->source->url, NULL);

		if (url != NULL) {
			em_rename_camel_url_params (url);
			g_free (account->source->url);
			account->source->url = camel_url_to_string (url, 0);
			camel_url_free (url);
		}

		url = NULL;

		if (account->transport->url != NULL)
			url = camel_url_new (account->transport->url, NULL);

		if (url != NULL) {
			em_rename_camel_url_params (url);
			g_free (account->transport->url);
			account->transport->url = camel_url_to_string (url, 0);
			camel_url_free (url);
		}

		e_iterator_next (iterator);
	}

	g_object_unref (iterator);
	e_account_list_save (account_list);
}

static void
em_ensure_proxy_ignore_hosts_being_list (void)
{
	const gchar *key = "/apps/evolution/shell/network_config/ignore_hosts";
	GConfClient *client;
	GConfValue  *key_value;

	/* Makes sure the 'key' is a list of strings, not a string,
	 * as set by previous versions. */

	client = gconf_client_get_default ();
	key_value = gconf_client_get (client, key, NULL);
	if (key_value && key_value->type == GCONF_VALUE_STRING) {
		gchar *value = gconf_client_get_string (client, key, NULL);
		GSList *lst = NULL;
		GError *error = NULL;

		if (value && *value) {
			gchar **split = g_strsplit (value, ",", -1);

			if (split) {
				gint ii;

				for (ii = 0; split[ii]; ii++) {
					const gchar *tmp = split[ii];

					if (tmp && *tmp) {
						gchar *val = g_strstrip (g_strdup (tmp));

						if (val && *val)
							lst = g_slist_append (lst, val);
						else
							g_free (val);
					}
				}
			}

			g_strfreev (split);
		}

		gconf_client_unset (client, key, NULL);
		gconf_client_set_list (client, key, GCONF_VALUE_STRING, lst, &error);

		g_slist_foreach (lst, (GFunc) g_free, NULL);
		g_slist_free (lst);
		g_free (value);

		if (error) {
			fprintf (
				stderr, "%s: Failed to set a list values "
				"with error: %s\n", G_STRFUNC, error->message);
			g_error_free (error);
		}
	}

	if (key_value)
		gconf_value_free (key_value);
	g_object_unref (client);
}

static void
em_rename_view_in_folder (gpointer data,
                          gpointer user_data)
{
	const gchar *filename = data;
	const gchar *views_dir = user_data;
	gchar *folderpos, *dotpos;

	g_return_if_fail (filename != NULL);
	g_return_if_fail (views_dir != NULL);

	folderpos = strstr (filename, "-folder:__");
	if (!folderpos)
		folderpos = strstr (filename, "-folder___");
	if (!folderpos)
		return;

	/* points on 'f' from the "folder" word */
	folderpos++;
	dotpos = strrchr (filename, '.');
	if (folderpos < dotpos && g_str_equal (dotpos, ".xml")) {
		GChecksum *checksum;
		gchar *oldname, *newname, *newfile;
		const gchar *md5_string;

		*dotpos = 0;

		/* use MD5 checksum of the folder URI, to not depend on its length */
		checksum = g_checksum_new (G_CHECKSUM_MD5);
		g_checksum_update (checksum, (const guchar *) folderpos, -1);

		*folderpos = 0;
		md5_string = g_checksum_get_string (checksum);
		newfile = g_strconcat (filename, md5_string, ".xml", NULL);
		*folderpos = 'f';
		*dotpos = '.';

		oldname = g_build_filename (views_dir, filename, NULL);
		newname = g_build_filename (views_dir, newfile, NULL);

		g_rename (oldname, newname);

		g_checksum_free (checksum);
		g_free (oldname);
		g_free (newname);
		g_free (newfile);
	}
}

static void
em_rename_folder_views (EShellBackend *shell_backend)
{
	const gchar *config_dir;
	gchar *views_dir;
	GDir *dir;

	g_return_if_fail (shell_backend != NULL);

	config_dir = e_shell_backend_get_config_dir (shell_backend);
	views_dir = g_build_filename (config_dir, "views", NULL);

	dir = g_dir_open (views_dir, 0, NULL);
	if (dir) {
		GSList *to_rename = NULL;
		const gchar *filename;

		while (filename = g_dir_read_name (dir), filename) {
			if (strstr (filename, "-folder:__") ||
			    strstr (filename, "-folder___"))
				to_rename = g_slist_prepend (to_rename, g_strdup (filename));
		}

		g_dir_close (dir);

		g_slist_foreach (to_rename, em_rename_view_in_folder, views_dir);
		g_slist_free_full (to_rename, g_free);
	}

	g_free (views_dir);
}

gboolean
e_mail_migrate (EShellBackend *shell_backend,
                gint major,
                gint minor,
                gint micro,
                GError **error)
{
	struct stat st;
	const gchar *data_dir;

	/* make sure ~/.evolution/mail exists */
	data_dir = e_shell_backend_get_data_dir (shell_backend);
	if (g_stat (data_dir, &st) == -1) {
		if (errno != ENOENT || g_mkdir_with_parents (data_dir, 0700) == -1) {
			g_set_error (
				error, E_SHELL_MIGRATE_ERROR,
				E_SHELL_MIGRATE_ERROR_FAILED,
				_("Unable to create local mail folders at "
				"'%s': %s"), data_dir, g_strerror (errno));
			return FALSE;
		}
	}

	if (major == 0)
		return emm_setup_initial (data_dir);

#ifndef G_OS_WIN32
	if (major < 2 || (major == 2 && minor < 12)) {
		em_update_accounts_2_11 ();
	}

	if (major < 2 || (major == 2 && minor < 22))
		em_update_message_notify_settings_2_21 ();

	if (major < 2 || (major == 2 && minor < 24))
		em_update_sa_junk_setting_2_23 ();
#else
	if (major < 2 || (major == 2 && minor < 24))
		g_warning (
			"Upgrading from ancient versions %d.%d "
			"not supported on Windows", major, minor);
#endif

	if (major < 2 || (major == 2 && minor < 32)) {
		em_ensure_proxy_ignore_hosts_being_list ();
	}

	/* Rename account URL parameters to
	 * match CamelSettings property names. */
	em_rename_account_params ();

	if (major <= 2 || (major == 3 && minor < 4))
		em_rename_folder_views (shell_backend);

	return TRUE;
}
