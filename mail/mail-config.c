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
 * Authors:
 *   Jeffrey Stedfast <fejj@ximian.com>
 *   Radek Doulik <rodo@ximian.com>
 *   Jonathon Jongsma <jonathon.jongsma@collabora.co.uk>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 * Copyright (C) 2009 Intel Corporation
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>

#include <libedataserver/e-data-server-util.h>
#include <e-util/e-util.h>
#include "e-util/e-account-utils.h"
#include "e-util/e-signature-utils.h"

#include <gconf/gconf-client.h>

#include "e-mail-local.h"
#include "e-mail-folder-utils.h"
#include "mail-config.h"
#include "mail-tools.h"

typedef struct {
	GSList *labels;

	gboolean address_compress;
	gint address_count;

	GSList *jh_header;
	gboolean jh_check;
	gboolean book_lookup;
	gboolean book_lookup_local_only;
} MailConfig;

extern gint camel_header_param_encode_filenames_in_rfc_2047;

static MailConfig *config = NULL;

static void
gconf_outlook_filenames_changed (GConfClient *client,
                                 guint cnxn_id,
                                 GConfEntry *entry,
                                 gpointer user_data)
{
	const gchar *key;

	g_return_if_fail (client != NULL);

	key = "/apps/evolution/mail/composer/outlook_filenames";

	/* pass option to the camel */
	if (gconf_client_get_bool (client, key, NULL))
		camel_header_param_encode_filenames_in_rfc_2047 = 1;
	else
		camel_header_param_encode_filenames_in_rfc_2047 = 0;
}

static void
gconf_jh_headers_changed (GConfClient *client,
                          guint cnxn_id,
                          GConfEntry *entry,
                          EMailSession *session)
{
	GSList *node;
	GPtrArray *name, *value;

	g_slist_foreach (config->jh_header, (GFunc) g_free, NULL);
	g_slist_free (config->jh_header);

	config->jh_header = gconf_client_get_list (
		client, "/apps/evolution/mail/junk/custom_header",
		GCONF_VALUE_STRING, NULL);

	node = config->jh_header;
	name = g_ptr_array_new ();
	value = g_ptr_array_new ();
	while (node && node->data) {
		gchar **tok = g_strsplit (node->data, "=", 2);
		g_ptr_array_add (name, g_strdup (tok[0]));
		g_ptr_array_add (value, g_strdup (tok[1]));
		node = node->next;
		g_strfreev (tok);
	}
	camel_session_set_junk_headers (
		CAMEL_SESSION (session),
		(const gchar **) name->pdata,
		(const gchar **) value->pdata, name->len);

	g_ptr_array_foreach (name, (GFunc) g_free, NULL);
	g_ptr_array_foreach (value, (GFunc) g_free, NULL);
	g_ptr_array_free (name, TRUE);
	g_ptr_array_free (value, TRUE);
}

static void
gconf_jh_check_changed (GConfClient *client,
                        guint cnxn_id,
                        GConfEntry *entry,
                        EMailSession *session)
{
	config->jh_check = gconf_client_get_bool (
		client, "/apps/evolution/mail/junk/check_custom_header", NULL);
	if (!config->jh_check) {
		camel_session_set_junk_headers (
			CAMEL_SESSION (session), NULL, NULL, 0);
	} else {
		gconf_jh_headers_changed (client, 0, NULL, session);
	}
}

static void
gconf_bool_value_changed (GConfClient *client,
                          guint cnxn_id,
                          GConfEntry *entry,
                          gboolean *save_location)
{
	GError *error = NULL;

	*save_location = gconf_client_get_bool (client, entry->key, &error);
	if (error != NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}
}

static void
gconf_int_value_changed (GConfClient *client,
                         guint cnxn_id,
                         GConfEntry *entry,
                         gint *save_location)
{
	GError *error = NULL;

	*save_location = gconf_client_get_int (client, entry->key, &error);
	if (error != NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}
}

void
mail_config_write (void)
{
	GConfClient *client;
	EAccountList *account_list;
	ESignatureList *signature_list;

	if (!config)
		return;

	account_list = e_get_account_list ();
	signature_list = e_get_signature_list ();

	e_account_list_save (account_list);
	e_signature_list_save (signature_list);

	client = gconf_client_get_default ();
	gconf_client_suggest_sync (client, NULL);
	g_object_unref (client);
}

gint
mail_config_get_address_count (void)
{
	if (!config->address_compress)
		return -1;

	return config->address_count;
}

/* timeout interval, in seconds, when to call server update */
gint
mail_config_get_sync_timeout (void)
{
	GConfClient *client;
	gint res = 60;
	GError *error = NULL;

	client = gconf_client_get_default ();

	res = gconf_client_get_int (
		client, "/apps/evolution/mail/sync_interval", &error);

	/* do not allow recheck sooner than every 30 seconds */
	if (error || res == 0)
		res = 60;
	else if (res < 30)
		res = 30;

	if (error)
		g_error_free (error);

	return res;
}

