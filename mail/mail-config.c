/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <pwd.h>
#include <ctype.h>

#include <glib.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-config.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-stock.h>
#include <gtkhtml/gtkhtml.h>
#include <glade/glade.h>

#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-context.h>
#include <bonobo/bonobo-moniker-util.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo-conf/bonobo-config-database.h>

#include <gal/util/e-util.h>
#include <gal/unicode/gunicode.h>
#include <e-util/e-html-utils.h>
#include <e-util/e-url.h>
#include "mail.h"
#include "mail-config.h"
#include "mail-ops.h"
#include "mail-mt.h"

#include "Mail.h"

typedef struct {
	Bonobo_ConfigDatabase db;

	gboolean show_preview;
	gboolean thread_list;
	gboolean hide_deleted;
	gint paned_size;
	gboolean send_html;
	gboolean citation_highlight;
	guint32  citation_color;
	gboolean prompt_empty_subject;
	gboolean prompt_only_bcc;
	gboolean do_seen_timeout;
	gint seen_timeout;
	gboolean empty_trash_on_exit;
	
	GSList *accounts;
	gint default_account;

	GSList *news;
	
	char *pgp_path;
	CamelPgpType pgp_type;
	gboolean remember_pgp_passphrase;

	MailConfigHTTPMode http_mode;
	MailConfigForwardStyle default_forward_style;
	MailConfigDisplayStyle message_display_style;
	char *default_charset;
	
	GHashTable *threaded_hash;
	GHashTable *preview_hash;
} MailConfig;

static MailConfig *config = NULL;

#define MAIL_CONFIG_IID "OAFIID:GNOME_Evolution_MailConfig_Factory"

/* Prototypes */
static void config_read (void);
static void mail_config_set_default_account_num (gint new_default);


/* Identity */
MailConfigIdentity *
identity_copy (const MailConfigIdentity *id)
{
	MailConfigIdentity *new;
	
	g_return_val_if_fail (id != NULL, NULL);
	
	new = g_new0 (MailConfigIdentity, 1);
	new->name = g_strdup (id->name);
	new->address = g_strdup (id->address);
	new->organization = g_strdup (id->organization);
	new->signature = g_strdup (id->signature);
	new->html_signature = g_strdup (id->html_signature);
	new->has_html_signature = id->has_html_signature;

	return new;
}

void
identity_destroy (MailConfigIdentity *id)
{
	if (!id)
		return;
	
	g_free (id->name);
	g_free (id->address);
	g_free (id->organization);
	g_free (id->signature);
	g_free (id->html_signature);
	
	g_free (id);
}

/* Service */
MailConfigService *
service_copy (const MailConfigService *source) 
{
	MailConfigService *new;
	
	g_return_val_if_fail (source != NULL, NULL);
	
	new = g_new0 (MailConfigService, 1);
	new->url = g_strdup (source->url);
	new->keep_on_server = source->keep_on_server;
	new->auto_check = source->auto_check;
	new->auto_check_time = source->auto_check_time;
	new->enabled = source->enabled;
	new->save_passwd = source->save_passwd;
	
	return new;
}

void
service_destroy (MailConfigService *source)
{
	if (!source)
		return;
	
	g_free (source->url);
	
	g_free (source);
}

void
service_destroy_each (gpointer item, gpointer data)
{
	service_destroy ((MailConfigService *)item);
}

/* Account */
MailConfigAccount *
account_copy (const MailConfigAccount *account) 
{
	MailConfigAccount *new;
	
	g_return_val_if_fail (account != NULL, NULL);
	
	new = g_new0 (MailConfigAccount, 1);
	new->name = g_strdup (account->name);
	
	new->id = identity_copy (account->id);
	new->source = service_copy (account->source);
	new->transport = service_copy (account->transport);
	
	new->drafts_folder_name = g_strdup (account->drafts_folder_name);
	new->drafts_folder_uri = g_strdup (account->drafts_folder_uri);
	new->sent_folder_name = g_strdup (account->sent_folder_name);
	new->sent_folder_uri = g_strdup (account->sent_folder_uri);
	
	new->pgp_key = g_strdup (account->pgp_key);
	new->pgp_encrypt_to_self = account->pgp_encrypt_to_self;
	
	new->smime_key = g_strdup (account->smime_key);
	new->smime_encrypt_to_self = account->smime_encrypt_to_self;
	
	return new;
}

void
account_destroy (MailConfigAccount *account)
{
	if (!account)
		return;
	
	g_free (account->name);
	
	identity_destroy (account->id);
	service_destroy (account->source);
	service_destroy (account->transport);
	
	g_free (account->drafts_folder_name);
	g_free (account->drafts_folder_uri);
	g_free (account->sent_folder_name);
	g_free (account->sent_folder_uri);
	
	g_free (account->pgp_key);
	g_free (account->smime_key);
	
	g_free (account);
}

void
account_destroy_each (gpointer item, gpointer data)
{
	account_destroy ((MailConfigAccount *)item);
}

/* Config struct routines */
void
mail_config_init (void)
{
	Bonobo_ConfigDatabase db;
	CORBA_Environment ev;

	if (config)
		return;
	
	CORBA_exception_init (&ev);

	db = bonobo_get_object ("wombat:", "Bonobo/ConfigDatabase", &ev);
	
	if (BONOBO_EX (&ev) || db == CORBA_OBJECT_NIL) {
	       
		CORBA_exception_free (&ev);
		return;
 	}

	CORBA_exception_free (&ev);

	config = g_new0 (MailConfig, 1);

	config->db = db;
	
	config_read ();
}

void
mail_config_clear (void)
{
	if (!config)
		return;
	
	if (config->accounts) {
		g_slist_foreach (config->accounts, account_destroy_each, NULL);
		g_slist_free (config->accounts);
		config->accounts = NULL;
	}
	
	if (config->news) {
		g_slist_foreach (config->news, service_destroy_each, NULL);
		g_slist_free (config->news);
		config->news = NULL;
	}
}

