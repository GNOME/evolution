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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* this group of headers is for pgp detection */
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>

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

#include <shell/evolution-shell-client.h>

#include <gal/util/e-unicode-i18n.h>
#include <gal/util/e-util.h>
#include <gal/unicode/gunicode.h>
#include <gal/widgets/e-gui-utils.h>
#include <e-util/e-html-utils.h>
#include <e-util/e-url.h>
#include <e-util/e-passwords.h>
#include "mail.h"
#include "mail-config.h"
#include "mail-mt.h"
#include "mail-tools.h"

#include "Mail.h"

typedef struct {
	Bonobo_ConfigDatabase db;
	
	gboolean corrupt;
	
	gboolean show_preview;
	gboolean thread_list;
	gboolean hide_deleted;
	int paned_size;
	gboolean send_html;
	gboolean confirm_unwanted_html;
	gboolean citation_highlight;
	guint32  citation_color;
	gboolean prompt_empty_subject;
	gboolean prompt_only_bcc;
	gboolean confirm_expunge;
	gboolean confirm_goto_next_folder;
	gboolean goto_next_folder;
	gboolean do_seen_timeout;
	int seen_timeout;
	gboolean empty_trash_on_exit;
	
	GSList *accounts;
	int default_account;
	
	GSList *news;
	
	char *pgp_path;
	CamelPgpType pgp_type;
	
	MailConfigHTTPMode http_mode;
	MailConfigForwardStyle default_forward_style;
	MailConfigDisplayStyle message_display_style;
	char *default_charset;
	
	GHashTable *threaded_hash;
	GHashTable *preview_hash;
	
	gboolean filter_log;
	char *filter_log_path;
	
	MailConfigNewMailNotify notify;
	char *notify_filename;
	
	char *last_filesel_dir;

	GList *signature_list;
	gint signatures;
	gint signatures_random;
} MailConfig;

static MailConfig *config = NULL;

#define MAIL_CONFIG_IID "OAFIID:GNOME_Evolution_MailConfig_Factory"

/* Prototypes */
static void config_read (void);
static void mail_config_set_default_account_num (int new_default);

/* signatures */
MailConfigSignature *
signature_copy (const MailConfigSignature *sig)
{
	MailConfigSignature *ns;

	g_return_val_if_fail (sig != NULL, NULL);
	
	ns = g_new (MailConfigSignature, 1);

	ns->id = sig->id;
	ns->name = g_strdup (sig->name);
	ns->filename = g_strdup (sig->filename);
	ns->script = g_strdup (sig->script);
	ns->random = sig->random;
	ns->html = sig->html;

	return ns;
}

void
signature_destroy (MailConfigSignature *sig)
{
	g_free (sig->name);
	g_free (sig->filename);
	g_free (sig->script);
	g_free (sig);
}

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
	new->text_signature = id->text_signature;
	new->text_random = id->text_random;
	new->html_signature = id->html_signature;
	new->html_random = id->html_random;

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
	
	new->always_cc = account->always_cc;
	new->cc_addrs = g_strdup (account->cc_addrs);
	new->always_bcc = account->always_bcc;
	new->bcc_addrs = g_strdup (account->bcc_addrs);
	
	new->pgp_key = g_strdup (account->pgp_key);
	new->pgp_encrypt_to_self = account->pgp_encrypt_to_self;
	new->pgp_always_sign = account->pgp_always_sign;
	
	new->smime_key = g_strdup (account->smime_key);
	new->smime_encrypt_to_self = account->smime_encrypt_to_self;
	new->smime_always_sign = account->smime_always_sign;
	
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
	
	g_free (account->cc_addrs);
	g_free (account->bcc_addrs);
	
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
		char *err;
		g_error ("Very serious error, cannot activate config database '%s'",
			 (err = bonobo_exception_get_text (&ev)));
		g_free (err);
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
	
	g_free (config->pgp_path);
	config->pgp_path = NULL;
	
	g_free (config->default_charset);
	config->default_charset = NULL;
	
	g_free (config->filter_log_path);
	config->filter_log_path = NULL;
	
	g_free (config->notify_filename);
	config->notify_filename = NULL;
	
	g_free (config->last_filesel_dir);
	config->last_filesel_dir = NULL;
}

static MailConfigSignature *
config_read_signature (gint i)
{
	MailConfigSignature *sig;
	gchar *path, *val;

	sig = g_new0 (MailConfigSignature, 1);

	sig->id = i;
	
	path = g_strdup_printf ("/Mail/Signatures/name_%d", i);
	val = bonobo_config_get_string (config->db, path, NULL);
	g_free (path);
	if (val && *val)
		sig->name = val;
	else
		g_free (val);

	path = g_strdup_printf ("/Mail/Signatures/filename_%d", i);
	val = bonobo_config_get_string (config->db, path, NULL);
	g_free (path);
	if (val && *val)
		sig->filename = val;
	else
		g_free (val);

	path = g_strdup_printf ("/Mail/Signatures/script_%d", i);
	val = bonobo_config_get_string (config->db, path, NULL);
	g_free (path);
	if (val && *val)
		sig->script = val;
	else
		g_free (val);

	path = g_strdup_printf ("/Mail/Signatures/random_%d", i);
	sig->random = bonobo_config_get_boolean_with_default (config->db, path, FALSE, NULL);

	path = g_strdup_printf ("/Mail/Signatures/html_%d", i);
	sig->html = bonobo_config_get_boolean_with_default (config->db, path, FALSE, NULL);

	return sig;
}

static void
config_read_signatures ()
{
	MailConfigSignature *sig;
	gint i;

	config->signature_list = NULL;
	config->signatures_random = 0;
	config->signatures = bonobo_config_get_long_with_default (config->db, "/Mail/Signatures/num", 0, NULL);

	for (i = 0; i < config->signatures; i ++) {
		sig = config_read_signature (i);
		config->signature_list = g_list_append (config->signature_list, sig);
		if (sig->random)
			config->signatures_random ++;
	}
}

static void
config_write_signature (MailConfigSignature *sig, gint i)
{
	gchar *path;

	printf ("config_write_signature i: %d id: %d\n", i, sig->id);

	path = g_strdup_printf ("/Mail/Signatures/name_%d", i);
	bonobo_config_set_string (config->db, path, sig->name ? sig->name : "", NULL);
	g_free (path);

	path = g_strdup_printf ("/Mail/Signatures/filename_%d", i);
	bonobo_config_set_string (config->db, path, sig->filename ? sig->filename : "", NULL);
	g_free (path);

	path = g_strdup_printf ("/Mail/Signatures/script_%d", i);
	bonobo_config_set_string (config->db, path, sig->script ? sig->script : "", NULL);
	g_free (path);

	path = g_strdup_printf ("/Mail/Signatures/random_%d", i);
	bonobo_config_set_boolean (config->db, path, sig->random, NULL);

	path = g_strdup_printf ("/Mail/Signatures/html_%d", i);
	bonobo_config_set_boolean (config->db, path, sig->html, NULL);
}

