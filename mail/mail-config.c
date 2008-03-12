/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *           Radek Doulik     <rodo@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
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
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

#include <string.h>
#include <ctype.h>

#include <glib.h>
#include <glib/gstdio.h>

#ifndef G_OS_WIN32
#include <sys/wait.h>
#endif

#include <gtk/gtkdialog.h>
#include <gtkhtml/gtkhtml.h>
#include <glade/glade.h>

#include <libxml/tree.h>
#include <libxml/parser.h>

#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-context.h>
#include <bonobo/bonobo-moniker-util.h>
#include <bonobo/bonobo-exception.h>

#include <libedataserver/e-data-server-util.h>
#include <e-util/e-util.h>
#include <misc/e-gui-utils.h>
#include "e-util/e-util-labels.h"

#include <libedataserver/e-account-list.h>
#include <e-util/e-signature-list.h>

#include <camel/camel-service.h>
#include <camel/camel-stream-mem.h>
#include <camel/camel-stream-fs.h>
#include <camel/camel-mime-filter-charset.h>
#include <camel/camel-stream-filter.h>

#include <libedataserverui/e-passwords.h>

#include "mail-component.h"
#include "mail-session.h"
#include "mail-config.h"
#include "mail-mt.h"
#include "mail-tools.h"

typedef struct {
	GConfClient *gconf;

	gboolean corrupt;

	char *gtkrc;

	EAccountList *accounts;
	ESignatureList *signatures;

	GSList *labels;
	guint label_notify_id;

	guint font_notify_id;
	guint spell_notify_id;
	guint mark_citations__notify_id;
	guint citation_colour_notify_id;
	guint address_count_notify_id;
	guint address_compress_notify_id;
	gboolean address_compress;
	gint address_count;
	guint mlimit_size_notify_id;
	guint mlimit_notify_id;
	gboolean mlimit;
	gint mlimit_size;
	guint magic_spacebar_notify_id;
	gboolean magic_spacebar;
	guint error_time;
	guint error_notify_id;
	guint error_level;
	guint error_level_id;

	GPtrArray *mime_types;
	guint mime_types_notify_id;

	GSList *jh_header;
	gboolean jh_check;
	guint jh_header_id;
	guint jh_check_id;
	gboolean book_lookup;
	guint book_lookup_id;
} MailConfig;

static MailConfig *config = NULL;
static guint config_write_timeout = 0;


void
mail_config_save_accounts (void)
{
	e_account_list_save (config->accounts);
}

void
mail_config_save_signatures (void)
{
	e_signature_list_save (config->signatures);
}

static void
config_clear_mime_types (void)
{
	int i;

	for (i = 0; i < config->mime_types->len; i++)
		g_free (config->mime_types->pdata[i]);

	g_ptr_array_set_size (config->mime_types, 0);
}

static void
config_cache_mime_types (void)
{
	GSList *n, *nn;

	n = gconf_client_get_list (config->gconf, "/apps/evolution/mail/display/mime_types", GCONF_VALUE_STRING, NULL);
	while (n != NULL) {
		nn = n->next;
		g_ptr_array_add (config->mime_types, n->data);
		g_slist_free_1 (n);
		n = nn;
	}

	g_ptr_array_add (config->mime_types, NULL);
}

#define CONFIG_GET_SPELL_VALUE(t,x,prop,f,c) G_STMT_START { \
        val = gconf_client_get_without_default (config->gconf, "/GNOME/Spell" x, NULL); \
        if (val) { f; prop = c (gconf_value_get_ ## t (val)); \
        gconf_value_free (val); } } G_STMT_END