static void
config_read (void)
{
	gint len, i, default_num;

	mail_config_clear ();
		
	len = bonobo_config_get_long_with_default (config->db, 
	        "/Mail/Accounts/num", 0, NULL);

	for (i = 0; i < len; i++) {
		MailConfigAccount *account;
		MailConfigIdentity *id;
		MailConfigService *source;
		MailConfigService *transport;
		gchar *path, *val;
		
		account = g_new0 (MailConfigAccount, 1);
		path = g_strdup_printf ("/Mail/Accounts/account_name_%d", i);
		account->name = bonobo_config_get_string (config->db, path, NULL);
		g_free (path);
		
		path = g_strdup_printf ("/Mail/Accounts/account_drafts_folder_name_%d", i);
		val = bonobo_config_get_string (config->db, path, NULL);
		g_free (path);
		if (val && *val)
			account->drafts_folder_name = val;
		else
			g_free (val);
		
		path = g_strdup_printf ("/Mail/Accounts/account_drafts_folder_uri_%d", i);
		val = bonobo_config_get_string (config->db, path, NULL);
		g_free (path);
		if (val && *val)
			account->drafts_folder_uri = val;
		else
			g_free (val);
		
		path = g_strdup_printf ("/Mail/Accounts/account_sent_folder_name_%d", i);
		val = bonobo_config_get_string (config->db, path, NULL);
		g_free (path);
		if (val && *val)
			account->sent_folder_name = val;
		else
			g_free (val);
		
		path = g_strdup_printf ("/Mail/Accounts/account_sent_folder_uri_%d", i);
		val = bonobo_config_get_string (config->db, path, NULL);
		g_free (path);
		if (val && *val)
			account->sent_folder_uri = val;
		else
			g_free (val);
		
		/* get the pgp info */
		path = g_strdup_printf ("/Mail/Accounts/account_pgp_key_%d", i);
		val = bonobo_config_get_string (config->db, path, NULL);
		g_free (path);
		if (val && *val)
			account->pgp_key = val;
		else
			g_free (val);
		
		path = g_strdup_printf ("/Mail/Accounts/account_pgp_encrypt_to_self_%d", i);
		account->pgp_encrypt_to_self = bonobo_config_get_boolean_with_default (
		        config->db, path, TRUE, NULL);
		g_free (path);
		
		/* get the s/mime info */
		path = g_strdup_printf ("/Mail/Accounts/account_smime_key_%d", i);
		val = bonobo_config_get_string (config->db, path, NULL);
		g_free (path);
		if (val && *val)
			account->smime_key = val;
		else
			g_free (val);
		
		path = g_strdup_printf ("/Mail/Accounts/account_smime_encrypt_to_self_%d", i);
		account->smime_encrypt_to_self = bonobo_config_get_boolean_with_default (
		        config->db, path, TRUE, NULL);
		g_free (path);
		
		/* get the identity info */
		id = g_new0 (MailConfigIdentity, 1);		
		path = g_strdup_printf ("/Mail/Accounts/identity_name_%d", i);
		id->name = bonobo_config_get_string (config->db, path, NULL);
		g_free (path);

		path = g_strdup_printf ("/Mail/Accounts/identity_address_%d", i);
		id->address = bonobo_config_get_string (config->db, path, NULL);
		g_free (path);

		path = g_strdup_printf ("/Mail/Accounts/identity_organization_%d", i);
		id->organization = bonobo_config_get_string (config->db, path, NULL);
		g_free (path);

		path = g_strdup_printf ("/Mail/Accounts/identity_signature_%d", i);
		id->signature = bonobo_config_get_string (config->db, path, NULL);
		g_free (path);
		path = g_strdup_printf ("/Mail/Accounts/identity_html_signature_%d", i);
		id->html_signature = bonobo_config_get_string (config->db, path, NULL);
		g_free (path);
		path = g_strdup_printf ("/Mail/Accounts/identity_has_html_signature_%d", i);
		id->has_html_signature = bonobo_config_get_boolean_with_default (
			config->db, path, FALSE, NULL);
		g_free (path);

		/* get the source */
		source = g_new0 (MailConfigService, 1);

		path = g_strdup_printf ("/Mail/Accounts/source_url_%d", i);
		val = bonobo_config_get_string (config->db, path, NULL);
		g_free (path);
		if (val && *val)
			source->url = val;
		else
			g_free (val);
		
		path = g_strdup_printf ("/Mail/Accounts/source_keep_on_server_%d", i);
		source->keep_on_server = bonobo_config_get_boolean (config->db, path, NULL);
		g_free (path);

		path = g_strdup_printf ("/Mail/Accounts/source_auto_check_%d", i);
		source->auto_check = bonobo_config_get_boolean_with_default (
                        config->db, path, FALSE, NULL);
		g_free (path);

		path = g_strdup_printf ("/Mail/Accounts/source_auto_check_time_%d", i);
		source->auto_check_time = bonobo_config_get_long_with_default ( 
			config->db, path, -1, NULL);

		if (source->auto_check && source->auto_check_time <= 0) {
			source->auto_check_time = 5;
			source->auto_check = FALSE;
		}

		g_free (path);

		path = g_strdup_printf ("/Mail/Accounts/source_enabled_%d", i);
		source->enabled = bonobo_config_get_boolean_with_default (
			config->db, path, TRUE, NULL);
		g_free (path);

		path = g_strdup_printf 
			("/Mail/Accounts/source_save_passwd_%d", i);
		source->save_passwd = bonobo_config_get_boolean_with_default ( 
			config->db, path, TRUE, NULL);
		g_free (path);
		
		/* get the transport */
		transport = g_new0 (MailConfigService, 1);
		path = g_strdup_printf ("/Mail/Accounts/transport_url_%d", i);
		val = bonobo_config_get_string (config->db, path, NULL);
		g_free (path);
		if (val && *val)
			transport->url = val;
		else
			g_free (val);
		
		path = g_strdup_printf ("/Mail/Accounts/transport_save_passwd_%d", i);
		transport->save_passwd = bonobo_config_get_boolean (config->db, path, NULL);
		g_free (path);
		
		account->id = id;
		account->source = source;
		account->transport = transport;
		
		config->accounts = g_slist_append (config->accounts, account);
	}


	default_num = bonobo_config_get_long_with_default (config->db,
		"/Mail/Accounts/default_account", 0, NULL);

	mail_config_set_default_account_num (default_num);

#ifdef ENABLE_NNTP
	/* News */
	
	len = bonobo_config_get_long_with_default (config->db, 
	        "/News/Sources/num", 0, NULL);
	for (i = 0; i < len; i++) {
		MailConfigService *n;
		gchar *path, *r;
		
		path = g_strdup_printf ("/News/Sources/url_%d", i);

		if ((r = bonobo_config_get_string (config->db, path, NULL))) {

			n = g_new0 (MailConfigService, 1);		
			n->url = r;
			config->news = g_slist_append (config->news, n);
		} 

		g_free (path);
		
	}
#endif
	
	/* Format */
	config->send_html = bonobo_config_get_boolean_with_default (config->db,
	        "/Mail/Format/send_html", FALSE, NULL);

	/* Citation */
	config->citation_highlight = bonobo_config_get_boolean_with_default (
		config->db, "/Mail/Display/citation_highlight", TRUE, NULL);
	
	config->citation_color = bonobo_config_get_long_with_default (
		config->db, "/Mail/Display/citation_color", 0x737373, NULL);
	
	/* Mark as seen toggle */
	config->do_seen_timeout = bonobo_config_get_boolean_with_default (
		config->db, "/Mail/Display/do_seen_timeout", TRUE, NULL);

	/* Mark as seen timeout */
	config->seen_timeout = bonobo_config_get_long_with_default (config->db,
                "/Mail/Display/seen_timeout", 1500, NULL);
	
	/* Show Messages Threaded */
	config->thread_list = bonobo_config_get_boolean_with_default (
                config->db, "/Mail/Display/thread_list", FALSE, NULL);
	
	config->show_preview = bonobo_config_get_boolean_with_default (
		config->db, "/Mail/Display/preview_pane", TRUE, NULL);
	
	/* Hide deleted automatically */
	config->hide_deleted = bonobo_config_get_boolean_with_default (
		config->db, "/Mail/Display/hide_deleted", FALSE, NULL);
	
	/* Size of vpaned in mail view */
	config->paned_size = bonobo_config_get_long_with_default (config->db, 
                "/Mail/Display/paned_size", 200, NULL);

	/* Empty Subject */
	config->prompt_empty_subject = bonobo_config_get_boolean_with_default (
		config->db, "/Mail/Prompts/empty_subject", TRUE, NULL);
	
	/* Only Bcc */
	config->prompt_only_bcc = bonobo_config_get_boolean_with_default (
		config->db, "/Mail/Prompts/only_bcc", TRUE, NULL);

	/* PGP/GPG */
	config->pgp_path = bonobo_config_get_string (config->db, 
						     "/Mail/PGP/path", NULL);
	
	config->pgp_type = bonobo_config_get_long_with_default (config->db, 
	        "/Mail/PGP/type", CAMEL_PGP_TYPE_NONE, NULL);
	
	config->remember_pgp_passphrase = bonobo_config_get_boolean_with_default (
		config->db, "/Mail/Prompts/remember_passphrase", TRUE, NULL);
	
	/* HTTP images */
	config->http_mode = bonobo_config_get_long_with_default (config->db, 
		"/Mail/Display/http_images", MAIL_CONFIG_HTTP_SOMETIMES, NULL);
	
	/* Forwarding */
	config->default_forward_style = bonobo_config_get_long_with_default (
		config->db, "/Mail/Format/default_forward_style", 
		MAIL_CONFIG_FORWARD_ATTACHED, NULL);
	
	/* Message Display */
	config->message_display_style = bonobo_config_get_long_with_default (
		config->db, "/Mail/Format/message_display_style", 
		MAIL_CONFIG_DISPLAY_NORMAL, NULL);
	
	/* Default charset */
	config->default_charset = bonobo_config_get_string (config->db, 
	        "/Mail/Format/default_charset", NULL);
	
	if (!config->default_charset) {
		g_get_charset (&config->default_charset);
		if (!config->default_charset ||
		    !g_strcasecmp (config->default_charset, "US-ASCII"))
			config->default_charset = g_strdup ("ISO-8859-1");
		else
			config->default_charset = g_strdup (config->default_charset);
	}
	
	/* Trash folders */
	config->empty_trash_on_exit = bonobo_config_get_boolean_with_default (
		config->db, "/Mail/Trash/empty_on_exit", FALSE, NULL);
}