static void
config_write_signatures_num ()
{
	bonobo_config_set_long (config->db, "/Mail/Signatures/num", config->signatures, NULL);
}

static void
config_write_signatures ()
{
	GList *l;
	gint id;

	for (id = 0, l = config->signature_list; l; l = l->next, id ++) {
		config_write_signature ((MailConfigSignature *) l->data, id);
	}

	config_write_signatures_num ();
}

static MailConfigSignature *
lookup_signature (gint i)
{
	MailConfigSignature *sig;
	GList *l;

	if (i == -1)
		return NULL;

	for (l = config->signature_list; l; l = l->next) {
		sig = (MailConfigSignature *) l->data;
		if (sig->id == i)
			return sig;
	}

	return NULL;
}

static void
config_write_imported_signature (gchar *filename, gint i, gboolean html)
{
	MailConfigSignature *sig = g_new0 (MailConfigSignature, 1);
	gchar *name;

	name = strrchr (filename, '/');
	if (!name)
		name = filename;
	else
		name ++;

	sig->name = g_strdup (name);
	sig->filename = filename;
	sig->html = html;

	config_write_signature (sig, i);
	signature_destroy (sig);
}

static void
config_import_old_signatures ()
{
	gint num;

	num = bonobo_config_get_long_with_default (config->db, "/Mail/Signatures/num", -1, NULL);

	if (num == -1) {
		/* there are no signatures defined
		 * look for old config to create new ones from old ones
		 */

		GHashTable *cache;
		gint i, accounts;

		cache = g_hash_table_new (g_str_hash, g_str_equal);
		accounts = bonobo_config_get_long_with_default (config->db, "/Mail/Accounts/num", 0, NULL);
		num = 0;
		for (i = 0; i < accounts; i ++) {
			gchar *path, *val;

			/* read text signature file */
			path = g_strdup_printf ("/Mail/Accounts/identity_signature_%d", i);
			val = bonobo_config_get_string (config->db, path, NULL);
			g_free (path);
			if (val && *val) {

				gint id;
				gpointer orig_key, node_val;

				if (g_hash_table_lookup_extended (cache, val, &orig_key, &node_val)) {
					id = GPOINTER_TO_INT (node_val);
				} else {
					g_hash_table_insert (cache, g_strdup (val), GINT_TO_POINTER (id));
					config_write_imported_signature (val, num, FALSE);
					id = num;
					num ++;
				}

				/* set new text signature to this identity */
				path = g_strdup_printf ("/Mail/Accounts/identity_signature_text_%d", i);
				bonobo_config_set_long (config->db, path, id, NULL);
				g_free (path);
			} else
				g_free (val);

			path = g_strdup_printf ("/Mail/Accounts/identity_has_html_signature_%d", i);
			if (bonobo_config_get_boolean_with_default (config->db, path, FALSE, NULL)) {
				g_free (path);
				path = g_strdup_printf ("/Mail/Accounts/identity_html_signature_%d", i);
				val = bonobo_config_get_string (config->db, path, NULL);
				if (val && *val) {

					gint id;
					gpointer orig_key, node_val;

					if (g_hash_table_lookup_extended (cache, val, &orig_key, &node_val)) {
						id = GPOINTER_TO_INT (node_val);
					} else {
						g_hash_table_insert (cache, g_strdup (val), GINT_TO_POINTER (id));
						config_write_imported_signature (val, num, TRUE);
						id = num;
						num ++;
					}

					/* set new html signature to this identity */
					g_free (path);
					path = g_strdup_printf ("/Mail/Accounts/identity_signature_html_%d", i);
					bonobo_config_set_long (config->db, path, id, NULL);
				} else
					g_free (val);
			}
			g_free (path);
		}
		bonobo_config_set_long (config->db, "/Mail/Signatures/num", num, NULL);
		g_hash_table_destroy (cache);
	}
}