static void
config_write_style (void)
{
	int red = 0xffff, green = 0, blue = 0;
	GConfValue *val;
	gboolean custom;
	char *fix_font;
	char *var_font;
	char *citation_color;
	FILE *rc;

	if (!(rc = g_fopen (config->gtkrc, "wt"))) {
		g_warning ("unable to open %s", config->gtkrc);
		return;
	}

	custom = gconf_client_get_bool (config->gconf, "/apps/evolution/mail/display/fonts/use_custom", NULL);
	var_font = gconf_client_get_string (config->gconf, "/apps/evolution/mail/display/fonts/variable", NULL);
	fix_font = gconf_client_get_string (config->gconf, "/apps/evolution/mail/display/fonts/monospace", NULL);
	citation_color = gconf_client_get_string (config->gconf, "/apps/evolution/mail/display/citation_colour", NULL);
 	CONFIG_GET_SPELL_VALUE (int, "/spell_error_color_red",   red, (void)0, (int));
 	CONFIG_GET_SPELL_VALUE (int, "/spell_error_color_green", green, (void)0, (int));
 	CONFIG_GET_SPELL_VALUE (int, "/spell_error_color_blue",  blue, (void)0, (int));

	fprintf (rc, "style \"evolution-mail-custom-fonts\" {\n");
	fprintf (rc, "        GtkHTML::spell_error_color = \"#%02x%02x%02x\"\n",
		 0xff & (red >> 8), 0xff & (green >> 8), 0xff & (blue >> 8));

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

	fprintf (rc, "widget \"*.EMFolderView.*.GtkHTML\" style \"evolution-mail-custom-fonts\"\n");
	fprintf (rc, "widget \"*.EMFolderBrowser.*.GtkHTML\" style \"evolution-mail-custom-fonts\"\n");
	fprintf (rc, "widget \"*.EMMessageBrowser.*.GtkHTML\" style \"evolution-mail-custom-fonts\"\n");
	fprintf (rc, "widget \"*.BonoboPlug.*.GtkHTML\" style \"evolution-mail-custom-fonts\"\n");
	fprintf (rc, "widget \"*.EvolutionMailPrintHTMLWidget\" style \"evolution-mail-custom-fonts\"\n");
	fflush (rc);
	fclose (rc);

	gtk_rc_reparse_all ();
}

static void
config_clear_labels (void)
{
	if (!config)
		return;

	e_util_labels_free (config->labels);
	config->labels = NULL;
}

static void
config_cache_labels (GConfClient *client)
{
	if (!config)
		return;

	config->labels = e_util_labels_parse (client);
}

static void
gconf_labels_changed (GConfClient *client, guint cnxn_id,
		      GConfEntry *entry, gpointer user_data)
{
	config_clear_labels ();
	config_cache_labels (client);
}

static void
gconf_style_changed (GConfClient *client, guint cnxn_id,
		     GConfEntry *entry, gpointer user_data)
{
	config_write_style ();
}

static void
gconf_jh_check_changed (GConfClient *client, guint cnxn_id,
		     GConfEntry *entry, gpointer user_data)
{
	config->jh_check = gconf_client_get_bool (config->gconf, "/apps/evolution/mail/junk/check_custom_header", NULL);
	if (!config->jh_check) {
		mail_session_set_junk_headers (NULL, NULL, 0);
	} else {
		config->jh_header = gconf_client_get_list (config->gconf, "/apps/evolution/mail/junk/custom_header", GCONF_VALUE_STRING, NULL);
		GSList *node = config->jh_header;
		GPtrArray *name, *value;
		name = g_ptr_array_new ();
		value = g_ptr_array_new ();
		while (node && node->data) {
			char **tok = g_strsplit (node->data, "=", 2);
			g_ptr_array_add (name, g_strdup(tok[0]));
			g_ptr_array_add (value, g_strdup(tok[1]));
			node = node->next;
			g_strfreev (tok);
		}
		mail_session_set_junk_headers ((const char **)name->pdata, (const char **)value->pdata, name->len);
		g_ptr_array_free (name, TRUE);
		g_ptr_array_free (value, TRUE);
	}
}

static void
gconf_lookup_book_changed (GConfClient *client, guint cnxn_id,
			  GConfEntry *entry, gpointer data)
{
	config->book_lookup = gconf_client_get_bool (config->gconf, "/apps/evolution/mail/junk/lookup_addressbook", NULL);
}

static void
gconf_jh_headers_changed (GConfClient *client, guint cnxn_id,
		     GConfEntry *entry, gpointer user_data)
{
	config->jh_header = gconf_client_get_list (config->gconf, "/apps/evolution/mail/junk/custom_header", GCONF_VALUE_STRING, NULL);
	GSList *node = config->jh_header;
	GPtrArray *name, *value;
	name = g_ptr_array_new ();
	value = g_ptr_array_new ();
	while (node && node->data) {
		char **tok = g_strsplit (node->data, "=", 2);
		g_ptr_array_add (name, g_strdup(tok[0]));
		g_ptr_array_add (value, g_strdup(tok[1]));
		node = node->next;
		g_strfreev (tok);
	}
	mail_session_set_junk_headers ((const char **)name->pdata, (const char **)value->pdata, name->len);
}