#define bonobo_config_set_string_wrapper(db, path, val, ev) bonobo_config_set_string (db, path, val ? val : "", ev)

void
mail_config_write (void)
{
	CORBA_Environment ev;
	gint len, i, default_num;
	
	/* Accounts */
	
	if (!config)
		return;
	
	CORBA_exception_init (&ev);
	Bonobo_ConfigDatabase_removeDir (config->db, "/Mail/Accounts", &ev);
	CORBA_exception_init (&ev);
	Bonobo_ConfigDatabase_removeDir (config->db, "/News/Sources", &ev);
	CORBA_exception_init (&ev);
	Bonobo_ConfigDatabase_sync (config->db, &ev);
	
	len = g_slist_length (config->accounts);
	bonobo_config_set_long (config->db,
				"/Mail/Accounts/num", len, NULL);
	
	default_num = mail_config_get_default_account_num ();
	bonobo_config_set_long (config->db,
				"/Mail/Accounts/default_account", default_num, NULL);
	
	for (i = 0; i < len; i++) {
		MailConfigAccount *account;
		gchar *path;
		
		account = g_slist_nth_data (config->accounts, i);
		
		/* account info */
		path = g_strdup_printf ("/Mail/Accounts/account_name_%d", i);
		bonobo_config_set_string_wrapper (config->db, path, account->name, NULL);
		g_free (path);
		
		path = g_strdup_printf ("/Mail/Accounts/account_drafts_folder_name_%d", i);
		bonobo_config_set_string_wrapper (config->db, path, 
						  account->drafts_folder_name, NULL);
		g_free (path);
		
		path = g_strdup_printf ("/Mail/Accounts/account_drafts_folder_uri_%d", i);
		bonobo_config_set_string_wrapper (config->db, path, 
						  account->drafts_folder_uri, NULL);
		g_free (path);
		
		path = g_strdup_printf ("/Mail/Accounts/account_sent_folder_name_%d", i);
		bonobo_config_set_string_wrapper (config->db, path, 
						  account->sent_folder_name, NULL);
		g_free (path);
		
		path = g_strdup_printf ("/Mail/Accounts/account_sent_folder_uri_%d", i);
		bonobo_config_set_string_wrapper (config->db, path, 
						  account->sent_folder_uri, NULL);
		g_free (path);
		
		/* account pgp options */
		path = g_strdup_printf ("/Mail/Accounts/account_pgp_key_%d", i);
		bonobo_config_set_string_wrapper (config->db, path, account->pgp_key, NULL);
		g_free (path);
		
		path = g_strdup_printf ("/Mail/Accounts/account_pgp_encrypt_to_self_%d", i);
		bonobo_config_set_boolean (config->db, path, 
					   account->pgp_encrypt_to_self, NULL);
		g_free (path);
		
		/* account s/mime options */
		path = g_strdup_printf ("/Mail/Accounts/account_smime_key_%d", i);
		bonobo_config_set_string_wrapper (config->db, path, account->smime_key, NULL);
		g_free (path);
		
		path = g_strdup_printf ("/Mail/Accounts/account_smime_encrypt_to_self_%d", i);
		bonobo_config_set_boolean (config->db, path, account->smime_encrypt_to_self, NULL);
		g_free (path);
		
		/* identity info */
		path = g_strdup_printf ("/Mail/Accounts/identity_name_%d", i);
		bonobo_config_set_string_wrapper (config->db, path, account->id->name, NULL);
		g_free (path);
		
		path = g_strdup_printf ("/Mail/Accounts/identity_address_%d", i);
		bonobo_config_set_string_wrapper (config->db, path, account->id->address, NULL);
		g_free (path);
		
		path = g_strdup_printf ("/Mail/Accounts/identity_organization_%d", i);
		bonobo_config_set_string_wrapper (config->db, path, account->id->organization, NULL);
		g_free (path);
		
		path = g_strdup_printf ("/Mail/Accounts/identity_signature_%d", i);
		bonobo_config_set_string_wrapper (config->db, path, account->id->signature, NULL);
		g_free (path);
		
		path = g_strdup_printf ("/Mail/Accounts/identity_html_signature_%d", i);
		bonobo_config_set_string_wrapper (config->db, path, account->id->html_signature, NULL);
		g_free (path);
		
		path = g_strdup_printf ("/Mail/Accounts/identity_has_html_signature_%d", i);
		bonobo_config_set_boolean (config->db, path, account->id->has_html_signature, NULL);
		g_free (path);
		
		/* source info */
		path = g_strdup_printf ("/Mail/Accounts/source_url_%d", i);
		bonobo_config_set_string_wrapper (config->db, path, account->source->url, NULL);
		g_free (path);
		
		path = g_strdup_printf ("/Mail/Accounts/source_keep_on_server_%d", i);
		bonobo_config_set_boolean (config->db, path, account->source->keep_on_server, NULL);
		g_free (path);
		
		path = g_strdup_printf ("/Mail/Accounts/source_auto_check_%d", i);
		bonobo_config_set_boolean (config->db, path, account->source->auto_check, NULL);
		g_free (path);
		
		path = g_strdup_printf ("/Mail/Accounts/source_auto_check_time_%d", i);
		bonobo_config_set_long (config->db, path, account->source->auto_check_time, NULL);
		g_free (path);
		
		path = g_strdup_printf ("/Mail/Accounts/source_enabled_%d", i);
		bonobo_config_set_boolean (config->db, path, account->source->enabled, NULL);
		g_free (path);
		
		path = g_strdup_printf ("/Mail/Accounts/source_save_passwd_%d", i);
		bonobo_config_set_boolean (config->db, path, account->source->save_passwd, NULL);
		g_free (path);
		
		/* transport info */
		path = g_strdup_printf ("/Mail/Accounts/transport_url_%d", i);
		bonobo_config_set_string_wrapper (config->db, path, account->transport->url, NULL);
		g_free (path);
		
		path = g_strdup_printf ("/Mail/Accounts/transport_save_passwd_%d", i);
		bonobo_config_set_boolean (config->db, path, account->transport->save_passwd, NULL);
		g_free (path);
	}
	
#ifdef ENABLE_NNTP
	/* News */
	
  	len = g_slist_length (config->news);
	bonobo_config_set_long (config->db, "/News/Sources/num", len, NULL);
	for (i = 0; i < len; i++) {
		MailConfigService *n;
		gchar *path;
		
		n = g_slist_nth_data (config->news, i);
		
		path = g_strdup_printf ("/News/Sources/url_%d", i);
		bonobo_config_set_string_wrapper (config->db, path, n->url, NULL);
		g_free (path);
	}
	
#endif
	
	CORBA_exception_init (&ev);
	Bonobo_ConfigDatabase_sync (config->db, &ev);
	CORBA_exception_free (&ev);
}