static void
config_read (void)
{
	int len, i, default_num;
	
	mail_config_clear ();

	config_import_old_signatures ();
	config_read_signatures ();
	
	len = bonobo_config_get_long_with_default (config->db, 
	        "/Mail/Accounts/num", 0, NULL);
	
	for (i = 0; i < len; i++) {
		MailConfigAccount *account;
		MailConfigIdentity *id;
		MailConfigService *source;
		MailConfigService *transport;
		char *path, *val;
		
		account = g_new0 (MailConfigAccount, 1);
		path = g_strdup_printf ("/Mail/Accounts/account_name_%d", i);
		val = bonobo_config_get_string (config->db, path, NULL);
		g_free (path);
		if (val && *val) {
			account->name = val;
		} else {
			g_free (val);
			account->name = g_strdup_printf (U_("Account %d"), i + 1);
			config->corrupt = TRUE;
		}
		
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
		
		path = g_strdup_printf ("/Mail/Accounts/account_always_cc_%d", i);
		account->always_cc = bonobo_config_get_boolean_with_default (
		        config->db, path, FALSE, NULL);
		g_free (path);
		
		path = g_strdup_printf ("/Mail/Accounts/account_always_cc_addrs_%d", i);
		val = bonobo_config_get_string (config->db, path, NULL);
		g_free (path);
		if (val && *val)
			account->cc_addrs = val;
		else
			g_free (val);
		
		path = g_strdup_printf ("/Mail/Accounts/account_always_bcc_%d", i);
		account->always_bcc = bonobo_config_get_boolean_with_default (
		        config->db, path, FALSE, NULL);
		g_free (path);
		
		path = g_strdup_printf ("/Mail/Accounts/account_always_bcc_addrs_%d", i);
		val = bonobo_config_get_string (config->db, path, NULL);
		g_free (path);
		if (val && *val)
			account->bcc_addrs = val;
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
		
		path = g_strdup_printf ("/Mail/Accounts/account_pgp_always_sign_%d", i);
		account->pgp_always_sign = bonobo_config_get_boolean_with_default (
		        config->db, path, FALSE, NULL);
		g_free (path);
		
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
		
		path = g_strdup_printf ("/Mail/Accounts/account_smime_always_sign_%d", i);
		account->smime_always_sign = bonobo_config_get_boolean_with_default (
		        config->db, path, FALSE, NULL);
		g_free (path);
		
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
		
		/* id signatures */
		path = g_strdup_printf ("/Mail/Accounts/identity_signature_text_%d", i);
		id->text_signature = lookup_signature (bonobo_config_get_long_with_default (config->db, path, -1, NULL));
		g_free (path);

		path = g_strdup_printf ("/Mail/Accounts/identity_signature_html_%d", i);
		id->html_signature = lookup_signature (bonobo_config_get_long_with_default (config->db, path, -1, NULL));
		g_free (path);

		path = g_strdup_printf ("/Mail/Accounts/identity_signature_text_random_%d", i);
		id->text_random = bonobo_config_get_boolean_with_default (config->db, path, FALSE, NULL);
		g_free (path);

		path = g_strdup_printf ("/Mail/Accounts/identity_signature_html_random_%d", i);
		id->html_random = bonobo_config_get_boolean_with_default (config->db, path, FALSE, NULL);
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
		char *path, *r;
		
		path = g_strdup_printf ("/News/Sources/url_%d", i);
		
		if ((r = bonobo_config_get_string (config->db, path, NULL))) {
			n = g_new0 (MailConfigService, 1);		
			n->url = r;
			n->enabled = TRUE;
			config->news = g_slist_append (config->news, n);
		} 
		
		g_free (path);
		
	}
#endif
	
	/* Format */
	config->send_html = bonobo_config_get_boolean_with_default (config->db,
	        "/Mail/Format/send_html", FALSE, NULL);

	/* Confirm Sending Unwanted HTML */
	config->confirm_unwanted_html = bonobo_config_get_boolean_with_default (config->db,
	        "/Mail/Format/confirm_unwanted_html", TRUE, NULL);
	
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
	
	/* Goto next folder when user has reached the bottom of the message-list */
	config->goto_next_folder = bonobo_config_get_boolean_with_default (
		config->db, "/Mail/MessageList/goto_next_folder", FALSE, NULL);
	
	/* Empty Subject */
	config->prompt_empty_subject = bonobo_config_get_boolean_with_default (
		config->db, "/Mail/Prompts/empty_subject", TRUE, NULL);
	
	/* Only Bcc */
	config->prompt_only_bcc = bonobo_config_get_boolean_with_default (
		config->db, "/Mail/Prompts/only_bcc", TRUE, NULL);
	
	/* Expunge */
	config->confirm_expunge = bonobo_config_get_boolean_with_default (
		config->db, "/Mail/Prompts/confirm_expunge", TRUE, NULL);
	
	/* Goto next folder */
	config->confirm_goto_next_folder = bonobo_config_get_boolean_with_default (
		config->db, "/Mail/Prompts/confirm_goto_next_folder", TRUE, NULL);
	
	/* PGP/GPG */
	config->pgp_path = bonobo_config_get_string (config->db, 
						     "/Mail/PGP/path", NULL);
	
	config->pgp_type = bonobo_config_get_long_with_default (config->db, 
	        "/Mail/PGP/type", CAMEL_PGP_TYPE_NONE, NULL);
	
	/* HTTP images */
	config->http_mode = bonobo_config_get_long_with_default (config->db, 
		"/Mail/Display/http_images", MAIL_CONFIG_HTTP_NEVER, NULL);
	
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
	
	/* Filter logging */
	config->filter_log = bonobo_config_get_boolean_with_default (
		config->db, "/Mail/Filters/log", FALSE, NULL);
	
	config->filter_log_path = bonobo_config_get_string (
		config->db, "/Mail/Filters/log_path", NULL);
	
	/* New Mail Notification */
	config->notify = bonobo_config_get_long_with_default (
		config->db, "/Mail/Notify/new_mail_notification", 
		MAIL_CONFIG_NOTIFY_NOT, NULL);
	
	config->notify_filename = bonobo_config_get_string (
		config->db, "/Mail/Notify/new_mail_notification_sound_file", NULL);
	
	/* last filesel dir */
	config->last_filesel_dir = bonobo_config_get_string (
		config->db, "/Mail/Filesel/last_filesel_dir", NULL);
}

#define bonobo_config_set_string_wrapper(db, path, val, ev) bonobo_config_set_string (db, path, val ? val : "", ev)

void
mail_config_write_account_sig (MailConfigAccount *account, gint i)
{
	char *path;

	if (i == -1) {
		GSList *link;

		link = g_slist_find (config->accounts, account);
		if (!link) {
			g_warning ("Can't find account in accounts list");
			return;
		}
		i = g_slist_position (config->accounts, link);
	}

	/* id signatures */
	path = g_strdup_printf ("/Mail/Accounts/identity_signature_text_%d", i);
	bonobo_config_set_long (config->db, path, account->id->text_signature
				? account->id->text_signature->id : -1, NULL);
	g_free (path);

	path = g_strdup_printf ("/Mail/Accounts/identity_signature_html_%d", i);
	bonobo_config_set_long (config->db, path, account->id->html_signature
				? account->id->html_signature->id : -1, NULL);
	g_free (path);

	path = g_strdup_printf ("/Mail/Accounts/identity_signature_text_random_%d", i);
	bonobo_config_set_boolean (config->db, path, account->id->text_random, NULL);
	g_free (path);

	path = g_strdup_printf ("/Mail/Accounts/identity_signature_html_random_%d", i);
	bonobo_config_set_boolean (config->db, path, account->id->html_random, NULL);
	g_free (path);
}

