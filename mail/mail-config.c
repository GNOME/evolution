/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@helixcode.com>
 *
 *  Copyright 2001 Helix Code, Inc. (www.helixcode.com)
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

#include <config.h>
#include <pwd.h>
#include <ctype.h>

#include <gnome.h>
#include <gtkhtml/gtkhtml.h>
#include <glade/glade.h>

#include <gal/util/e-util.h>
#include "e-util/e-html-utils.h"
#include "mail.h"
#include "mail-config.h"
#include "mail-ops.h"

typedef struct {
	gboolean thread_list;
	gboolean view_source;
	gint paned_size;
	gboolean send_html;
	gint seen_timeout;
	
	GSList *accounts;
	GSList *news;
} MailConfig;

static const char GCONFPATH[] = "/apps/Evolution/Mail";
static MailConfig *config = NULL;

/* Prototypes */
static void config_read (void);

/* Identity */
MailConfigIdentity *
identity_copy (const MailConfigIdentity *id)
{
	MailConfigIdentity *new;
	
	g_return_val_if_fail (id != NULL, NULL);
	
	new = g_new0 (MailConfigIdentity, 1);
	new->name = g_strdup (id->name);
	new->address = g_strdup (id->address);
	new->reply_to = g_strdup (id->reply_to);
	new->organization = g_strdup (id->organization);
	new->signature = g_strdup (id->signature);
	
	return new;
}

void
identity_destroy (MailConfigIdentity *id)
{
	if (!id)
		return;
	
	g_free (id->name);
	g_free (id->address);
	g_free (id->reply_to);
	g_free (id->organization);
	g_free (id->signature);
	
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
	new->save_passwd = source->save_passwd;
	new->use_ssl = source->use_ssl;
	
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
	new->default_account = account->default_account;
	
	new->id = identity_copy (account->id);
	new->source = service_copy (account->source);
	new->transport = service_copy (account->transport);
	
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
	if (config)
		return;
	
	config = g_new0 (MailConfig, 1);
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
	
	/* overkill? */
	memset (config, 0, sizeof (MailConfig));
}

static void
config_read (void)
{
	gchar *str;
	gint len, i;
	gboolean have_default = FALSE;
	
	mail_config_clear ();
	
	/* Accounts */
	str = g_strdup_printf ("=%s/config/Mail=/Accounts/", evolution_dir);
	gnome_config_push_prefix (str);
	g_free (str);
	
	len = gnome_config_get_int ("num");
	for (i = 0; i < len; i++) {
		MailConfigAccount *account;
		MailConfigIdentity *id;
		MailConfigService *source;
		MailConfigService *transport;
		gchar *path;
		
		account = g_new0 (MailConfigAccount, 1);
		path = g_strdup_printf ("account_name_%d", i);
		account->name = gnome_config_get_string (path);
		g_free (path);
		path = g_strdup_printf ("account_is_default_%d", i);
		account->default_account = gnome_config_get_bool (path) && !have_default;
		if (account->default_account)
			have_default = TRUE;
		g_free (path);
		
		/* get the identity info */
		id = g_new0 (MailConfigIdentity, 1);		
		path = g_strdup_printf ("identity_name_%d", i);
		id->name = gnome_config_get_string (path);
		g_free (path);
		path = g_strdup_printf ("identity_replyto_%d", i);
		id->reply_to = gnome_config_get_string (path);
		g_free (path);
		path = g_strdup_printf ("identity_address_%d", i);
		id->address = gnome_config_get_string (path);
		g_free (path);
		path = g_strdup_printf ("identity_organization_%d", i);
		id->organization = gnome_config_get_string (path);
		g_free (path);
		path = g_strdup_printf ("identity_signature_%d", i);
		id->signature = gnome_config_get_string (path);
		g_free (path);
		
		/* get the source */
		source = g_new0 (MailConfigService, 1);
		path = g_strdup_printf ("source_url_%d", i);
		source->url = gnome_config_get_string (path);
		g_free (path);
		
		if (!*source->url) {
			/* no source associated with this account */
			g_free (source->url);
			source->url = NULL;
		}
		
		path = g_strdup_printf ("source_keep_on_server_%d", i);
		source->keep_on_server = gnome_config_get_bool (path);
		g_free (path);
		path = g_strdup_printf ("source_save_passwd_%d", i);
		source->save_passwd = gnome_config_get_bool (path);
		g_free (path);
		path = g_strdup_printf ("source_use_ssl_%d", i);
		source->use_ssl = gnome_config_get_bool (path);
		g_free (path);
		
		/* get the transport */
		transport = g_new0 (MailConfigService, 1);
		path = g_strdup_printf ("transport_url_%d", i);
		transport->url = gnome_config_get_string (path);
		g_free (path);
		
		if (!*transport->url) {
			/* no transport associated with this account */
			g_free (transport->url);
			transport->url = NULL;
		}
		
		path = g_strdup_printf ("transport_use_ssl_%d", i);
		transport->use_ssl = gnome_config_get_bool (path);
		g_free (path);
		
		account->id = id;
		account->source = source;
		account->transport = transport;
		
		config->accounts = g_slist_append (config->accounts, account);
	}
	gnome_config_pop_prefix ();
	
	/* News */
	str = g_strdup_printf ("=%s/config/News=/Sources/", evolution_dir);
	gnome_config_push_prefix (str);
	g_free (str);
	
	len = gnome_config_get_int ("num");
	for (i = 0; i < len; i++) {
		MailConfigService *n;
		gchar *path;
		
		n = g_new0 (MailConfigService, 1);
		
		path = g_strdup_printf ("url_%d", i);
		n->url = gnome_config_get_string (path);
		g_free (path);
		
		config->news = g_slist_append (config->news, n);
	}
	gnome_config_pop_prefix ();
	
	/* Format */
	str = g_strdup_printf ("=%s/config/Mail=/Format/send_html", 
			       evolution_dir);
	config->send_html = gnome_config_get_bool (str);
	g_free (str);
	
	/* Mark as seen timeout */
	str = g_strdup_printf ("=%s/config/Mail=/Display/seen_timeout=1500", 
			       evolution_dir);
	config->seen_timeout = gnome_config_get_int (str);
	g_free (str);
	
	/* Show Messages Threaded */
	str = g_strdup_printf ("=%s/config/Mail=/Display/thread_list", 
			       evolution_dir);
	config->thread_list = gnome_config_get_bool (str);
	g_free (str);
	
	/* Size of vpaned in mail view */
	str = g_strdup_printf ("=%s/config/Mail=/Display/paned_size=200", 
			       evolution_dir);
	config->paned_size = gnome_config_get_int (str);
	g_free (str);
	
	gnome_config_sync ();
}