static gboolean
hash_save_state (gpointer key, gpointer value, gpointer user_data)
{
	char *path;
	gboolean bool = GPOINTER_TO_INT (value);
	
	path = g_strconcat ("/Mail/", (char *)user_data, "/", (char *)key, 
			    NULL);
	bonobo_config_set_boolean (config->db, path, bool, NULL);
	g_free (path);
	g_free (key);
	
	return TRUE;
}

void
mail_config_write_on_exit (void)
{
	CORBA_Environment ev;
	GSList *sources;
	MailConfigService *s;
	
	/* Show Messages Threaded */
	bonobo_config_set_boolean (config->db, "/Mail/Display/thread_list", 
				   config->thread_list, NULL);
	
	/* Show Message Preview */
	bonobo_config_set_boolean (config->db, "/Mail/Display/preview_pane",
				   config->show_preview, NULL);
	
	/* Hide deleted automatically */
	bonobo_config_set_boolean (config->db, "/Mail/Display/hide_deleted", 
				   config->hide_deleted, NULL);
	
	/* Size of vpaned in mail view */
	bonobo_config_set_long (config->db, "/Mail/Display/paned_size", 
				config->paned_size, NULL);

	/* Mark as seen toggle */
	bonobo_config_set_boolean (config->db, "/Mail/Display/do_seen_timeout",
				   config->do_seen_timeout, NULL);
	/* Mark as seen timeout */
	bonobo_config_set_long (config->db, "/Mail/Display/seen_timeout", 
				config->seen_timeout, NULL);
	
	/* Format */
	bonobo_config_set_boolean (config->db, "/Mail/Format/send_html", 
				   config->send_html, NULL);
	
	/* Citation */
	bonobo_config_set_boolean (config->db, 
				   "/Mail/Display/citation_highlight", 
				   config->citation_highlight, NULL);
	
	bonobo_config_set_long (config->db, "/Mail/Display/citation_color",
				config->citation_color, NULL);
	
	/* Empty Subject */
	bonobo_config_set_boolean (config->db, "/Mail/Prompts/empty_subject",
				   config->prompt_empty_subject, NULL);	
	
	/* Only Bcc */
	bonobo_config_set_boolean (config->db, "/Mail/Prompts/only_bcc",
				   config->prompt_only_bcc, NULL);	
	
	/* PGP/GPG */
	bonobo_config_set_string_wrapper (config->db, "/Mail/PGP/path", 
					  config->pgp_path, NULL);
	
	bonobo_config_set_long (config->db, "/Mail/PGP/type", 
				config->pgp_type, NULL);
	
	bonobo_config_set_boolean (config->db, "/Mail/Prompts/remember_passphrase",
				   config->remember_pgp_passphrase, NULL);	
	
	/* HTTP images */
	bonobo_config_set_long (config->db, "/Mail/Display/http_images", 
				config->http_mode, NULL);
	
	/* Forwarding */
	bonobo_config_set_long (config->db, 
				"/Mail/Format/default_forward_style", 
				config->default_forward_style, NULL);
	
	/* Message Display */
	bonobo_config_set_long (config->db, 
				"/Mail/Format/message_display_style", 
				config->message_display_style, NULL);
	
	/* Default charset */
	bonobo_config_set_string_wrapper (config->db, "/Mail/Format/default_charset", 
					  config->default_charset, NULL);
	
	/* Trash folders */
	bonobo_config_set_boolean (config->db, "/Mail/Trash/empty_on_exit", 
				   config->empty_trash_on_exit, NULL);
	
	g_hash_table_foreach_remove (config->threaded_hash, 
				     hash_save_state, "Threads");
	
	g_hash_table_foreach_remove (config->preview_hash, 
				     hash_save_state, "Preview");
	
	CORBA_exception_init (&ev);
	Bonobo_ConfigDatabase_sync (config->db, &ev);
	CORBA_exception_free (&ev);
	
	/* Passwords */
	/* fixme: still depends on gnome-config */
	gnome_config_private_clean_section ("/Evolution/Passwords");
	sources = mail_config_get_sources ();
	for ( ; sources; sources = sources->next) {
		s = sources->data;
		if (s->save_passwd && s->url)
			mail_session_remember_password (s->url);
	}
	g_slist_free (sources);
	gnome_config_sync ();
				
	/* now do cleanup */
	mail_config_clear ();
}