void
mail_config_write (void)
{
	CORBA_Environment ev;
	int len, i, default_num;
	
	/* Accounts */
	
	if (!config)
		return;
	
	CORBA_exception_init (&ev);
	Bonobo_ConfigDatabase_removeDir (config->db, "/Mail/Accounts", &ev);
	CORBA_exception_init (&ev);
	Bonobo_ConfigDatabase_removeDir (config->db, "/News/Sources", &ev);
	CORBA_exception_init (&ev);
	Bonobo_ConfigDatabase_sync (config->db, &ev);
	
	config_write_signatures ();

	len = g_slist_length (config->accounts);
	bonobo_config_set_long (config->db,
				"/Mail/Accounts/num", len, NULL);
	
	default_num = mail_config_get_default_account_num ();
	bonobo_config_set_long (config->db,
				"/Mail/Accounts/default_account", default_num, NULL);
	
	for (i = 0; i < len; i++) {
		MailConfigAccount *account;
		char *path;
		
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
		
		path = g_strdup_printf ("/Mail/Accounts/account_always_cc_%d", i);
		bonobo_config_set_boolean (config->db, path, account->always_cc, NULL);
		g_free (path);
		
		path = g_strdup_printf ("/Mail/Accounts/account_always_cc_addrs_%d", i);
		bonobo_config_set_string_wrapper (config->db, path, 
						  account->cc_addrs, NULL);
		g_free (path);
		
		path = g_strdup_printf ("/Mail/Accounts/account_always_bcc_%d", i);
		bonobo_config_set_boolean (config->db, path, account->always_bcc, NULL);
		g_free (path);
		
		path = g_strdup_printf ("/Mail/Accounts/account_always_bcc_addrs_%d", i);
		bonobo_config_set_string_wrapper (config->db, path, 
						  account->bcc_addrs, NULL);
		g_free (path);
		
		/* account pgp options */
		path = g_strdup_printf ("/Mail/Accounts/account_pgp_key_%d", i);
		bonobo_config_set_string_wrapper (config->db, path, account->pgp_key, NULL);
		g_free (path);
		
		path = g_strdup_printf ("/Mail/Accounts/account_pgp_always_sign_%d", i);
		bonobo_config_set_boolean (config->db, path, account->pgp_always_sign, NULL);
		g_free (path);
		
		path = g_strdup_printf ("/Mail/Accounts/account_pgp_encrypt_to_self_%d", i);
		bonobo_config_set_boolean (config->db, path, 
					   account->pgp_encrypt_to_self, NULL);
		g_free (path);
		
		/* account s/mime options */
		path = g_strdup_printf ("/Mail/Accounts/account_smime_key_%d", i);
		bonobo_config_set_string_wrapper (config->db, path, account->smime_key, NULL);
		g_free (path);
		
		path = g_strdup_printf ("/Mail/Accounts/account_smime_always_sign_%d", i);
		bonobo_config_set_boolean (config->db, path, account->smime_always_sign, NULL);
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
		
		mail_config_write_account_sig (account, i);

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
		char *path;
		
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
	MailConfigAccount *account;
	const GSList *accounts;
	
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
	
	/* Confirm Sending Unwanted HTML */
	bonobo_config_set_boolean (config->db, "/Mail/Format/confirm_unwanted_html",
				   config->confirm_unwanted_html, NULL);
	
	/* Citation */
	bonobo_config_set_boolean (config->db, 
				   "/Mail/Display/citation_highlight", 
				   config->citation_highlight, NULL);
	
	bonobo_config_set_long (config->db, "/Mail/Display/citation_color",
				config->citation_color, NULL);
	
	/* Goto next folder */
	bonobo_config_set_boolean (config->db, "/Mail/MessageList/goto_next_folder",
				   config->goto_next_folder, NULL);
	
	/* Empty Subject */
	bonobo_config_set_boolean (config->db, "/Mail/Prompts/empty_subject",
				   config->prompt_empty_subject, NULL);
	
	/* Only Bcc */
	bonobo_config_set_boolean (config->db, "/Mail/Prompts/only_bcc",
				   config->prompt_only_bcc, NULL);
	
	/* Expunge */
	bonobo_config_set_boolean (config->db, "/Mail/Prompts/confirm_expunge",
				   config->confirm_expunge, NULL);
	
	/* Goto next folder */
	bonobo_config_set_boolean (config->db, "/Mail/Prompts/confirm_goto_next_folder",
				   config->confirm_goto_next_folder, NULL);
	
	/* PGP/GPG */
	bonobo_config_set_string_wrapper (config->db, "/Mail/PGP/path", 
					  config->pgp_path, NULL);
	
	bonobo_config_set_long (config->db, "/Mail/PGP/type", 
				config->pgp_type, NULL);
	
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
	
	/* Filter logging */
	bonobo_config_set_boolean (config->db, "/Mail/Filters/log",
				   config->filter_log, NULL);
	
	bonobo_config_set_string_wrapper (config->db, "/Mail/Filters/log_path",
					  config->filter_log_path, NULL);
	
	/* New Mail Notification */
	bonobo_config_set_long (config->db, "/Mail/Notify/new_mail_notification", 
				config->notify, NULL);
	
	bonobo_config_set_string_wrapper (config->db, "/Mail/Notify/new_mail_notification_sound_file",
					  config->notify_filename, NULL);
	
	/* last filesel dir */
	bonobo_config_set_string_wrapper (config->db, "/Mail/Filesel/last_filesel_dir",
					  config->last_filesel_dir, NULL);
	
	if (config->threaded_hash)
		g_hash_table_foreach_remove (config->threaded_hash, hash_save_state, "Threads");
	
	if (config->preview_hash)
		g_hash_table_foreach_remove (config->preview_hash, hash_save_state, "Preview");
	
	CORBA_exception_init (&ev);
	Bonobo_ConfigDatabase_sync (config->db, &ev);
	CORBA_exception_free (&ev);
	
	/* Passwords */
	
	/* then we make sure the ones we want to remember are in the
           session cache */
	accounts = mail_config_get_accounts ();
	for ( ; accounts; accounts = accounts->next) {
		char *passwd;
		account = accounts->data;
		if (account->source->save_passwd && account->source->url) {
			passwd = mail_session_get_password (account->source->url);
			mail_session_forget_password (account->source->url);
			mail_session_add_password (account->source->url, passwd);
			g_free (passwd);
		}
		
		if (account->transport->save_passwd && account->transport->url) {
			passwd = mail_session_get_password (account->transport->url);
			mail_session_forget_password (account->transport->url);
			mail_session_add_password (account->transport->url, passwd);
			g_free (passwd);
		}
	}
	
	/* then we clear out our component passwords */
	e_passwords_clear_component_passwords ();
	
	/* then we remember them */
	accounts = mail_config_get_accounts ();
	for ( ; accounts; accounts = accounts->next) {
		account = accounts->data;
		if (account->source->save_passwd && account->source->url)
			mail_session_remember_password (account->source->url);
		
		if (account->transport->save_passwd && account->transport->url)
			mail_session_remember_password (account->transport->url);
	}
	
	/* now do cleanup */
	mail_config_clear ();
}

/* Accessor functions */
gboolean
mail_config_is_configured (void)
{
	return config->accounts != NULL;
}

gboolean
mail_config_is_corrupt (void)
{
	return config->corrupt;
}

static char *
uri_to_key (const char *uri)
{
	char *rval, *ptr;
	
	if (!uri)
		return NULL;
	
	rval = g_strdup (uri);
	
	for (ptr = rval; *ptr; ptr++)
		if (*ptr == '/' || *ptr == ':')
			*ptr = '_';
	
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
	if (uri && *uri) {
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
		} else {
			g_free (dbkey);
			return GPOINTER_TO_INT (val);
		}
	}
	
	/* return the default value */
	
	return config->show_preview;
}

void
mail_config_set_show_preview (const char *uri, gboolean value)
{
	if (uri && *uri) {
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
	if (uri && *uri) {
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
		} else {
			g_free(dbkey);
			return GPOINTER_TO_INT (val);
		}
	}
	
	/* return the default value */
	
	return config->thread_list;
}