void
mail_config_write (void)
{
	gchar *str;
	gint len, i;
	
	/* Accounts */
	str = g_strdup_printf ("=%s/config/Mail=/Accounts/", evolution_dir);
	gnome_config_push_prefix (str);
	g_free (str);
	
	len = g_slist_length (config->accounts);
	gnome_config_set_int ("num", len);
	for (i = 0; i < len; i++) {
		MailConfigAccount *account;
		gchar *path;
		
		account = g_slist_nth_data (config->accounts, i);
		
		/* account info */
		path = g_strdup_printf ("account_name_%d", i);
		gnome_config_set_string (path, account->name);
		g_free (path);
		path = g_strdup_printf ("account_is_default_%d", i);
		gnome_config_set_bool (path, account->default_account);
		g_free (path);
		
		/* identity info */
		path = g_strdup_printf ("identity_name_%d", i);
		gnome_config_set_string (path, account->id->name);
		g_free (path);
		path = g_strdup_printf ("identity_address_%d", i);
		gnome_config_set_string (path, account->id->address);
		g_free (path);
		path = g_strdup_printf ("identity_organization_%d", i);
		gnome_config_set_string (path, account->id->organization);
		g_free (path);
		path = g_strdup_printf ("identity_signature_%d", i);
		gnome_config_set_string (path, account->id->signature);
		g_free (path);
		
		/* source info */
		path = g_strdup_printf ("source_url_%d", i);
		gnome_config_set_string (path, account->source->url ? account->source->url : "");
		g_free (path);
		path = g_strdup_printf ("source_keep_on_server_%d", i);
		gnome_config_set_bool (path, account->source->keep_on_server);
		g_free (path);
		path = g_strdup_printf ("source_save_passwd_%d", i);
		gnome_config_set_bool (path, account->source->save_passwd);
		g_free (path);
		
		/* transport info */
		path = g_strdup_printf ("transport_url_%d", i);
		gnome_config_set_string (path, account->transport->url ? account->transport->url : "");
		g_free (path);
	}
	gnome_config_pop_prefix ();
	
	/* News */
	str = g_strdup_printf ("=%s/config/News=/Sources/", evolution_dir);
	gnome_config_push_prefix (str);
	g_free (str);
	
  	len = g_slist_length (config->news);
	gnome_config_set_int ("num", len);
	for (i = 0; i < len; i++) {
		MailConfigService *n;
		gchar *path;
		
		n = g_slist_nth_data (config->news, i);
		
		path = g_strdup_printf ("url_%d", i);
		gnome_config_set_string (path, n->url);
		g_free (path);
	}
	gnome_config_pop_prefix ();
	
	/* Mark as seen timeout */
	str = g_strdup_printf ("=%s/config/Mail=/Display/seen_timeout", 
			       evolution_dir);
	gnome_config_set_int (str, config->seen_timeout);
	g_free (str);
	
	/* Format */
	str = g_strdup_printf ("=%s/config/Mail=/Format/send_html", 
			       evolution_dir);
	gnome_config_set_bool (str, config->send_html);
	g_free (str);
	
	gnome_config_sync ();
}