/* Accessor functions */
gboolean
mail_config_is_configured (void)
{
	return config->accounts != NULL;
}

static char *
uri_to_key (const char *uri)
{
	char *rval;
	int i = 0;
	
	if (!uri)
		return NULL;
	
	rval = g_strdup (uri);
	
	while (rval [i]) {	
		if (rval [i] == '/' || rval [i] == ':')
			rval [i] = '_';
		
		i++;
	}
	
	return rval;
}

gboolean
mail_config_get_empty_trash_on_exit (void)
{
	return config->empty_trash_on_exit;
}

void
mail_config_set_empty_trash_on_exit (gboolean value)
{
	config->empty_trash_on_exit = value;
}

gboolean
mail_config_get_show_preview (const char *uri)
{
	if (uri) {
		gpointer key, val;
		char *dbkey;
		
		dbkey = uri_to_key (uri);
		
		if (!config->preview_hash)
			config->preview_hash = g_hash_table_new (g_str_hash, g_str_equal);
		
		if (!g_hash_table_lookup_extended (config->preview_hash, dbkey, &key, &val)) {
			gboolean value;
			char *str;
			
			str = g_strdup_printf ("/Mail/Preview/%s", dbkey);
			value = bonobo_config_get_boolean_with_default (config->db, str, TRUE, NULL);
			g_free (str);
			
			g_hash_table_insert (config->preview_hash, dbkey,
					     GINT_TO_POINTER (value));
			
			return value;
		} else
			return GPOINTER_TO_INT (val);
	}
	
	/* return the default value */
	
	return config->show_preview;
}

void
mail_config_set_show_preview (const char *uri, gboolean value)
{
	if (uri) {
		char *dbkey = uri_to_key (uri);
		gpointer key, val;
		
		if (!config->preview_hash)
			config->preview_hash = g_hash_table_new (g_str_hash, g_str_equal);
		
		if (g_hash_table_lookup_extended (config->preview_hash, dbkey, &key, &val)) {
			g_hash_table_insert (config->preview_hash, dbkey,
					     GINT_TO_POINTER (value));
			g_free (dbkey);
		} else {
			g_hash_table_insert (config->preview_hash, dbkey, 
					     GINT_TO_POINTER (value));
		}
	} else
		config->show_preview = value;
}