static void
gconf_address_count_changed (GConfClient *client, guint cnxn_id,
			     GConfEntry *entry, gpointer user_data)
{
	config->address_count = gconf_client_get_int (config->gconf, "/apps/evolution/mail/display/address_count", NULL);
}

static void
gconf_error_time_changed (GConfClient *client, guint cnxn_id,
			     GConfEntry *entry, gpointer user_data)
{
	config->error_time = gconf_client_get_int (config->gconf, "/apps/evolution/mail/display/error_timeout", NULL);
}

static void
gconf_error_level_changed (GConfClient *client, guint cnxn_id,
			     GConfEntry *entry, gpointer user_data)
{
	config->error_level = gconf_client_get_int (config->gconf, "/apps/evolution/mail/display/error_level", NULL);
}

static void
gconf_address_compress_changed (GConfClient *client, guint cnxn_id,
			     GConfEntry *entry, gpointer user_data)
{
	config->address_compress = gconf_client_get_bool (config->gconf, "/apps/evolution/mail/display/address_compress", NULL);
}

static void
gconf_mlimit_size_changed (GConfClient *client, guint cnxn_id,
			     GConfEntry *entry, gpointer user_data)
{
	config->mlimit_size = gconf_client_get_int (config->gconf, "/apps/evolution/mail/display/message_text_part_limit", NULL);
}

static void
gconf_mlimit_changed (GConfClient *client, guint cnxn_id,
			     GConfEntry *entry, gpointer user_data)
{
	config->mlimit = gconf_client_get_bool (config->gconf, "/apps/evolution/mail/display/force_message_limit", NULL);
}

static void
gconf_magic_spacebar_changed (GConfClient *client, guint cnxn_id,
			     GConfEntry *entry, gpointer user_data)
{
	config->magic_spacebar = gconf_client_get_bool (config->gconf, "/apps/evolution/mail/display/magic_spacebar", NULL);
}

static void
gconf_mime_types_changed (GConfClient *client, guint cnxn_id,
			  GConfEntry *entry, gpointer user_data)
{
	config_clear_mime_types ();
	config_cache_mime_types ();
}

