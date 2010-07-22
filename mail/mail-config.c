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
#include <glib/gstdio.h>

#include <libedataserver/e-data-server-util.h>
#include <e-util/e-util.h>
#include "e-util/e-account-utils.h"
#include "e-util/e-signature-utils.h"

#include <gconf/gconf-client.h>

#include "e-mail-local.h"
#include "mail-config.h"
#include "mail-folder-cache.h"
#include "mail-session.h"
#include "mail-tools.h"

typedef struct {
	GConfClient *gconf;

	gchar *gtkrc;

	GSList *labels;

	gboolean address_compress;
	gint address_count;

	GSList *jh_header;
	gboolean jh_check;
	gboolean book_lookup;
	gboolean book_lookup_local_only;
	gboolean scripts_disabled;
} MailConfig;

extern gint camel_header_param_encode_filenames_in_rfc_2047;

static MailConfig *config = NULL;

static void
config_write_style (void)
{
	GConfClient *client;
	gboolean custom;
	gchar *fix_font;
	gchar *var_font;
	gchar *citation_color;
	gchar *spell_color;
	const gchar *key;
	FILE *rc;

	if (!(rc = g_fopen (config->gtkrc, "wt"))) {
		g_warning ("unable to open %s", config->gtkrc);
		return;
	}

	client = config->gconf;

	key = "/apps/evolution/mail/display/fonts/use_custom";
	custom = gconf_client_get_bool (client, key, NULL);

	key = "/apps/evolution/mail/display/fonts/variable";
	var_font = gconf_client_get_string (client, key, NULL);

	key = "/apps/evolution/mail/display/fonts/monospace";
	fix_font = gconf_client_get_string (client, key, NULL);

	key = "/apps/evolution/mail/display/citation_colour";
	citation_color = gconf_client_get_string (client, key, NULL);

	key = "/apps/evolution/mail/composer/spell_color";
	spell_color = gconf_client_get_string (client, key, NULL);

	fprintf (rc, "style \"evolution-mail-custom-fonts\" {\n");
	fprintf (rc, "        GtkHTML::spell_error_color = \"%s\"\n", spell_color);
	g_free (spell_color);

	if (gconf_client_get_bool (config->gconf, "/apps/evolution/mail/display/mark_citations", NULL))
		fprintf (rc, "        GtkHTML::cite_color = \"%s\"\n",
			 citation_color);
	g_free (citation_color);

	if (custom && var_font && fix_font) {
		fprintf (rc,
			 "        GtkHTML::fixed_font_name = \"%s\"\n"
			 "        font_name = \"%s\"\n",
			 fix_font, var_font);
	}
	g_free (fix_font);
	g_free (var_font);

	fprintf (rc, "}\n\n");

	fprintf (rc, "class \"EWebView\" style \"evolution-mail-custom-fonts\"\n");
	fflush (rc);
	fclose (rc);

	gtk_rc_reparse_all ();
}

static void
gconf_style_changed (GConfClient *client, guint cnxn_id,
		     GConfEntry *entry, gpointer user_data)
{
	config_write_style ();
}

static void
gconf_outlook_filenames_changed (GConfClient *client, guint cnxn_id,
				 GConfEntry *entry, gpointer user_data)
{
	g_return_if_fail (client != NULL);

	/* pass option to the camel */
	if (gconf_client_get_bool (client, "/apps/evolution/mail/composer/outlook_filenames", NULL)) {
		camel_header_param_encode_filenames_in_rfc_2047 = 1;
	} else {
		camel_header_param_encode_filenames_in_rfc_2047 = 0;
	}
}