gboolean
mail_config_get_thread_list (const char *uri)
{
	if (uri) {
		gpointer key, val;
		char *dbkey;
		
		dbkey = uri_to_key (uri);
		
		if (!config->threaded_hash)
			config->threaded_hash = g_hash_table_new (g_str_hash, g_str_equal);
		
		if (!g_hash_table_lookup_extended (config->threaded_hash, dbkey, &key, &val)) {
			gboolean value;
			char *str;
			
			str = g_strdup_printf ("/Mail/Threads/%s", dbkey);
			value = bonobo_config_get_boolean_with_default (config->db, str, FALSE, NULL);
			g_free (str);
			
			g_hash_table_insert (config->threaded_hash, dbkey,
					     GINT_TO_POINTER (value));
			
			return value;
		} else
			return GPOINTER_TO_INT (val);
	}
	
	/* return the default value */
	
	return config->thread_list;
}

void
mail_config_set_thread_list (const char *uri, gboolean value)
{
	if (uri) {
		char *dbkey = uri_to_key (uri);
		gpointer key, val;
		
		if (!config->threaded_hash)
			config->threaded_hash = g_hash_table_new (g_str_hash, g_str_equal);
		
		if (g_hash_table_lookup_extended (config->threaded_hash, dbkey, &key, &val)) {
			g_hash_table_insert (config->threaded_hash, dbkey,
					     GINT_TO_POINTER (value));
			g_free (dbkey);
		} else {
			g_hash_table_insert (config->threaded_hash, dbkey, 
					     GINT_TO_POINTER (value));
		}
	} else
		config->thread_list = value;
}

gboolean
mail_config_get_hide_deleted (void)
{
	return config->hide_deleted;
}

void
mail_config_set_hide_deleted (gboolean value)
{
	config->hide_deleted = value;
}

gint
mail_config_get_paned_size (void)
{
	return config->paned_size;
}

void
mail_config_set_paned_size (gint value)
{
	config->paned_size = value;
}

gboolean
mail_config_get_send_html (void)
{
	return config->send_html;
}

void
mail_config_set_send_html (gboolean send_html)
{
	config->send_html = send_html;
}

gboolean
mail_config_get_citation_highlight (void)
{
	return config->citation_highlight;
}

void
mail_config_set_citation_highlight (gboolean citation_highlight)
{
	config->citation_highlight = citation_highlight;
}

guint32
mail_config_get_citation_color (void)
{
	return config->citation_color;
}

void
mail_config_set_citation_color (guint32 citation_color)
{
	config->citation_color = citation_color;
}

gboolean
mail_config_get_do_seen_timeout (void)
{
	return config->do_seen_timeout;
}

void
mail_config_set_do_seen_timeout (gboolean do_seen_timeout)
{
	config->do_seen_timeout = do_seen_timeout;
}

gint
mail_config_get_mark_as_seen_timeout (void)
{
	return config->seen_timeout;
}

void
mail_config_set_mark_as_seen_timeout (gint timeout)
{
	config->seen_timeout = timeout;
}

gboolean
mail_config_get_prompt_empty_subject (void)
{
	return config->prompt_empty_subject;
}

void
mail_config_set_prompt_empty_subject (gboolean value)
{
	config->prompt_empty_subject = value;
}

gboolean
mail_config_get_prompt_only_bcc (void)
{
	return config->prompt_only_bcc;
}

void
mail_config_set_prompt_only_bcc (gboolean value)
{
	config->prompt_only_bcc = value;
}


struct {
	char *bin;
	CamelPgpType type;
} binaries[] = {
	{ "gpg", CAMEL_PGP_TYPE_GPG },
	{ "pgpv", CAMEL_PGP_TYPE_PGP5 },
	{ "pgp", CAMEL_PGP_TYPE_PGP2 },
	{ NULL, CAMEL_PGP_TYPE_NONE }
};

/* FIXME: what about PGP 6.x? And I assume we want to "prefer" GnuPG
   over the other, which is done now, but after that do we have a
   order-of-preference for the rest? */
static void
auto_detect_pgp_variables (void)
{
	CamelPgpType type = CAMEL_PGP_TYPE_NONE;
	const char *PATH, *path;
	char *pgp = NULL;
	
	PATH = getenv ("PATH");
	
	path = PATH;
	while (path && *path && !type) {
		const char *pend = strchr (path, ':');
		char *dirname;
		int i;
		
		if (pend) {
			/* don't even think of using "." */
			if (!strncmp (path, ".", pend - path)) {
				path = pend + 1;
				continue;
			}
			
			dirname = g_strndup (path, pend - path);
			path = pend + 1;
		} else {
			/* don't even think of using "." */
			if (!strcmp (path, "."))
				break;
			
			dirname = g_strdup (path);
			path = NULL;
		}
		
		for (i = 0; binaries[i].bin; i++) {
			struct stat st;
			
			pgp = g_strdup_printf ("%s/%s", dirname, binaries[i].bin);
			/* make sure the file exists *and* is executable? */
			if (stat (pgp, &st) != -1 && st.st_mode & (S_IXOTH | S_IXGRP | S_IXUSR)) {
				type = binaries[i].type;
				break;
			}
			
			g_free (pgp);
			pgp = NULL;
		}
		
		g_free (dirname);
	}
	
	if (pgp && type) {
		mail_config_set_pgp_path (pgp);
		mail_config_set_pgp_type (type);
	}
	
	g_free (pgp);
}

CamelPgpType
mail_config_get_pgp_type (void)
{
	if (!config->pgp_path || !config->pgp_type)
		auto_detect_pgp_variables ();
	
	return config->pgp_type;
}

void
mail_config_set_pgp_type (CamelPgpType pgp_type)
{
	config->pgp_type = pgp_type;
}

const char *
mail_config_get_pgp_path (void)
{
	if (!config->pgp_path || !config->pgp_type)
		auto_detect_pgp_variables ();
	
	return config->pgp_path;
}

void
mail_config_set_pgp_path (const char *pgp_path)
{
	g_free (config->pgp_path);
	
	config->pgp_path = g_strdup (pgp_path);
}

gboolean
mail_config_get_remember_pgp_passphrase (void)
{
	return config->remember_pgp_passphrase;
}

void
mail_config_set_remember_pgp_passphrase (gboolean value)
{
	config->remember_pgp_passphrase = value;
}