void
mail_config_set_thread_list (const char *uri, gboolean value)
{
	if (uri && *uri) {
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
mail_config_get_filter_log (void)
{
	return config->filter_log;
}

void
mail_config_set_filter_log (gboolean value)
{
	config->filter_log = value;
}

const char *
mail_config_get_filter_log_path (void)
{
	return config->filter_log_path;
}

void
mail_config_set_filter_log_path (const char *path)
{
	g_free (config->filter_log_path);
	config->filter_log_path = g_strdup (path);
}

const char *
mail_config_get_last_filesel_dir (void)
{
	if (config->last_filesel_dir)
		return config->last_filesel_dir;
	else
		return g_get_home_dir ();
}

void
mail_config_set_last_filesel_dir (const char *path)
{
	g_free (config->last_filesel_dir);
	config->last_filesel_dir = g_strdup (path);
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

int
mail_config_get_paned_size (void)
{
	return config->paned_size;
}

void
mail_config_set_paned_size (int value)
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
mail_config_get_confirm_unwanted_html (void)
{
	return config->confirm_unwanted_html;
}

void
mail_config_set_confirm_unwanted_html (gboolean confirm)
{
	config->confirm_unwanted_html = confirm;
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

int
mail_config_get_mark_as_seen_timeout (void)
{
	return config->seen_timeout;
}

void
mail_config_set_mark_as_seen_timeout (int timeout)
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

gboolean
mail_config_get_confirm_expunge (void)
{
	return config->confirm_expunge;
}

void
mail_config_set_confirm_expunge (gboolean value)
{
	config->confirm_expunge = value;
}

gboolean
mail_config_get_confirm_goto_next_folder (void)
{
	return config->confirm_goto_next_folder;
}

void
mail_config_set_confirm_goto_next_folder (gboolean value)
{
	config->confirm_goto_next_folder = value;
}

gboolean
mail_config_get_goto_next_folder (void)
{
	return config->goto_next_folder;
}

void
mail_config_set_goto_next_folder (gboolean value)
{
	config->goto_next_folder = value;
}

struct {
	char *bin;
	char *version;
	CamelPgpType type;
} binaries[] = {
	{ "gpg", NULL, CAMEL_PGP_TYPE_GPG },
	{ "pgp", "6.5.8", CAMEL_PGP_TYPE_PGP6 },
	{ "pgp", "5.0", CAMEL_PGP_TYPE_PGP5 },
	{ "pgp", "2.6", CAMEL_PGP_TYPE_PGP2 },
	{ NULL, NULL, CAMEL_PGP_TYPE_NONE }
};


typedef struct _PGPFILE {
	FILE *fp;
	pid_t pid;
} PGPFILE;

static PGPFILE *
pgpopen (const char *command, const char *mode)
{
	int in_fds[2], out_fds[2];
	PGPFILE *pgp = NULL;
	char **argv = NULL;
	pid_t child;
	int fd;
	
	g_return_val_if_fail (command != NULL, NULL);
	
	if (*mode != 'r' && *mode != 'w')
		return NULL;
	
	argv = g_strsplit (command, " ", 0);
	if (!argv)
		return NULL;
	
	if (pipe (in_fds) == -1)
		goto error;
	
	if (pipe (out_fds) == -1) {
		close (in_fds[0]);
		close (in_fds[1]);
		goto error;
	}
	
	if ((child = fork ()) == 0) {
		/* In child */
		int maxfd;
		
		if ((dup2 (in_fds[0], STDIN_FILENO) < 0 ) ||
		    (dup2 (out_fds[1], STDOUT_FILENO) < 0 ) ||
		    (dup2 (out_fds[1], STDERR_FILENO) < 0 )) {
			_exit (255);
		}
		
		/* Dissociate from evolution-mail's controlling
		 * terminal so that pgp/gpg won't be able to read from
		 * it: PGP 2 will fall back to asking for the password
		 * on /dev/tty if the passed-in password is incorrect.
		 * This will make that fail rather than hanging.
		 */
		setsid ();
		
		/* close all open fds that we aren't using */
		maxfd = sysconf (_SC_OPEN_MAX);
		for (fd = 0; fd < maxfd; fd++) {
			if (fd != STDIN_FILENO && fd != STDOUT_FILENO && fd != STDERR_FILENO)
				close (fd);
		}
		
		execvp (argv[0], argv);
		fprintf (stderr, "Could not execute %s: %s\n", argv[0],
			 g_strerror (errno));
		_exit (255);
	} else if (child < 0) {
		close (in_fds[0]);
		close (in_fds[1]);
		close (out_fds[0]);
		close (out_fds[1]);
		goto error;
	}
	
	/* Parent */
	g_strfreev (argv);
	
	close (in_fds[0]);   /* pgp's stdin */
	close (out_fds[1]);  /* pgp's stdout */
	
	if (mode[0] == 'r') {
		/* opening in read-mode */
		fd = out_fds[0];
		close (in_fds[1]);
	} else {
		/* opening in write-mode */
		fd = in_fds[1];
		close (out_fds[0]);
	}
	
	pgp = g_new (PGPFILE, 1);
	pgp->fp = fdopen (fd, mode);
	pgp->pid = child;
	
	return pgp;
 error:
	g_strfreev (argv);
	
	return NULL;
}

static int
pgpclose (PGPFILE *pgp)
{
	sigset_t mask, omask;
	pid_t wait_result;
	int status;
	
	if (pgp->fp) {
		fclose (pgp->fp);
		pgp->fp = NULL;
	}
	
	/* PGP5 closes fds before exiting, meaning this might be called
	 * too early. So wait a bit for the result.
	 */
	sigemptyset (&mask);
	sigaddset (&mask, SIGALRM);
	sigprocmask (SIG_BLOCK, &mask, &omask);
	alarm (1);
	wait_result = waitpid (pgp->pid, &status, 0);
	alarm (0);
	sigprocmask (SIG_SETMASK, &omask, NULL);
	
	if (wait_result == -1 && errno == EINTR) {
		/* PGP is hanging: send a friendly reminder. */
		kill (pgp->pid, SIGTERM);
		sleep (1);
		wait_result = waitpid (pgp->pid, &status, WNOHANG);
		if (wait_result == 0) {
			/* Still hanging; use brute force. */
			kill (pgp->pid, SIGKILL);
			sleep (1);
			wait_result = waitpid (pgp->pid, &status, WNOHANG);
		}
	}
	
	if (wait_result != -1 && WIFEXITED (status)) {
		g_free (pgp);
		return 0;
	} else
		return -1;
}

CamelPgpType
mail_config_pgp_type_detect_from_path (const char *pgp)
{
	const char *bin = g_basename (pgp);
	struct stat st;
	int i;
	
	/* make sure the file exists *and* is executable? */
	if (stat (pgp, &st) == -1 || !(st.st_mode & (S_IXOTH | S_IXGRP | S_IXUSR)))
		return CAMEL_PGP_TYPE_NONE;
	
	for (i = 0; binaries[i].bin; i++) {
		if (binaries[i].version) {
			/* compare version strings */
			char buffer[256], *command;
			gboolean found = FALSE;
			PGPFILE *fp;
			
			command = g_strdup_printf ("%s --version", pgp);
			fp = pgpopen (command, "r");
			g_free (command);
			if (fp) {
				while (!feof (fp->fp) && !found) {
					memset (buffer, 0, sizeof (buffer));
					fgets (buffer, sizeof (buffer), fp->fp);
					found = strstr (buffer, binaries[i].version) != NULL;
				}
				
				pgpclose (fp);
				
				if (found)
					return binaries[i].type;
			}
		} else if (!strcmp (binaries[i].bin, bin)) {
			/* no version string to compare against... */
			return binaries[i].type;
		}
	}
	
	return CAMEL_PGP_TYPE_NONE;
}

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
		gboolean found = FALSE;
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
				if (binaries[i].version) {
					/* compare version strings */
					char buffer[256], *command;
					PGPFILE *fp;
					
					command = g_strdup_printf ("%s --version", pgp);
					fp = pgpopen (command, "r");
					g_free (command);
					if (fp) {
						while (!feof (fp->fp) && !found) {
							memset (buffer, 0, sizeof (buffer));
							fgets (buffer, sizeof (buffer), fp->fp);
							found = strstr (buffer, binaries[i].version) != NULL;
						}
						
						pgpclose (fp);
					}
				} else {
					/* no version string to compare against... */
					found = TRUE;
				}
				
				if (found) {
					type = binaries[i].type;
					break;
				}
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

MailConfigNewMailNotify
mail_config_get_new_mail_notify (void)
{
	return config->notify;
}

void
mail_config_set_new_mail_notify (MailConfigNewMailNotify type)
{
	config->notify = type;
}

const char *
mail_config_get_new_mail_notify_sound_file (void)
{
	return config->notify_filename;
}

void
mail_config_set_new_mail_notify_sound_file (const char *filename)
{
	g_free (config->notify_filename);
	config->notify_filename = g_strdup (filename);
}

gboolean
mail_config_find_account (const MailConfigAccount *account)
{
	return g_slist_find (config->accounts, (gpointer) account) != NULL;
}

const MailConfigAccount *
mail_config_get_default_account (void)
{
	MailConfigAccount *account;
	
	if (config == NULL) {
		mail_config_init ();
	}
	
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
	CamelProvider *provider;
	CamelURL *source;
	GSList *l;
	
	g_return_val_if_fail (source_url != NULL, NULL);
	
	provider = camel_session_get_provider (session, source_url, NULL);
	if (!provider)
		return NULL;
	
	source = camel_url_new (source_url, NULL);
	if (!source)
		return NULL;
	
	l = config->accounts;
	while (l) {
		account = l->data;
		
		if (account && account->source && account->source->url) {
			CamelURL *url;
			
			url = camel_url_new (account->source->url, NULL);
			if (url && provider->url_equal (url, source)) {
				camel_url_free (url);
				camel_url_free (source);
				return account;
			}
			
			if (url)
				camel_url_free (url);
		}
		
		l = l->next;
	}
	
	camel_url_free (source);
	
	return NULL;
}

const MailConfigAccount *
mail_config_get_account_by_transport_url (const char *transport_url)
{
	const MailConfigAccount *account;
	CamelProvider *provider;
	CamelURL *transport;
	GSList *l;
	
	g_return_val_if_fail (transport_url != NULL, NULL);
	
	provider = camel_session_get_provider (session, transport_url, NULL);
	if (!provider)
		return NULL;
	
	transport = camel_url_new (transport_url, NULL);
	if (!transport)
		return NULL;
	
	l = config->accounts;
	while (l) {
		account = l->data;
		
		if (account && account->transport && account->transport->url) {
			CamelURL *url;
			
			url = camel_url_new (account->transport->url, NULL);
			if (url && provider->url_equal (url, transport)) {
				camel_url_free (url);
				camel_url_free (transport);
				return account;
			}
			
			if (url)
				camel_url_free (url);
		}
		
		l = l->next;
	}
	
	camel_url_free (transport);
	
	return NULL;
}

const GSList *
mail_config_get_accounts (void)
{
	g_assert (config != NULL);
	
	return config->accounts;
}

static void
add_shortcut_entry (const char *name, const char *uri, const char *type)
{
	extern EvolutionShellClient *global_shell_client;
	CORBA_Environment ev;
	GNOME_Evolution_Shortcuts shortcuts_interface;
	GNOME_Evolution_Shortcuts_Group *the_group;
	GNOME_Evolution_Shortcuts_Shortcut *the_shortcut;
	int i;
	
	if (!global_shell_client)
		return;
	
	CORBA_exception_init (&ev);
	
	shortcuts_interface = evolution_shell_client_get_shortcuts_interface (global_shell_client);
	if (CORBA_Object_is_nil (shortcuts_interface, &ev)) {
		g_warning ("No ::Shortcut interface on the shell");
		CORBA_exception_free (&ev);
		return;
	}
	
	the_group = GNOME_Evolution_Shortcuts_getGroup (shortcuts_interface, 0, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Exception getting first group: %s", ev._repo_id);
		CORBA_exception_free (&ev);
		return;
	}

	the_shortcut = NULL;
	for (i = 0; i < the_group->shortcuts._length; i++) {
		GNOME_Evolution_Shortcuts_Shortcut *iter;
		
		iter = the_group->shortcuts._buffer + i;
		if (!strcmp (iter->name, name)) {
			the_shortcut = iter;
			break;
		}
	}
	
	if (the_shortcut == NULL) {
		the_shortcut = GNOME_Evolution_Shortcuts_Shortcut__alloc ();
		the_shortcut->name = CORBA_string_dup (name);
		the_shortcut->uri = CORBA_string_dup (uri);
		the_shortcut->type = CORBA_string_dup (type);
		
		GNOME_Evolution_Shortcuts_add (shortcuts_interface,
					       0, -1, /* "end of list" */
					       the_shortcut,
					       &ev);
		
		CORBA_free (the_shortcut);
		
		if (ev._major != CORBA_NO_EXCEPTION)
			g_warning ("Exception creating shortcut \"%s\": %s", name, ev._repo_id);
	}
	
	CORBA_free (the_group);
	CORBA_exception_free (&ev);
}

static void
add_new_storage (const char *url, const char *name)
{
	extern EvolutionShellClient *global_shell_client;
	GNOME_Evolution_Shell corba_shell;

	corba_shell = bonobo_object_corba_objref (BONOBO_OBJECT (global_shell_client));
	mail_load_storage_by_uri (corba_shell, url, name);
}

static void
new_source_created (MailConfigAccount *account)
{
	CamelProvider *prov;
	CamelFolder *inbox;
	CamelException ex;
	char *name;
	char *url;
	
	/* no source, don't bother. */
	if (!account->source || !account->source->url)
		return;

	camel_exception_init (&ex);
	prov = camel_session_get_provider (session, account->source->url, &ex);
	if (camel_exception_is_set (&ex)) {
		g_warning ("Configured provider that doesn't exist?");
		camel_exception_clear (&ex);
		return;
	}

	/* not a storage, don't bother. */
	if (!(prov->flags & CAMEL_PROVIDER_IS_STORAGE) ||
	    (prov->flags & CAMEL_PROVIDER_IS_EXTERNAL))
		return;

	inbox = mail_tool_get_inbox (account->source->url, &ex);
	if (camel_exception_is_set (&ex)) {
		e_notice (NULL, GNOME_MESSAGE_BOX_ERROR,
			  _("Could not get inbox for new mail store:\n%s\n"
			    "No shortcut will be created."),
			  camel_exception_get_description (&ex));
		camel_exception_clear (&ex);
		return;
	}

	if (inbox) {
		/* Create the shortcut. FIXME: This only works if the
		 * full name matches the path.
		 */
		name = g_strdup_printf (U_("%s: Inbox"), account->name);
		url = g_strdup_printf ("evolution:/%s/%s", account->name,
				       inbox->full_name);
		add_shortcut_entry (name, url, "mail");
		g_free (name);
		g_free (url);

		/* If we unref inbox here, it will disconnect from the
		 * store, but then add_new_storage will reconnect. So
		 * we'll keep holding the ref until after that.
		 */
	}

	/* add the storage to the folder tree */
	add_new_storage (account->source->url, account->name);

	if (inbox)
		camel_object_unref (CAMEL_OBJECT (inbox));
}

void
mail_config_add_account (MailConfigAccount *account)
{
	config->accounts = g_slist_append (config->accounts, account);
	
	if (account->source && account->source->url)
		new_source_created (account);
}

static void
remove_account_shortcuts (MailConfigAccount *account)
{
	extern EvolutionShellClient *global_shell_client;
	CORBA_Environment ev;
	GNOME_Evolution_Shortcuts shortcuts_interface;
	GNOME_Evolution_Shortcuts_GroupList *groups;
	int i, j, len;;

	CORBA_exception_init (&ev);

	shortcuts_interface = evolution_shell_client_get_shortcuts_interface (global_shell_client);
	if (CORBA_Object_is_nil (shortcuts_interface, &ev)) {
		g_warning ("No ::Shortcut interface on the shell");
		CORBA_exception_free (&ev);
		return;
	}

	groups = GNOME_Evolution_Shortcuts__get_groups (shortcuts_interface, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Exception getting the groups: %s", ev._repo_id);
		CORBA_exception_free (&ev);
		return;
	}

	len = strlen (account->name);

	for (i = 0; i < groups->_length; i++) {
		GNOME_Evolution_Shortcuts_Group *iter;
		
		iter = groups->_buffer + i;

		for (j = 0; j < iter->shortcuts._length; j++) {
			GNOME_Evolution_Shortcuts_Shortcut *sc;

			sc = iter->shortcuts._buffer + j;

			/* "evolution:/" = 11 */
			if (!strncmp (sc->uri + 11, account->name, len)) {
				GNOME_Evolution_Shortcuts_remove (shortcuts_interface, i, j, &ev);

				if (ev._major != CORBA_NO_EXCEPTION) {
					g_warning ("Exception removing shortcut \"%s\": %s", sc->name, ev._repo_id);
					break;
				}
			}
		}
	}

	CORBA_exception_free (&ev);
	CORBA_free (groups);
}

const GSList *
mail_config_remove_account (MailConfigAccount *account)
{
	int index;
	
	/* Removing the current default, so make the first account the
           default */
	if (account == mail_config_get_default_account ()) {
		config->default_account = 0;
	} else {
		/* adjust the default to make sure it points to the same one */
		index = g_slist_index (config->accounts, account);
		if (config->default_account > index)
			config->default_account--;
	}
	
	config->accounts = g_slist_remove (config->accounts, account);
	remove_account_shortcuts (account);
	account_destroy (account);
	
	return config->accounts;
}

int
mail_config_get_default_account_num (void)
{
	return config->default_account;
}

static void
mail_config_set_default_account_num (int new_default)
{
	config->default_account = new_default;
}

void
mail_config_set_default_account (const MailConfigAccount *account)
{
	int index;
	
	index = g_slist_index (config->accounts, (void *) account);
	if (index == -1)
		return;
	
	config->default_account = index;
	
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
}

char *
mail_config_folder_to_safe_url (CamelFolder *folder)
{
	CamelService *service = CAMEL_SERVICE (folder->parent_store);
	char *service_url, *url;
	
	service_url = camel_url_to_string (service->url, CAMEL_URL_HIDE_ALL);
	url = g_strdup_printf ("%s/%s", service_url, folder->full_name);
	g_free (service_url);
	
	e_filename_make_safe (url);
	return url;
}

char *
mail_config_folder_to_cachename (CamelFolder *folder, const char *prefix)
{
	char *url, *filename;
	
	url = mail_config_folder_to_safe_url (folder);
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

static char *
check_service_describe (struct _mail_msg *mm, int complete)
{
	return g_strdup (_("Checking Service"));
}

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
	check_service_describe,
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
mail_config_check_service (const char *url, CamelProviderType type, GList **authtypes, GtkWindow *window)
{
	static GtkWidget *dialog = NULL;
	gboolean ret = FALSE;
	struct _check_msg *m;
	GtkWidget *label;
	int id;
	
	if (dialog) {
		gdk_window_raise (dialog->window);
		*authtypes = NULL;
		return FALSE;
	}
	
	m = mail_msg_new (&check_service_op, NULL, sizeof(*m));
	m->url = url;
	m->type = type;
	m->authtypes = authtypes;
	m->success = &ret;
	
	id = m->msg.seq;
	e_thread_put(mail_thread_queued, (EMsg *)m);
	
	dialog = gnome_dialog_new (_("Connecting to server..."),
				   GNOME_STOCK_BUTTON_CANCEL,
				   NULL);
	gnome_dialog_set_parent (GNOME_DIALOG (dialog), window);
	label = gtk_label_new (_("Connecting to server..."));
	gtk_box_pack_start (GTK_BOX(GNOME_DIALOG (dialog)->vbox),
			    label, TRUE, TRUE, 10);
	gnome_dialog_set_close (GNOME_DIALOG (dialog), FALSE);
	gtk_signal_connect (GTK_OBJECT (dialog), "clicked",
			    GTK_SIGNAL_FUNC (check_cancelled), &id);
	gtk_signal_connect (GTK_OBJECT (dialog), "delete_event",
			    GTK_SIGNAL_FUNC (check_cancelled), &id);
	gtk_window_set_modal (GTK_WINDOW (dialog), FALSE);
	gtk_widget_show_all (dialog);
	
	mail_msg_wait(id);
	
	gtk_widget_destroy (dialog);
	dialog = NULL;
	
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
	
	if (mail_config_get_account_by_name (account->name)) {
		/* FIXME: we need an exception. */
		return;
	}
	
	mail_account = g_new0 (MailConfigAccount, 1);
	mail_account->name = g_strdup (account->name);
	
	/* Copy ID */
	id = account->id;
	mail_id = g_new0 (MailConfigIdentity, 1);
	mail_id->name = g_strdup (id.name);
	mail_id->address = g_strdup (id.address);
	mail_id->organization = g_strdup (id.organization);
	
	mail_account->id = mail_id;
	
	/* Copy source */
	source = account->source;
	mail_service = g_new0 (MailConfigService, 1);
	if (source.url == NULL || strcmp (source.url, "none://") == 0) {
		mail_service->url = NULL;
	} else {
		mail_service->url = g_strdup (source.url);
	}
	mail_service->keep_on_server = source.keep_on_server;
	mail_service->auto_check = source.auto_check;
	mail_service->auto_check_time = source.auto_check_time;
	mail_service->save_passwd = source.save_passwd;
	mail_service->enabled = source.enabled;
	
	mail_account->source = mail_service;
	
	/* Copy transport */
	transport = account->transport;
	mail_service = g_new0 (MailConfigService, 1);
	if (transport.url == NULL) {
		mail_service->url = NULL;
	} else {
		mail_service->url = g_strdup (transport.url);
	}
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

	config = gtk_type_new (evolution_mail_config_get_type ());
	return BONOBO_OBJECT (config);
}

gboolean
evolution_mail_config_factory_init (void)
{
	BonoboGenericFactory *factory;
	
	factory = bonobo_generic_factory_new (MAIL_CONFIG_IID, 
					      evolution_mail_config_factory_fn,
					      NULL);
	if (factory == NULL) {
		g_warning ("Error starting MailConfig");
		return FALSE;
	}

	bonobo_running_context_auto_exit_unref (BONOBO_OBJECT (factory));
	return TRUE;
}

GList *
mail_config_get_signature_list (void)
{
	return config->signature_list;
}

static gchar *
get_new_signature_filename ()
{
	struct stat st_buf;
	gchar *filename;
	gint i;

	filename = g_strconcat (evolution_dir, "/signatures", NULL);
	if (lstat (filename, &st_buf)) {
		if (errno == ENOENT) {
			if (mkdir (filename, 0700))
				g_warning ("Fatal problem creating %s/signatures directory.", evolution_dir);
		} else
			g_warning ("Fatal problem with %s/signatures directory.", evolution_dir);
	}
	g_free (filename);

	for (i = 0; ; i ++) {
		filename = g_strdup_printf ("%s/signatures/signature-%d", evolution_dir, i);
		if (lstat (filename, &st_buf) == - 1 && errno == ENOENT) {
			return filename;
		}
		g_free (filename);
	}

	return NULL;
}

MailConfigSignature *
mail_config_signature_add (gboolean html)
{
	MailConfigSignature *sig;

	sig = g_new0 (MailConfigSignature, 1);

	/* printf ("mail_config_signature_add %d\n", config->signatures); */
	sig->id = config->signatures;
	sig->name = g_strdup (_("Unnamed"));
	sig->filename = get_new_signature_filename ();
	sig->html = html;

	config->signature_list = g_list_append (config->signature_list, sig);
	config->signatures ++;

	config_write_signature (sig, sig->id);
	config_write_signatures_num ();

	mail_config_signature_emit_event (MAIL_CONFIG_SIG_EVENT_ADDED, sig);
	/* printf ("mail_config_signature_add end\n"); */

	return sig;
}

static void
delete_unused_signature_file (const gchar *filename)
{
	gint len;

	/* remove signature file if it's in evolution dir and no other signature uses it */
	len = strlen (evolution_dir);
	if (filename && !strncmp (filename, evolution_dir, len)) {
		GList *l;
		gboolean only_one = TRUE;

		for (l = config->signature_list; l; l = l->next) {
			if (((MailConfigSignature *)l->data)->filename
			    && !strcmp (filename, ((MailConfigSignature *)l->data)->filename)) {
				only_one = FALSE;
				break;
			}
		}

		if (only_one) {
			unlink (filename);
		}
	}
}

void
mail_config_signature_delete (MailConfigSignature *sig)
{
	GList *l, *next;
	GSList *al;
	gboolean after = FALSE;

	/* FIXME remove it from all accounts */

	for (al = config->accounts; al; al = al->next) {
		MailConfigAccount *account;

		account = (MailConfigAccount *) al->data;

		if (account->id->text_signature == sig)
			account->id->text_signature = NULL;
		if (account->id->html_signature == sig)
			account->id->html_signature = NULL;
	}

	for (l = config->signature_list; l; l = next) {
		next = l->next;
		if (after)
			((MailConfigSignature *) l->data)->id --;
		else if (l->data == sig) {
			config->signature_list = g_list_remove_link (config->signature_list, l);
			after = TRUE;
			config->signatures --;
			if (sig->random)
				config->signatures_random --;
		}
	}

	config_write_signatures ();
	delete_unused_signature_file (sig->filename);
	/* printf ("signatures: %d\n", config->signatures); */
	mail_config_signature_emit_event (MAIL_CONFIG_SIG_EVENT_DELETED, sig);
	signature_destroy (sig);
}

void
mail_config_signature_write (MailConfigSignature *sig)
{
	config_write_signature (sig, sig->id);
}

void
mail_config_signature_set_filename (MailConfigSignature *sig, const gchar *filename)
{
	gchar *old_filename = sig->filename;

	sig->filename = g_strdup (filename);
	if (old_filename) {
		delete_unused_signature_file (old_filename);
		g_free (old_filename);
	}
}

void
mail_config_signature_set_name (MailConfigSignature *sig, const gchar *name)
{
	g_free (sig->name);
	sig->name = g_strdup (name);

	mail_config_signature_emit_event (MAIL_CONFIG_SIG_EVENT_NAME_CHANGED, sig);
}

void
mail_config_signature_set_random (MailConfigSignature *sig, gboolean random)
{
	if (random != sig->random) {
		if (random && !sig->random) {
			config->signatures_random ++;
			if (config->signatures_random == 1)
				mail_config_signature_emit_event (MAIL_CONFIG_SIG_EVENT_RANDOM_ON, sig);
		} else if (!random && sig->random) {
			config->signatures_random --;
			if (config->signatures_random == 0)
				mail_config_signature_emit_event (MAIL_CONFIG_SIG_EVENT_RANDOM_OFF, sig);
		}
		sig->random = random;
	}
}

gint
mail_config_get_signatures_random (void)
{
	return config->signatures_random;
}

static GList *clients = NULL;

void
mail_config_signature_register_client (MailConfigSignatureClient client, gpointer data)
{
	clients = g_list_append (clients, client);
	clients = g_list_append (clients, data);
}

void
mail_config_signature_unregister_client (MailConfigSignatureClient client, gpointer data)
{
	GList *link;

	link = g_list_find (clients, data);
	clients = g_list_remove_link (clients, link->prev);
	clients = g_list_remove_link (clients, link);
}

void
mail_config_signature_emit_event (MailConfigSigEvent event, MailConfigSignature *sig)
{
	GList *l, *next;

	for (l = clients; l; l = next) {
		next = l->next->next;
		(*((MailConfigSignatureClient) l->data)) (event, sig, l->next->data);
	}
}

void
mail_config_signature_run_script (gchar *script)
{
	if (script) {
		gchar *argv[2];
		gint pid, status;

		printf ("running script %s\n", script);
		argv [0] = script;
		argv [1] = NULL;
		pid = gnome_execute_async (NULL, 1, argv);
		if (pid < 0)
			gnome_error_dialog (_("Cannot execute signature script"));
		else
			waitpid (pid, &status, 0);
	}
}