gchar *
mail_config_folder_to_cachename (CamelFolder *folder,
                                 const gchar *prefix)
{
	gchar *folder_uri, *basename, *filename;
	const gchar *config_dir;

	config_dir = mail_session_get_config_dir ();

	basename = g_build_filename (config_dir, "folders", NULL);
	if (!g_file_test (basename, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
		/* create the folder if does not exist */
		g_mkdir_with_parents (basename, 0700);
	}
	g_free (basename);

	folder_uri = e_mail_folder_uri_from_folder (folder);
	e_filename_make_safe (folder_uri);
	basename = g_strdup_printf ("%s%s", prefix, folder_uri);
	filename = g_build_filename (config_dir, "folders", basename, NULL);
	g_free (basename);
	g_free (folder_uri);

	return filename;
}

void
mail_config_reload_junk_headers (EMailSession *session)
{
	g_return_if_fail (E_IS_MAIL_SESSION (session));

	/* It automatically sets in the session */
	if (config == NULL)
		mail_config_init (session);
	else {
		GConfClient *client;

		client = gconf_client_get_default ();
		gconf_jh_check_changed (client, 0, NULL, session);
		g_object_unref (client);
	}
}

gboolean
mail_config_get_lookup_book (void)
{
	g_return_val_if_fail (config != NULL, FALSE);

	return config->book_lookup;
}

gboolean
mail_config_get_lookup_book_local_only (void)
{
	g_return_val_if_fail (config != NULL, FALSE);

	return config->book_lookup_local_only;
}

/* Config struct routines */
void
mail_config_init (EMailSession *session)
{
	GConfClient *client;
	GConfClientNotifyFunc func;
	const gchar *key;

	g_return_if_fail (E_IS_MAIL_SESSION (session));

	if (config)
		return;

	config = g_new0 (MailConfig, 1);

	client = gconf_client_get_default ();

	gconf_client_add_dir (
		client, "/apps/evolution/mail/prompts",
		GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

	/* Composer Configuration */

	gconf_client_add_dir (
		client, "/apps/evolution/mail/composer",
		GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

	key = "/apps/evolution/mail/composer/outlook_filenames";
	func = (GConfClientNotifyFunc) gconf_outlook_filenames_changed;
	gconf_outlook_filenames_changed (client, 0, NULL, NULL);
	gconf_client_notify_add (client, key, func, NULL, NULL, NULL);

	/* Display Configuration */

	gconf_client_add_dir (
		client, "/apps/evolution/mail/display",
		GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

	key = "/apps/evolution/mail/display/address_compress";
	func = (GConfClientNotifyFunc) gconf_bool_value_changed;
	gconf_client_notify_add (
		client, key, func,
		&config->address_compress, NULL, NULL);
	config->address_compress = gconf_client_get_bool (client, key, NULL);

	key = "/apps/evolution/mail/display/address_count";
	func = (GConfClientNotifyFunc) gconf_int_value_changed;
	gconf_client_notify_add (
		client, key, func,
		&config->address_count, NULL, NULL);
	config->address_count = gconf_client_get_int (client, key, NULL);

	/* Font Configuration */

	gconf_client_add_dir (
		client, "/apps/evolution/mail/display/fonts",
		GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

	/* Junk Configuration */

	gconf_client_add_dir (
		client, "/apps/evolution/mail/junk",
		GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

	key = "/apps/evolution/mail/junk/check_custom_header";
	func = (GConfClientNotifyFunc) gconf_jh_check_changed;
	gconf_client_notify_add (client, key, func, session, NULL, NULL);
	config->jh_check = gconf_client_get_bool (client, key, NULL);

	key = "/apps/evolution/mail/junk/custom_header";
	func = (GConfClientNotifyFunc) gconf_jh_headers_changed;
	gconf_client_notify_add (client, key, func, session, NULL, NULL);

	key = "/apps/evolution/mail/junk/lookup_addressbook";
	func = (GConfClientNotifyFunc) gconf_bool_value_changed;
	gconf_client_notify_add (
		client, key, func,
		&config->book_lookup, NULL, NULL);
	config->book_lookup = gconf_client_get_bool (client, key, NULL);

	key = "/apps/evolution/mail/junk/lookup_addressbook_local_only";
	func = (GConfClientNotifyFunc) gconf_bool_value_changed;
	gconf_client_notify_add (
		client, key, func,
		&config->book_lookup_local_only, NULL, NULL);
	config->book_lookup_local_only =
		gconf_client_get_bool (client, key, NULL);

	gconf_jh_check_changed (client, 0, NULL, session);

	g_object_unref (client);
}