MailConfigHTTPMode
mail_config_get_http_mode (void)
{
	return config->http_mode;
}

void
mail_config_set_http_mode (MailConfigHTTPMode mode)
{
	config->http_mode = mode;
}

MailConfigForwardStyle
mail_config_get_default_forward_style (void)
{
	return config->default_forward_style;
}

void
mail_config_set_default_forward_style (MailConfigForwardStyle style)
{
	config->default_forward_style = style;
}

MailConfigDisplayStyle
mail_config_get_message_display_style (void)
{
	return config->message_display_style;
}

void
mail_config_set_message_display_style (MailConfigDisplayStyle style)
{
	config->message_display_style = style;
}

const char *
mail_config_get_default_charset (void)
{
	return config->default_charset;
}

void
mail_config_set_default_charset (const char *charset)
{
	g_free (config->default_charset);
	config->default_charset = g_strdup (charset);
}


const MailConfigAccount *
mail_config_get_default_account (void)
{
	MailConfigAccount *account;
	
	if (!config->accounts)
		return NULL;
	
	account = g_slist_nth_data (config->accounts,
				   config->default_account);
	
	/* Looks like we have no default, so make the first account
           the default */
	if (account == NULL) {
		mail_config_set_default_account_num (0);
		account = config->accounts->data;
	}
	
	return account;
}

const MailConfigAccount *
mail_config_get_account_by_name (const char *account_name)
{
	/* FIXME: this should really use a hash */
	const MailConfigAccount *account;
	GSList *l;
	
	l = config->accounts;
	while (l) {
		account = l->data;
		if (account && !strcmp (account->name, account_name))
			return account;
		
		l = l->next;
	}
	
	return NULL;
}

const MailConfigAccount *
mail_config_get_account_by_source_url (const char *source_url)
{
	const MailConfigAccount *account;
	GSList *l;

	g_return_val_if_fail (source_url != NULL, NULL);

	l = config->accounts;
	while (l) {
		account = l->data;
		if (account
		    && account->source 
		    && account->source->url
		    && e_url_equal (account->source->url, source_url))
			return account;
		
		l = l->next;
	}
	
	return NULL;
}

const GSList *
mail_config_get_accounts (void)
{
	return config->accounts;
}

void
mail_config_add_account (MailConfigAccount *account)
{
	config->accounts = g_slist_append (config->accounts, account);
}

const GSList *
mail_config_remove_account (MailConfigAccount *account)
{
	/* Removing the current default, so make the first account the
           default */
	if (account == mail_config_get_default_account ())
		config->default_account = 0;

	config->accounts = g_slist_remove (config->accounts, account);
	account_destroy (account);

	return config->accounts;
}

gint
mail_config_get_default_account_num (void)
{
	return config->default_account;
}

static void
mail_config_set_default_account_num (gint new_default)
{
	config->default_account = new_default;
}

void
mail_config_set_default_account (const MailConfigAccount *account)
{
	int position;

	position = g_slist_index (config->accounts, (void*)account);

	config->default_account = position;

	return;
}

const MailConfigIdentity *
mail_config_get_default_identity (void)
{
	const MailConfigAccount *account;
	
	account = mail_config_get_default_account ();
	if (account)
		return account->id;
	else
		return NULL;
}

const MailConfigService *
mail_config_get_default_transport (void)
{
	const MailConfigAccount *account;
	
	account = mail_config_get_default_account ();
	if (account)
		return account->transport;
	else
		return NULL;
}

const MailConfigService *
mail_config_get_default_news (void)
{
	if (!config->news)
		return NULL;
	
	return (MailConfigService *)config->news->data;
}

const GSList *
mail_config_get_news (void)
{
	return config->news;
}

void
mail_config_add_news (MailConfigService *news) 
{
	config->news = g_slist_append (config->news, news);
}

const GSList *
mail_config_remove_news (MailConfigService *news)
{
	config->news = g_slist_remove (config->news, news);
	service_destroy (news);
	
	return config->news;
}

GSList *
mail_config_get_sources (void)
{
	const GSList *accounts;
	GSList *sources = NULL;
	
	accounts = mail_config_get_accounts ();
	while (accounts) {
		const MailConfigAccount *account = accounts->data;
		
		if (account->source)
			sources = g_slist_append (sources, account->source);
		
		accounts = accounts->next;
	}
	
	return sources;
}

void 
mail_config_service_set_save_passwd (MailConfigService *service, gboolean save_passwd)
{
	service->save_passwd = save_passwd;
	mail_config_write (); /*bleah*/
}

char *
mail_config_folder_to_cachename (CamelFolder *folder, const char *prefix)
{
	CamelService *service = CAMEL_SERVICE (folder->parent_store);
	char *url, *filename;

	/* This is the way it is for backward compatibility with
	 * the way it was, not because it's necessarily a good thing.
	 */

	url = camel_url_to_string (service->url, CAMEL_URL_HIDE_PASSWORD | CAMEL_URL_HIDE_PARAMS);

	/* Really we want to check CAMEL_IS_LOCAL_FOLDER here, but we
	 * can't do that.
	 */
	if (service->provider->flags & CAMEL_PROVIDER_IS_REMOTE) {
		char *store_url = url;
		url = g_strdup_printf ("%s/%s", store_url, folder->full_name);
		g_free (store_url);
	}
	e_filename_make_safe (url);

	filename = g_strdup_printf ("%s/config/%s%s", evolution_dir, prefix, url);
	g_free (url);
	
	return filename;
}


/* Async service-checking/authtype-lookup code. */
struct _check_msg {
	struct _mail_msg msg;

	const char *url;
	CamelProviderType type;
	GList **authtypes;
	gboolean *success;
};

static void
check_service_check (struct _mail_msg *mm)
{
	struct _check_msg *m = (struct _check_msg *)mm;
	CamelService *service = NULL;

	camel_operation_register(mm->cancel);

	service = camel_session_get_service (session, m->url, m->type, &mm->ex);
	if (!service) {
		camel_operation_unregister(mm->cancel);
		return;
	}

	if (m->authtypes)
		*m->authtypes = camel_service_query_auth_types (service, &mm->ex);
	else
		camel_service_connect (service, &mm->ex);

	camel_object_unref (CAMEL_OBJECT (service));
	*m->success = !camel_exception_is_set(&mm->ex);

	camel_operation_unregister(mm->cancel);
}