void
mail_config_write_on_exit (void)
{
	gchar *str;
	GSList *sources;
	MailConfigService *s;
	
	/* Show Messages Threaded */
	str = g_strdup_printf ("=%s/config/Mail=/Display/thread_list", 
			       evolution_dir);
	gnome_config_set_bool (str, config->thread_list);
	g_free (str);
	
	/* Size of vpaned in mail view */
	str = g_strdup_printf ("=%s/config/Mail=/Display/paned_size", 
			       evolution_dir);
	gnome_config_set_int (str, config->paned_size);
	g_free (str);
	
	/* Passwords */
	gnome_config_private_clean_section ("/Evolution/Passwords");
	sources = mail_config_get_sources ();
	for ( ; sources; sources = sources->next) {
		s = sources->data;
		if (s->save_passwd)
			mail_session_remember_password (s->url);
	}
	g_slist_free (sources);
	
	gnome_config_sync ();
}

/* Accessor functions */
gboolean
mail_config_is_configured (void)
{
	return config->accounts != NULL;
}

gboolean
mail_config_get_thread_list (void)
{
	return config->thread_list;
}

void
mail_config_set_thread_list (gboolean value)
{
	config->thread_list = value;
}

gboolean
mail_config_get_view_source (void)
{
	return config->view_source;
}

void
mail_config_set_view_source (gboolean value)
{
	config->view_source = value;
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

const MailConfigAccount *
mail_config_get_default_account (void)
{
	const MailConfigAccount *account;
	GSList *l;
	
	if (!config->accounts)
		return NULL;
	
	/* find the default account */
	l = config->accounts;
	while (l) {
		account = l->data;
		if (account->default_account)
			return account;
		
		l = l->next;
	}
	
	/* none are marked as default so mark the first one as the default */
	account = config->accounts->data;
	mail_config_set_default_account (account);
	
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

const GSList *
mail_config_get_accounts (void)
{
	return config->accounts;
}

void
mail_config_add_account (MailConfigAccount *account)
{
	if (account->default_account) {
		/* Un-defaultify other accounts */
		GSList *node = config->accounts;
		
		while (node) {
			MailConfigAccount *acnt = node->data;
			
			acnt->default_account = FALSE;
			
			node = node->next;
		}
	}
	
	config->accounts = g_slist_append (config->accounts, account);
}

const GSList *
mail_config_remove_account (MailConfigAccount *account)
{
	config->accounts = g_slist_remove (config->accounts, account);
	account_destroy (account);
	
	return config->accounts;
}

void
mail_config_set_default_account (const MailConfigAccount *account)
{
	GSList *node = config->accounts;
	
	while (node) {
		MailConfigAccount *acnt = node->data;
		
		acnt->default_account = FALSE;
		
		node = node->next;
	}
	
	((MailConfigAccount *) account)->default_account = TRUE;
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

char *
mail_config_folder_to_cachename (CamelFolder *folder, const char *prefix)
{
	char *url, *filename;
	
	url = camel_url_to_string (CAMEL_SERVICE (folder->parent_store)->url, FALSE);
	e_filename_make_safe (url);
	
	filename = g_strdup_printf ("%s/config/%s%s", evolution_dir, prefix, url);
	g_free (url);
	
	return filename;
}


/* Async service-checking/authtype-lookup code. */

typedef struct {
	char *url;
	CamelProviderType type;
	GList **authtypes;
	gboolean success;
} check_service_input_t;

static char *
describe_check_service (gpointer in_data, gboolean gerund)
{
	if (gerund)
		return g_strdup (_("Connecting to server"));
	else
		return g_strdup (_("Connect to server"));
}

static void
do_check_service (gpointer in_data, gpointer op_data, CamelException *ex)
{
	check_service_input_t *input = in_data;
	CamelService *service;
	
	if (input->authtypes) {
		service = camel_session_get_service (
			session, input->url, input->type, ex);
		if (!service)
			return;
		*input->authtypes = camel_service_query_auth_types (service, ex);
	} else {
		service = camel_session_get_service_connected (
			session, input->url, input->type, ex);
	}
	if (service)
		camel_object_unref (CAMEL_OBJECT (service));
	if (!camel_exception_is_set (ex))
		input->success = TRUE;
}

static const mail_operation_spec op_check_service = {
	describe_check_service,
	0,
	NULL,
	do_check_service,
	NULL
};

gboolean
mail_config_check_service (CamelURL *url, CamelProviderType type, GList **authtypes)
{
	check_service_input_t input;
	
	input.url = camel_url_to_string (url, TRUE);
	input.type = type;
	input.authtypes = authtypes;
	input.success = FALSE;
	
	mail_operation_queue (&op_check_service, &input, FALSE);
	mail_operation_wait_for_finish ();
	g_free (input.url);
	
	return input.success;
}