static void
gconf_jh_headers_changed (GConfClient *client, guint cnxn_id,
                          GConfEntry *entry, gpointer user_data)
{
	GSList *node;
	GPtrArray *name, *value;

	g_slist_foreach (config->jh_header, (GFunc) g_free, NULL);
	g_slist_free (config->jh_header);

	config->jh_header = gconf_client_get_list (config->gconf, "/apps/evolution/mail/junk/custom_header", GCONF_VALUE_STRING, NULL);

	node = config->jh_header;
	name = g_ptr_array_new ();
	value = g_ptr_array_new ();
	while (node && node->data) {
		gchar **tok = g_strsplit (node->data, "=", 2);
		g_ptr_array_add (name, g_strdup(tok[0]));
		g_ptr_array_add (value, g_strdup(tok[1]));
		node = node->next;
		g_strfreev (tok);
	}
	mail_session_set_junk_headers ((const gchar **)name->pdata, (const gchar **)value->pdata, name->len);

	g_ptr_array_foreach (name, (GFunc) g_free, NULL);
	g_ptr_array_foreach (value, (GFunc) g_free, NULL);
	g_ptr_array_free (name, TRUE);
	g_ptr_array_free (value, TRUE);
}

static void
gconf_jh_check_changed (GConfClient *client, guint cnxn_id,
		     GConfEntry *entry, gpointer user_data)
{
	config->jh_check = gconf_client_get_bool (config->gconf, "/apps/evolution/mail/junk/check_custom_header", NULL);
	if (!config->jh_check) {
		mail_session_set_junk_headers (NULL, NULL, 0);
	} else {
		gconf_jh_headers_changed (NULL, 0, NULL, NULL);
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
	EAccountList *account_list;
	ESignatureList *signature_list;

	if (!config)
		return;

	account_list = e_get_account_list ();
	signature_list = e_get_signature_list ();

	e_account_list_save (account_list);
	e_signature_list_save (signature_list);

	gconf_client_suggest_sync (config->gconf, NULL);
}

/* Accessor functions */
GConfClient *
mail_config_get_gconf_client (void)
{
	if (!config)
		mail_config_init ();

	return config->gconf;
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
	GConfClient *gconf = mail_config_get_gconf_client ();
	gint res = 60;

	if (gconf) {
		GError *error = NULL;

		res = gconf_client_get_int (gconf, "/apps/evolution/mail/sync_interval", &error);

		/* do not allow recheck sooner than every 30 seconds */
		if (error || res == 0)
			res = 60;
		else if (res < 30)
			res = 30;

		if (error)
			g_error_free (error);
	}

	return res;
}

static EAccount *
mc_get_account_by (const gchar *given_url, const gchar * (get_url_string)(EAccount *account))
{
	EAccountList *account_list;
	EAccount *account = NULL;
	EIterator *iter;
	CamelURL *url;
	CamelProvider *provider;

	g_return_val_if_fail (given_url != NULL, NULL);
	g_return_val_if_fail (get_url_string != NULL, NULL);

	url = camel_url_new (given_url, NULL);
	g_return_val_if_fail (url != NULL, NULL);

	provider = camel_provider_get (given_url, NULL);
	g_return_val_if_fail (provider != NULL && provider->url_equal != NULL, NULL);

	account_list = e_get_account_list ();
	iter = e_list_get_iterator ((EList *) account_list);
	while (account == NULL && e_iterator_is_valid (iter)) {
		CamelURL *account_url;
		const gchar *account_url_string;

		account = (EAccount *) e_iterator_get (iter);

		e_iterator_next (iter);

		account_url_string = get_url_string (account);

		if ( !account_url_string || !*account_url_string) {
			account = NULL;
			continue;
		}

		account_url = camel_url_new (account_url_string, NULL);
		if (account_url == NULL) {
			account = NULL;
			continue;
		}

		if (!provider->url_equal (url, account_url))
			account = NULL;  /* not a match */

		camel_url_free (account_url);
	}

	g_object_unref (iter);
	camel_url_free (url);

	return account;
}

static const gchar *
get_source_url_string (EAccount *account)
{
	if (account && account->source && account->source->url && *account->source->url)
		return account->source->url;
	return NULL;
}

static const gchar *
get_transport_url_string (EAccount *account)
{
	if (account && account->transport && account->transport->url && *account->transport->url)
		return account->transport->url;
	return NULL;
}

EAccount *
mail_config_get_account_by_source_url (const gchar *source_url)
{
	return mc_get_account_by (source_url, get_source_url_string);
}

EAccount *
mail_config_get_account_by_transport_url (const gchar *transport_url)
{
	return mc_get_account_by (transport_url, get_transport_url_string);
}

EAccountService *
mail_config_get_default_transport (void)
{
	EAccountList *account_list;
	EAccount *account;
	EIterator *iter;

	account_list = e_get_account_list ();
	account = e_get_default_account ();

	if (account && account->enabled && account->transport && account->transport->url && account->transport->url[0])
		return account->transport;

	/* return the first account with a transport? */
	iter = e_list_get_iterator ((EList *) account_list);
	while (e_iterator_is_valid (iter)) {
		account = (EAccount *) e_iterator_get (iter);

		if (account->enabled && account->transport && account->transport->url && account->transport->url[0]) {
			g_object_unref (iter);

			return account->transport;
		}

		e_iterator_next (iter);
	}

	g_object_unref (iter);

	return NULL;
}

static gchar *
uri_to_evname (const gchar *uri, const gchar *prefix)
{
	const gchar *data_dir;
	gchar *safe;
	gchar *tmp;

	data_dir = mail_session_get_data_dir ();

	safe = g_strdup (uri);
	e_filename_make_safe (safe);
	/* blah, easiest thing to do */
	if (prefix[0] == '*')
		tmp = g_strdup_printf ("%s/%s%s.xml", data_dir, prefix + 1, safe);
	else
		tmp = g_strdup_printf ("%s/%s%s", data_dir, prefix, safe);
	g_free (safe);
	return tmp;
}

static void
mail_config_uri_renamed (GCompareFunc uri_cmp, const gchar *old, const gchar *new)
{
	EAccountList *account_list;
	EAccount *account;
	EIterator *iter;
	gint i, work = 0;
	gchar *oldname, *newname;
	const gchar *cachenames[] = {
		"config/hidestate-",
		"config/et-expanded-",
		"config/et-header-",
		"*views/current_view-",
		"*views/custom_view-",
		NULL };

	account_list = e_get_account_list ();
	iter = e_list_get_iterator ((EList *) account_list);
	while (e_iterator_is_valid (iter)) {
		account = (EAccount *) e_iterator_get (iter);

		if (account->sent_folder_uri && uri_cmp (account->sent_folder_uri, old)) {
			g_free (account->sent_folder_uri);
			account->sent_folder_uri = g_strdup (new);
			work = 1;
		}

		if (account->drafts_folder_uri && uri_cmp (account->drafts_folder_uri, old)) {
			g_free (account->drafts_folder_uri);
			account->drafts_folder_uri = g_strdup (new);
			work = 1;
		}

		e_iterator_next (iter);
	}

	g_object_unref (iter);

	/* ignore return values or if the files exist or
	 * not, doesn't matter */

	for (i = 0; cachenames[i]; i++) {
		oldname = uri_to_evname (old, cachenames[i]);
		newname = uri_to_evname (new, cachenames[i]);
		/*printf ("** renaming %s to %s\n", oldname, newname);*/
		g_rename (oldname, newname);
		g_free (oldname);
		g_free (newname);
	}

	/* nasty ... */
	if (work)
		mail_config_write ();
}

static void
mail_config_uri_deleted (GCompareFunc uri_cmp, const gchar *uri)
{
	EAccountList *account_list;
	EAccount *account;
	EIterator *iter;
	gint work = 0;
	const gchar *local_drafts_folder_uri;
	const gchar *local_sent_folder_uri;

	/* assumes these can't be removed ... */
	local_drafts_folder_uri =
		e_mail_local_get_folder_uri (E_MAIL_FOLDER_DRAFTS);
	local_sent_folder_uri =
		e_mail_local_get_folder_uri (E_MAIL_FOLDER_SENT);

	account_list = e_get_account_list ();
	iter = e_list_get_iterator ((EList *) account_list);
	while (e_iterator_is_valid (iter)) {
		account = (EAccount *) e_iterator_get (iter);

		if (account->sent_folder_uri && uri_cmp (account->sent_folder_uri, uri)) {
			g_free (account->sent_folder_uri);
			account->sent_folder_uri = g_strdup (local_sent_folder_uri);
			work = 1;
		}

		if (account->drafts_folder_uri && uri_cmp (account->drafts_folder_uri, uri)) {
			g_free (account->drafts_folder_uri);
			account->drafts_folder_uri = g_strdup (local_drafts_folder_uri);
			work = 1;
		}

		e_iterator_next (iter);
	}

	/* nasty again */
	if (work)
		mail_config_write ();
}

void
mail_config_service_set_save_passwd (EAccountService *service, gboolean save_passwd)
{
	service->save_passwd = save_passwd;
}

gchar *
mail_config_folder_to_safe_url (CamelFolder *folder)
{
	gchar *url;

	url = mail_tools_folder_to_url (folder);
	e_filename_make_safe (url);

	return url;
}

gchar *
mail_config_folder_to_cachename (CamelFolder *folder, const gchar *prefix)
{
	gchar *url, *basename, *filename;
	const gchar *config_dir;

	config_dir = mail_session_get_config_dir ();
	url = mail_config_folder_to_safe_url (folder);
	basename = g_strdup_printf ("%s%s", prefix, url);
	filename = g_build_filename (config_dir, "folders", basename, NULL);
	g_free (basename);
	g_free (url);

	return filename;
}

void
mail_config_reload_junk_headers (void)
{
	/* It automatically sets in the session */
	if (config == NULL)
		mail_config_init ();
	else
		gconf_jh_check_changed (config->gconf, 0, NULL, config);

}

gboolean
mail_config_get_lookup_book (void)
{
	/* It automatically sets in the session */
	if (config == NULL)
		mail_config_init ();

	return config->book_lookup;
}

gboolean
mail_config_get_lookup_book_local_only (void)
{
	/* It automatically sets in the session */
	if (config == NULL)
		mail_config_init ();

	return config->book_lookup_local_only;
}

static void
folder_deleted_cb (MailFolderCache *cache, CamelStore *store, const gchar *uri, gpointer user_data)
{
	mail_config_uri_deleted(CAMEL_STORE_CLASS(CAMEL_OBJECT_GET_CLASS(store))->compare_folder_name, uri);
}

static void
folder_renamed_cb (MailFolderCache *cache, CamelStore *store, const gchar *olduri, const gchar *newuri, gpointer user_data)
{
	mail_config_uri_renamed(CAMEL_STORE_CLASS(CAMEL_OBJECT_GET_CLASS(store))->compare_folder_name,
				olduri, newuri);
}

/* Config struct routines */
void
mail_config_init (void)
{
	GConfClientNotifyFunc func;
	const gchar *key;

	if (config)
		return;

	config = g_new0 (MailConfig, 1);
	config->gconf = gconf_client_get_default ();
	config->gtkrc = g_build_filename (
		mail_session_get_config_dir (),
		"gtkrc-mail-fonts", NULL);

	gtk_rc_parse (config->gtkrc);

	gconf_client_add_dir (
		config->gconf, "/apps/evolution/mail/prompts",
		GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

	/* Composer Configuration */

	gconf_client_add_dir (
		config->gconf, "/apps/evolution/mail/composer",
		GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

	key = "/apps/evolution/mail/composer/spell_color";
	func = (GConfClientNotifyFunc) gconf_style_changed;
	gconf_client_notify_add (
		config->gconf, key, func, NULL, NULL, NULL);

	key = "/apps/evolution/mail/composer/outlook_filenames";
	func = (GConfClientNotifyFunc) gconf_outlook_filenames_changed;
	gconf_outlook_filenames_changed (config->gconf, 0, NULL, NULL);
	gconf_client_notify_add (
		config->gconf, key, func, NULL, NULL, NULL);

	/* Display Configuration */

	gconf_client_add_dir (
		config->gconf, "/apps/evolution/mail/display",
		GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

	key = "/apps/evolution/mail/display/address_compress";
	func = (GConfClientNotifyFunc) gconf_bool_value_changed;
	gconf_client_notify_add (
		config->gconf, key, func,
		&config->address_compress, NULL, NULL);
	config->address_compress =
		gconf_client_get_bool (config->gconf, key, NULL);

	key = "/apps/evolution/mail/display/address_count";
	func = (GConfClientNotifyFunc) gconf_int_value_changed;
	gconf_client_notify_add (
		config->gconf, key, func,
		&config->address_count, NULL, NULL);
	config->address_count =
		gconf_client_get_int (config->gconf, key, NULL);

	key = "/apps/evolution/mail/display/citation_colour";
	func = (GConfClientNotifyFunc) gconf_style_changed;
	gconf_client_notify_add (
		config->gconf, key, func, NULL, NULL, NULL);

	key = "/apps/evolution/mail/display/mark_citations";
	func = (GConfClientNotifyFunc) gconf_style_changed;
	gconf_client_notify_add (
		config->gconf, key, func, NULL, NULL, NULL);

	/* Font Configuration */

	gconf_client_add_dir (
		config->gconf, "/apps/evolution/mail/display/fonts",
		GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

	key = "/apps/evolution/mail/display/fonts";
	func = (GConfClientNotifyFunc) gconf_style_changed;
	gconf_client_notify_add (
		config->gconf, key, func, NULL, NULL, NULL);

	/* Junk Configuration */

	gconf_client_add_dir (
		config->gconf, "/apps/evolution/mail/junk",
		GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

	key = "/apps/evolution/mail/junk/check_custom_header";
	func = (GConfClientNotifyFunc) gconf_jh_check_changed;
	gconf_client_notify_add (
		config->gconf, key, func, NULL, NULL, NULL);
	config->jh_check =
		gconf_client_get_bool (config->gconf, key, NULL);

	key = "/apps/evolution/mail/junk/custom_header";
	func = (GConfClientNotifyFunc) gconf_jh_headers_changed;
	gconf_client_notify_add (
		config->gconf, key, func, NULL, NULL, NULL);

	key = "/apps/evolution/mail/junk/lookup_addressbook";
	func = (GConfClientNotifyFunc) gconf_bool_value_changed;
	gconf_client_notify_add (
		config->gconf, key, func,
		&config->book_lookup, NULL, NULL);
	config->book_lookup =
		gconf_client_get_bool (config->gconf, key, NULL);

	key = "/apps/evolution/mail/junk/lookup_addressbook_local_only";
	func = (GConfClientNotifyFunc) gconf_bool_value_changed;
	gconf_client_notify_add (
		config->gconf, key, func,
		&config->book_lookup_local_only, NULL, NULL);
	config->book_lookup_local_only =
		gconf_client_get_bool (config->gconf, key, NULL);

	key = "/desktop/gnome/lockdown/disable_command_line";
	func = (GConfClientNotifyFunc) gconf_bool_value_changed;
	gconf_client_notify_add (
		config->gconf, key, func,
		&config->scripts_disabled, NULL, NULL);
	config->scripts_disabled =
		gconf_client_get_bool (config->gconf, key, NULL);

	gconf_jh_check_changed (config->gconf, 0, NULL, config);

	g_signal_connect (mail_folder_cache_get_default (), "folder-deleted",
			  (GCallback) folder_deleted_cb, NULL);
	g_signal_connect (mail_folder_cache_get_default (), "folder-renamed",
			  (GCallback) folder_renamed_cb, NULL);
}