static struct _mail_msg_op check_service_op = {
	NULL,
	check_service_check,
	NULL,
	NULL
};

static void
check_cancelled (GnomeDialog *dialog, int button, gpointer data)
{
	int *msg_id = data;

	mail_msg_cancel (*msg_id);
}

/**
 * mail_config_check_service:
 * @url: service url
 * @type: provider type
 * @authtypes: set to list of supported authtypes on return if non-%NULL.
 *
 * Checks the service for validity. If @authtypes is non-%NULL, it will
 * be filled in with a list of supported authtypes.
 *
 * Return value: %TRUE on success or %FALSE on error.
 **/
gboolean
mail_config_check_service (const char *url, CamelProviderType type, GList **authtypes)
{
	gboolean ret = FALSE;
	struct _check_msg *m;
	int id;
	GtkWidget *dialog, *label;

	m = mail_msg_new(&check_service_op, NULL, sizeof(*m));
	m->url = url;
	m->type = type;
	m->authtypes = authtypes;
	m->success = &ret;

	id = m->msg.seq;
	e_thread_put(mail_thread_queued, (EMsg *)m);

	dialog = gnome_dialog_new (_("Connecting to server..."),
				   GNOME_STOCK_BUTTON_CANCEL,
				   NULL);
	label = gtk_label_new (_("Connecting to server..."));
	gtk_box_pack_start (GTK_BOX(GNOME_DIALOG (dialog)->vbox),
			    label, TRUE, TRUE, 10);
	gnome_dialog_set_close (GNOME_DIALOG (dialog), FALSE);
	gtk_signal_connect (GTK_OBJECT (dialog), "clicked",
			    GTK_SIGNAL_FUNC (check_cancelled), &id);
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	gtk_widget_show_all (dialog);

	mail_msg_wait(id);

	gtk_widget_destroy (dialog);

	return ret;
}

/* MailConfig Bonobo object */
#define PARENT_TYPE BONOBO_X_OBJECT_TYPE
static BonoboObjectClass *parent_class = NULL;

/* For the bonobo object */
typedef struct _EvolutionMailConfig EvolutionMailConfig;
typedef struct _EvolutionMailConfigClass EvolutionMailConfigClass;

struct _EvolutionMailConfig {
	BonoboXObject parent;
};

struct _EvolutionMailConfigClass {
	BonoboXObjectClass parent_class;

	POA_GNOME_Evolution_MailConfig__epv epv;
};

static void
impl_GNOME_Evolution_MailConfig_addAccount (PortableServer_Servant servant,
					    const GNOME_Evolution_MailConfig_Account *account,
					    CORBA_Environment *ev)
{
	GNOME_Evolution_MailConfig_Service source, transport;
	GNOME_Evolution_MailConfig_Identity id;
	MailConfigAccount *mail_account;
	MailConfigService *mail_service;
	MailConfigIdentity *mail_id;

	mail_account = g_new0 (MailConfigAccount, 1);
	mail_account->name = g_strdup (account->name);

	/* Copy ID */
	id = account->id;
	mail_id = g_new0 (MailConfigIdentity, 1);
	mail_id->name = g_strdup (id.name);
	mail_id->address = g_strdup (id.address);
	mail_id->organization = g_strdup (id.organization);
	mail_id->signature = g_strdup (id.signature);
	mail_id->html_signature = g_strdup (id.html_signature);
	mail_id->has_html_signature = id.has_html_signature;

	mail_account->id = mail_id;

	/* Copy source */
	source = account->source;
	mail_service = g_new0 (MailConfigService, 1);
	mail_service->url = g_strdup (source.url);
	mail_service->keep_on_server = source.keep_on_server;
	mail_service->auto_check = source.auto_check;
	mail_service->auto_check_time = source.auto_check_time;
	mail_service->save_passwd = source.save_passwd;
	mail_service->enabled = source.enabled;

	mail_account->source = mail_service;

	/* Copy transport */
	transport = account->transport;
	mail_service = g_new0 (MailConfigService, 1);
	mail_service->url = g_strdup (transport.url);
	mail_service->keep_on_server = transport.keep_on_server;
	mail_service->auto_check = transport.auto_check;
	mail_service->auto_check_time = transport.auto_check_time;
	mail_service->save_passwd = transport.save_passwd;
	mail_service->enabled = transport.enabled;

	mail_account->transport = mail_service;

	/* Add new account */
	mail_config_add_account (mail_account);
}

static void
evolution_mail_config_class_init (EvolutionMailConfigClass *klass)
{
	POA_GNOME_Evolution_MailConfig__epv *epv = &klass->epv;

	parent_class = gtk_type_class (PARENT_TYPE);
	epv->addAccount = impl_GNOME_Evolution_MailConfig_addAccount;
}

static void
evolution_mail_config_init (EvolutionMailConfig *config)
{
}

BONOBO_X_TYPE_FUNC_FULL (EvolutionMailConfig,
			 GNOME_Evolution_MailConfig,
			 PARENT_TYPE,
			 evolution_mail_config);

static BonoboObject *
evolution_mail_config_factory_fn (BonoboGenericFactory *factory,
				  void *closure)
{
	EvolutionMailConfig *config;

	g_warning ("Made");
	config = gtk_type_new (evolution_mail_config_get_type ());
	return BONOBO_OBJECT (config);
}

void
evolution_mail_config_factory_init (void)
{
	BonoboGenericFactory *factory;
	
	g_warning ("Starting mail config");
	factory = bonobo_generic_factory_new (MAIL_CONFIG_IID, 
					      evolution_mail_config_factory_fn,
					      NULL);
	if (factory == NULL) {
		g_warning ("Error starting MailConfig");
	}

	g_warning ("Registered");
	bonobo_running_context_auto_exit_unref (BONOBO_OBJECT (factory));
}