/* Config struct routines */
void
mail_config_init (void)
{
	if (config)
		return;

	config = g_new0 (MailConfig, 1);
	config->gconf = gconf_client_get_default ();
	config->mime_types = g_ptr_array_new ();
	config->gtkrc = g_build_filename (e_get_user_data_dir (), "mail", "config", "gtkrc-mail-fonts", NULL);

	mail_config_clear ();

	gtk_rc_parse (config->gtkrc);

	gconf_client_add_dir (config->gconf, "/apps/evolution/mail/display",
			      GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
	gconf_client_add_dir (config->gconf, "/apps/evolution/mail/display/fonts",
			      GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
	gconf_client_add_dir (config->gconf, "/GNOME/Spell",
			      GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
	gconf_client_add_dir (config->gconf, "/apps/evolution/mail/junk",
			      GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

	config->font_notify_id = gconf_client_notify_add (config->gconf, "/apps/evolution/mail/display/fonts",
							  gconf_style_changed, NULL, NULL, NULL);
	config->font_notify_id = gconf_client_notify_add (config->gconf, "/apps/evolution/mail/display/address_compress",
							  gconf_address_compress_changed, NULL, NULL, NULL);
	config->font_notify_id = gconf_client_notify_add (config->gconf, "/apps/evolution/mail/display/address_count",
							  gconf_address_count_changed, NULL, NULL, NULL);
	config->error_notify_id = gconf_client_notify_add (config->gconf, "/apps/evolution/mail/display/error_timeout",
							  gconf_error_time_changed, NULL, NULL, NULL);
	config->error_level_id = gconf_client_notify_add (config->gconf, "/apps/evolution/mail/display/error_level",
							  gconf_error_level_changed, NULL, NULL, NULL);

	config->mlimit_notify_id = gconf_client_notify_add (config->gconf, "/apps/evolution/mail/display/force_message_limit",
							  gconf_mlimit_changed, NULL, NULL, NULL);
	config->mlimit_size_notify_id = gconf_client_notify_add (config->gconf, "/apps/evolution/mail/display/message_text_part_limit",
							  gconf_mlimit_size_changed, NULL, NULL, NULL);
	config->magic_spacebar_notify_id = gconf_client_notify_add (config->gconf, "/apps/evolution/mail/display/magic_spacebar",
							  gconf_magic_spacebar_changed, NULL, NULL, NULL);		
	config->spell_notify_id = gconf_client_notify_add (config->gconf, "/GNOME/Spell",
							   gconf_style_changed, NULL, NULL, NULL);
	config->mark_citations__notify_id = gconf_client_notify_add (config->gconf, "/apps/evolution/mail/display/mark_citations",
								     gconf_style_changed, NULL, NULL, NULL);
	config->citation_colour_notify_id = gconf_client_notify_add (config->gconf, "/apps/evolution/mail/display/citation_colour",
								     gconf_style_changed, NULL, NULL, NULL);

	gconf_client_add_dir (config->gconf, E_UTIL_LABELS_GCONF_KEY,
			      GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
	config->label_notify_id =
		gconf_client_notify_add (config->gconf, E_UTIL_LABELS_GCONF_KEY,
					 gconf_labels_changed, NULL, NULL, NULL);

	gconf_client_add_dir (config->gconf, "/apps/evolution/mail/mime_types",
			      GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
	config->mime_types_notify_id =
		gconf_client_notify_add (config->gconf, "/apps/evolution/mail/mime_types",
					 gconf_mime_types_changed, NULL, NULL, NULL);

	config_cache_labels (config->gconf);
	config_cache_mime_types ();
	config->address_compress = gconf_client_get_bool (config->gconf, "/apps/evolution/mail/display/address_compress", NULL);
	config->address_count = gconf_client_get_int (config->gconf, "/apps/evolution/mail/display/address_count", NULL);
	config->error_time = gconf_client_get_int (config->gconf, "/apps/evolution/mail/display/error_timeout", NULL);	
	config->error_level= gconf_client_get_int (config->gconf, "/apps/evolution/mail/display/error_level", NULL);	

	config->mlimit = gconf_client_get_bool (config->gconf, "/apps/evolution/mail/display/force_message_limit", NULL);
	config->mlimit_size = gconf_client_get_int (config->gconf, "/apps/evolution/mail/display/message_text_part_limit", NULL);
	config->magic_spacebar = gconf_client_get_bool (config->gconf, "/apps/evolution/mail/display/magic_spacebar", NULL);
	config->accounts = e_account_list_new (config->gconf);
	config->signatures = e_signature_list_new (config->gconf);

	config->jh_check = gconf_client_get_bool (config->gconf, "/apps/evolution/mail/junk/check_custom_header", NULL);
	config->jh_check_id = gconf_client_notify_add (config->gconf, "/apps/evolution/mail/junk/check_custom_header",
								     gconf_jh_check_changed, NULL, NULL, NULL);
	config->jh_header_id = gconf_client_notify_add (config->gconf, "/apps/evolution/mail/junk/custom_header",
								     gconf_jh_headers_changed, NULL, NULL, NULL);
	config->book_lookup = gconf_client_get_bool (config->gconf, "/apps/evolution/mail/junk/lookup_addressbook", NULL);
	config->book_lookup_id  = gconf_client_notify_add (config->gconf, "/apps/evolution/mail/junk/lookup_addressbook",
								     gconf_lookup_book_changed, NULL, NULL, NULL);

	gconf_jh_check_changed (config->gconf, 0, NULL, config);

}


void
mail_config_clear (void)
{
	if (!config)
		return;

	if (config->accounts) {
		g_object_unref (config->accounts);
		config->accounts = NULL;
	}

	if (config->signatures) {
		g_object_unref (config->signatures);
		config->signatures = NULL;
	}

	config_clear_labels ();
	config_clear_mime_types ();
}


void
mail_config_write (void)
{
	if (!config)
		return;

	e_account_list_save (config->accounts);
	e_signature_list_save (config->signatures);

	gconf_client_suggest_sync (config->gconf, NULL);
}

void
mail_config_write_on_exit (void)
{
	EAccount *account;
	EIterator *iter;

	if (config_write_timeout) {
		g_source_remove (config_write_timeout);
		config_write_timeout = 0;
		mail_config_write ();
	}

	/* Passwords */

	/* then we make sure the ones we want to remember are in the
           session cache */
	iter = e_list_get_iterator ((EList *) config->accounts);
	while (e_iterator_is_valid (iter)) {
		char *passwd;

		account = (EAccount *) e_iterator_get (iter);

		if (account->source->save_passwd && account->source->url && account->source->url[0]) {
			passwd = mail_session_get_password (account->source->url);
			mail_session_forget_password (account->source->url);
			mail_session_add_password (account->source->url, passwd);
			g_free (passwd);
		}

		if (account->transport->save_passwd && account->transport->url && account->transport->url[0]) {
			passwd = mail_session_get_password (account->transport->url);
			mail_session_forget_password (account->transport->url);
			mail_session_add_password (account->transport->url, passwd);
			g_free (passwd);
		}

		e_iterator_next (iter);
	}

	g_object_unref (iter);

	/* then we clear out our component passwords */
	e_passwords_clear_passwords ("Mail");

	/* then we remember them */
	iter = e_list_get_iterator ((EList *) config->accounts);
	while (e_iterator_is_valid (iter)) {
		account = (EAccount *) e_iterator_get (iter);

		if (account->source->save_passwd && account->source->url && account->source->url[0])
			mail_session_remember_password (account->source->url);

		if (account->transport->save_passwd && account->transport->url && account->transport->url[0])
			mail_session_remember_password (account->transport->url);

		e_iterator_next (iter);
	}

	/* now do cleanup */
	mail_config_clear ();

	g_object_unref (config->gconf);
	g_ptr_array_free (config->mime_types, TRUE);

	g_free (config->gtkrc);

	g_free (config);
}

/* Accessor functions */
GConfClient *
mail_config_get_gconf_client (void)
{
	return config->gconf;
}

gboolean
mail_config_is_configured (void)
{
	return e_list_length ((EList *) config->accounts) > 0;
}

gboolean
mail_config_is_corrupt (void)
{
	return config->corrupt;
}

int
mail_config_get_address_count (void)
{
	if (!config->address_compress)
		return -1;

	return config->address_count;
}

guint
mail_config_get_error_timeout  (void)
{	
	if (!config)
		mail_config_init ();

	return config->error_time;
}

guint
mail_config_get_error_level  (void)
{	
	if (!config)
		mail_config_init ();

	return config->error_level;
}

int
mail_config_get_message_limit (void)
{
	if (!config->mlimit)
		return -1;

	return config->mlimit_size;
}

gboolean
mail_config_get_enable_magic_spacebar ()
{
	return config->magic_spacebar;
}

/**
 * mail_config_get_labels
 *
 * @return list of known labels, each member data is EUtilLabel structure.
 *         Returned list should not be freed, neither data inside it.
 **/
GSList *
mail_config_get_labels (void)
{
	return config->labels;
}

const char **
mail_config_get_allowable_mime_types (void)
{
	return (const char **) config->mime_types->pdata;
}

gboolean
mail_config_find_account (EAccount *account)
{
	EAccount *acnt;
	EIterator *iter;

	iter = e_list_get_iterator ((EList *) config->accounts);
	while (e_iterator_is_valid (iter)) {
		acnt = (EAccount *) e_iterator_get (iter);
		if (acnt == account) {
			g_object_unref (iter);
			return TRUE;
		}

		e_iterator_next (iter);
	}

	g_object_unref (iter);

	return FALSE;
}

EAccount *
mail_config_get_default_account (void)
{
	if (config == NULL)
		mail_config_init ();

	if (!config->accounts)
		return NULL;

	/* should probably return const */
	return (EAccount *)e_account_list_get_default(config->accounts);
}

EAccount *
mail_config_get_account_by_name (const char *account_name)
{
	return (EAccount *)e_account_list_find(config->accounts, E_ACCOUNT_FIND_NAME, account_name);
}

EAccount *
mail_config_get_account_by_uid (const char *uid)
{
	return (EAccount *) e_account_list_find (config->accounts, E_ACCOUNT_FIND_UID, uid);
}

EAccount *
mail_config_get_account_by_source_url (const char *source_url)
{
	CamelProvider *provider;
	EAccount *account;
	CamelURL *source;
	EIterator *iter;

	g_return_val_if_fail (source_url != NULL, NULL);

	provider = camel_provider_get(source_url, NULL);
	if (!provider)
		return NULL;

	source = camel_url_new (source_url, NULL);
	if (!source)
		return NULL;

	iter = e_list_get_iterator ((EList *) config->accounts);
	while (e_iterator_is_valid (iter)) {
		account = (EAccount *) e_iterator_get (iter);

		if (account->source && account->source->url && account->source->url[0]) {
			CamelURL *url;

			url = camel_url_new (account->source->url, NULL);
			if (url && provider->url_equal (url, source)) {
				camel_url_free (url);
				camel_url_free (source);
				g_object_unref (iter);

				return account;
			}

			if (url)
				camel_url_free (url);
		}

		e_iterator_next (iter);
	}

	g_object_unref (iter);

	camel_url_free (source);

	return NULL;
}

EAccount *
mail_config_get_account_by_transport_url (const char *transport_url)
{
	CamelProvider *provider;
	CamelURL *transport;
	EAccount *account;
	EIterator *iter;

	g_return_val_if_fail (transport_url != NULL, NULL);

	provider = camel_provider_get(transport_url, NULL);
	if (!provider)
		return NULL;

	transport = camel_url_new (transport_url, NULL);
	if (!transport)
		return NULL;

	iter = e_list_get_iterator ((EList *) config->accounts);
	while (e_iterator_is_valid (iter)) {
		account = (EAccount *) e_iterator_get (iter);

		if (account->transport && account->transport->url && account->transport->url[0]) {
			CamelURL *url;

			url = camel_url_new (account->transport->url, NULL);
			if (url && provider->url_equal (url, transport)) {
				camel_url_free (url);
				camel_url_free (transport);
				g_object_unref (iter);

				return account;
			}

			if (url)
				camel_url_free (url);
		}

		e_iterator_next (iter);
	}

	g_object_unref (iter);

	camel_url_free (transport);

	return NULL;
}

int
mail_config_has_proxies (EAccount *account)
{
	return e_account_list_account_has_proxies (config->accounts, account);
}

void
mail_config_remove_account_proxies (EAccount *account)
{
	e_account_list_remove_account_proxies (config->accounts, account);
}

void
mail_config_prune_proxies (void)
{
	e_account_list_prune_proxies (config->accounts);
}

EAccountList *
mail_config_get_accounts (void)
{
	if (config == NULL)
		mail_config_init ();

	return config->accounts;
}

void
mail_config_add_account (EAccount *account)
{
	e_account_list_add(config->accounts, account);
	mail_config_save_accounts ();
}

void
mail_config_remove_account (EAccount *account)
{
	e_account_list_remove(config->accounts, account);
	mail_config_save_accounts ();
}

void
mail_config_set_default_account (EAccount *account)
{
	e_account_list_set_default(config->accounts, account);
}

EAccountIdentity *
mail_config_get_default_identity (void)
{
	EAccount *account;

	account = mail_config_get_default_account ();
	if (account)
		return account->id;
	else
		return NULL;
}

EAccountService *
mail_config_get_default_transport (void)
{
	EAccount *account;
	EIterator *iter;

	account = mail_config_get_default_account ();
	if (account && account->enabled && account->transport && account->transport->url && account->transport->url[0])
		return account->transport;

	/* return the first account with a transport? */
	iter = e_list_get_iterator ((EList *) config->accounts);
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

static char *
uri_to_evname (const char *uri, const char *prefix)
{
	const char *base_directory = mail_component_peek_base_directory (mail_component_peek ());
	char *safe;
	char *tmp;

	safe = g_strdup (uri);
	e_filename_make_safe (safe);
	/* blah, easiest thing to do */
	if (prefix[0] == '*')
		tmp = g_strdup_printf ("%s/%s%s.xml", base_directory, prefix + 1, safe);
	else
		tmp = g_strdup_printf ("%s/%s%s", base_directory, prefix, safe);
	g_free (safe);
	return tmp;
}

void
mail_config_uri_renamed (GCompareFunc uri_cmp, const char *old, const char *new)
{
	EAccount *account;
	EIterator *iter;
	int i, work = 0;
	char *oldname, *newname;
	char *cachenames[] = { "config/hidestate-",
			       "config/et-expanded-",
			       "config/et-header-",
			       "*views/current_view-",
			       "*views/custom_view-",
			       NULL };

	iter = e_list_get_iterator ((EList *) config->accounts);
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

void
mail_config_uri_deleted (GCompareFunc uri_cmp, const char *uri)
{
	EAccount *account;
	EIterator *iter;
	int work = 0;
	/* assumes these can't be removed ... */
	const char *default_sent_folder_uri = mail_component_get_folder_uri(NULL, MAIL_COMPONENT_FOLDER_SENT);
	const char *default_drafts_folder_uri = mail_component_get_folder_uri(NULL, MAIL_COMPONENT_FOLDER_DRAFTS);

	iter = e_list_get_iterator ((EList *) config->accounts);
	while (e_iterator_is_valid (iter)) {
		account = (EAccount *) e_iterator_get (iter);

		if (account->sent_folder_uri && uri_cmp (account->sent_folder_uri, uri)) {
			g_free (account->sent_folder_uri);
			account->sent_folder_uri = g_strdup (default_sent_folder_uri);
			work = 1;
		}

		if (account->drafts_folder_uri && uri_cmp (account->drafts_folder_uri, uri)) {
			g_free (account->drafts_folder_uri);
			account->drafts_folder_uri = g_strdup (default_drafts_folder_uri);
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

char *
mail_config_folder_to_safe_url (CamelFolder *folder)
{
	char *url;

	url = mail_tools_folder_to_url (folder);
	e_filename_make_safe (url);

	return url;
}

char *
mail_config_folder_to_cachename (CamelFolder *folder, const char *prefix)
{
	char *url, *basename, *filename;
	const char *evolution_dir;

	evolution_dir = mail_component_peek_base_directory (mail_component_peek ());

	url = mail_config_folder_to_safe_url (folder);
	basename = g_strdup_printf ("%s%s", prefix, url);
	filename = g_build_filename (evolution_dir, "config", basename, NULL);
	g_free (basename);
	g_free (url);

	return filename;
}

ESignatureList *
mail_config_get_signatures (void)
{
	return config->signatures;
}

static char *
get_new_signature_filename (void)
{
	const char *base_directory;
	char *filename, *id;
	struct stat st;
	int i;

	base_directory = e_get_user_data_dir ();
	filename = g_build_filename (base_directory, "signatures", NULL);
	if (g_lstat (filename, &st)) {
		if (errno == ENOENT) {
			if (g_mkdir (filename, 0700))
				g_warning ("Fatal problem creating %s directory.", filename);
		} else
			g_warning ("Fatal problem with %s directory.", filename);
	}
	g_free (filename);

	filename = g_malloc (strlen (base_directory) + sizeof ("/signatures/signature-") + 12);
	id = g_stpcpy (filename, base_directory);
	id = g_stpcpy (id, "/signatures/signature-");

	for (i = 0; i < (INT_MAX - 1); i++) {
		sprintf (id, "%d", i);
		if (g_lstat (filename, &st) == -1 && errno == ENOENT) {
			int fd;

			fd = g_creat (filename, 0600);
			if (fd >= 0) {
				close (fd);
				return filename;
			}
		}
	}

	g_free (filename);

	return NULL;
}


ESignature *
mail_config_signature_new (const char *filename, gboolean script, gboolean html)
{
	ESignature *sig;

	sig = e_signature_new ();
	sig->name = g_strdup (_("Unnamed"));
	sig->script = script;
	sig->html = html;

	if (filename == NULL)
		sig->filename = get_new_signature_filename ();
	else
		sig->filename = g_strdup (filename);

	return sig;
}

ESignature *
mail_config_get_signature_by_uid (const char *uid)
{
	return (ESignature *) e_signature_list_find (config->signatures, E_SIGNATURE_FIND_UID, uid);
}

ESignature *
mail_config_get_signature_by_name (const char *name)
{
	return (ESignature *) e_signature_list_find (config->signatures, E_SIGNATURE_FIND_NAME, name);
}

void
mail_config_add_signature (ESignature *signature)
{
	e_signature_list_add (config->signatures, signature);
	mail_config_save_signatures ();
}

void
mail_config_remove_signature (ESignature *signature)
{
	if (signature->filename && !signature->script)
		g_unlink (signature->filename);

	e_signature_list_remove (config->signatures, signature);
	mail_config_save_signatures ();
}

void
mail_config_reload_junk_headers ()
{
	/* It automatically sets in the session */
	if (config == NULL)
		mail_config_init ();
	else 
		gconf_jh_check_changed (config->gconf, 0, NULL, config);

}

gboolean
mail_config_get_lookup_book()
{
	/* It automatically sets in the session */
	if (config == NULL)
		mail_config_init ();

	return config->book_lookup;

}

char *
mail_config_signature_run_script (const char *script)
{
#ifndef G_OS_WIN32
	int result, status;
	int in_fds[2];
	pid_t pid;

	if (pipe (in_fds) == -1) {
		g_warning ("Failed to create pipe to '%s': %s", script, g_strerror (errno));
		return NULL;
	}

	if (!(pid = fork ())) {
		/* child process */
		int maxfd, i;

		close (in_fds [0]);
		if (dup2 (in_fds[1], STDOUT_FILENO) < 0)
			_exit (255);
		close (in_fds [1]);

		setsid ();

		maxfd = sysconf (_SC_OPEN_MAX);
		for (i = 3; i < maxfd; i++) {
			if (i != STDIN_FILENO && i != STDOUT_FILENO && i != STDERR_FILENO)
				fcntl (i, F_SETFD, FD_CLOEXEC);
		}

		execlp("/bin/sh", "/bin/sh", "-c", script, NULL);
		g_warning ("Could not execute %s: %s\n", script, g_strerror (errno));
		_exit (255);
	} else if (pid < 0) {
		g_warning ("Failed to create create child process '%s': %s", script, g_strerror (errno));
		close (in_fds [0]);
		close (in_fds [1]);
		return NULL;
	} else {
		CamelStreamFilter *filtered_stream;
		CamelStreamMem *memstream;
		CamelMimeFilter *charenc;
		CamelStream *stream;
		GByteArray *buffer;
		char *charset;
		char *content;

		/* parent process */
		close (in_fds[1]);

		stream = camel_stream_fs_new_with_fd (in_fds[0]);

		memstream = (CamelStreamMem *) camel_stream_mem_new ();
		buffer = g_byte_array_new ();
		camel_stream_mem_set_byte_array (memstream, buffer);

		camel_stream_write_to_stream (stream, (CamelStream *) memstream);
		camel_object_unref (stream);

		/* signature scripts are supposed to generate UTF-8 content, but because users
		   are known to not ever read the manual... we try to do our best if the
                   content isn't valid UTF-8 by assuming that the content is in the user's
		   preferred charset. */
		if (!g_utf8_validate ((char *)buffer->data, buffer->len, NULL)) {
			stream = (CamelStream *) memstream;
			memstream = (CamelStreamMem *) camel_stream_mem_new ();
			camel_stream_mem_set_byte_array (memstream, g_byte_array_new ());

			filtered_stream = camel_stream_filter_new_with_stream (stream);
			camel_object_unref (stream);

			charset = gconf_client_get_string (config->gconf, "/apps/evolution/mail/composer/charset", NULL);
			if (charset && *charset) {
				if ((charenc = (CamelMimeFilter *) camel_mime_filter_charset_new_convert (charset, "utf-8"))) {
					camel_stream_filter_add (filtered_stream, charenc);
					camel_object_unref (charenc);
				}
			}
			g_free (charset);

			camel_stream_write_to_stream ((CamelStream *) filtered_stream, (CamelStream *) memstream);
			camel_object_unref (filtered_stream);
			g_byte_array_free (buffer, TRUE);

			buffer = memstream->buffer;
		}

		camel_object_unref (memstream);

		g_byte_array_append (buffer, (const unsigned char *)"", 1);
		content = (char *)buffer->data;
		g_byte_array_free (buffer, FALSE);

		/* wait for the script process to terminate */
		result = waitpid (pid, &status, 0);

		if (result == -1 && errno == EINTR) {
			/* child process is hanging... */
			kill (pid, SIGTERM);
			sleep (1);
			result = waitpid (pid, &status, WNOHANG);
			if (result == 0) {
				/* ...still hanging, set phasers to KILL */
				kill (pid, SIGKILL);
				sleep (1);
				result = waitpid (pid, &status, WNOHANG);
			}
		}

		return content;
	}
#else
	return NULL;
#endif
}
