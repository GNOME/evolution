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

#include <gal/util/e-util.h>
#include <e-util/e-html-utils.h>
#include <e-util/e-url.h>
#include "mail.h"
#include "mail-config.h"
#include "mail-ops.h"
#include "mail-mt.h"

#include "Mail.h"

typedef struct {
	gboolean thread_list;
	gboolean view_source;
	gboolean hide_deleted;
	gint paned_size;
	gboolean send_html;
	gboolean citation_highlight;
	guint32  citation_color;
	gboolean prompt_empty_subject;
	gint seen_timeout;
	
	GSList *accounts;
	GSList *news;
	
	char *pgp_path;
	CamelPgpType pgp_type;
} MailConfig;

static const char GCONFPATH[] = "/apps/Evolution/Mail";
static MailConfig *config = NULL;

#define MAIL_CONFIG_IID "OAFIID:GNOME_Evolution_MailConfig_Factory"

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
	new->default_account = account->default_account;
	
	new->id = identity_copy (account->id);
	new->source = service_copy (account->source);
	new->transport = service_copy (account->transport);
	
	new->drafts_folder_name = g_strdup (account->drafts_folder_name);
	new->drafts_folder_uri = g_strdup (account->drafts_folder_uri);
	new->sent_folder_name = g_strdup (account->sent_folder_name);
	new->sent_folder_uri = g_strdup (account->sent_folder_uri);

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
	gboolean def;
	
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
		
		path = g_strdup_printf ("account_drafts_folder_name_%d", i);
		account->drafts_folder_name = gnome_config_get_string (path);
		g_free (path);
		path = g_strdup_printf ("account_drafts_folder_uri_%d", i);
		account->drafts_folder_uri = gnome_config_get_string (path);
		g_free (path);
		path = g_strdup_printf ("account_sent_folder_name_%d", i);
		account->sent_folder_name = gnome_config_get_string (path);
		g_free (path);
		path = g_strdup_printf ("account_sent_folder_uri_%d", i);
		account->sent_folder_uri = gnome_config_get_string (path);
		g_free (path);

		/* get the identity info */
		id = g_new0 (MailConfigIdentity, 1);		
		path = g_strdup_printf ("identity_name_%d", i);
		id->name = gnome_config_get_string (path);
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
		path = g_strdup_printf ("source_auto_check_%d", i);
		source->auto_check = gnome_config_get_bool_with_default (path, &def);
		if (def)
			source->auto_check = FALSE;
		g_free (path);
		path = g_strdup_printf ("source_auto_check_time_%d", i);
		source->auto_check_time = gnome_config_get_int_with_default (path, &def);
		if (source->auto_check && def)
			source->auto_check = FALSE;
		g_free (path);
		path = g_strdup_printf ("source_enabled_%d", i);
		source->enabled = gnome_config_get_bool_with_default (path, &def);
		if (def)
			source->enabled = TRUE;
		g_free (path);
		path = g_strdup_printf ("source_save_passwd_%d", i);
		source->save_passwd = gnome_config_get_bool (path);
		g_free (path);
		
		/* get the transport */
		transport = g_new0 (MailConfigService, 1);
		path = g_strdup_printf ("transport_url_%d", i);
		transport->url = gnome_config_get_string (path);
		g_free (path);
		
		path = g_strdup_printf ("transport_save_passwd_%d", i);
		transport->save_passwd = gnome_config_get_bool (path);
		g_free (path);
		
		if (!*transport->url) {
			/* no transport associated with this account */
			g_free (transport->url);
			transport->url = NULL;
		}
		
		account->id = id;
		account->source = source;
		account->transport = transport;
		
		config->accounts = g_slist_append (config->accounts, account);
	}
	gnome_config_pop_prefix ();
	
#ifdef ENABLE_NNTP
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
#endif
	
	/* Format */
	str = g_strdup_printf ("=%s/config/Mail=/Format/send_html", 
			       evolution_dir);
	config->send_html = gnome_config_get_bool_with_default (str, &def);
	if (def)
		config->send_html = FALSE;
	g_free (str);
	
	/* Citation */
	str = g_strdup_printf ("=%s/config/Mail=/Display/citation_highlight", 
			       evolution_dir);
	config->citation_highlight = gnome_config_get_bool_with_default (str, &def);
	if (def)
		config->citation_highlight = TRUE;
	g_free (str);
	str = g_strdup_printf ("=%s/config/Mail=/Display/citation_color", 
			       evolution_dir);
	config->citation_color = gnome_config_get_int_with_default (str, &def);
	if (def)
		config->citation_color = 0x737373;
	g_free (str);

	/* Mark as seen timeout */
	str = g_strdup_printf ("=%s/config/Mail=/Display/seen_timeout", 
			       evolution_dir);
	config->seen_timeout = gnome_config_get_int_with_default (str, &def);
	if (def)
		config->seen_timeout = 1500;
	g_free (str);
	
	/* Show Messages Threaded */
	str = g_strdup_printf ("=%s/config/Mail=/Display/thread_list", 
			       evolution_dir);
	config->thread_list = gnome_config_get_bool_with_default (str, &def);
	if (def)
		config->thread_list = FALSE;
	g_free (str);
	
	/* Hide deleted automatically */
	str = g_strdup_printf ("=%s/config/Mail=/Display/hide_deleted", 
			       evolution_dir);
	config->hide_deleted = gnome_config_get_bool_with_default (str, &def);
	if (def)
		config->hide_deleted = FALSE;
	g_free (str);
	
	/* Size of vpaned in mail view */
	str = g_strdup_printf ("=%s/config/Mail=/Display/paned_size", 
			       evolution_dir);
	config->paned_size = gnome_config_get_int_with_default (str, &def);
	if (def)
		config->paned_size = 200;
	g_free (str);
	
	/* Empty Subject */
	str = g_strdup_printf ("=%s/config/Mail=/Prompts/empty_subject", 
			       evolution_dir);
	config->prompt_empty_subject = gnome_config_get_bool_with_default (str, &def);
	if (def)
		config->prompt_empty_subject = TRUE;
	g_free (str);
	
	/* PGP/GPG */
	str = g_strdup_printf ("=%s/config/Mail=/PGP/path", 
			       evolution_dir);
	config->pgp_path = gnome_config_get_string (str);
	g_free (str);
	str = g_strdup_printf ("=%s/config/Mail=/PGP/type", 
			       evolution_dir);
	config->pgp_type = gnome_config_get_int_with_default (str, &def);
	if (def)
		config->pgp_type = CAMEL_PGP_TYPE_NONE;
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
	gnome_config_clean_section (str);
	gnome_config_sync ();
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
		path = g_strdup_printf ("account_drafts_folder_name_%d", i);
		gnome_config_set_string (path, account->drafts_folder_name);
		g_free (path);
		path = g_strdup_printf ("account_drafts_folder_uri_%d", i);
		gnome_config_set_string (path, account->drafts_folder_uri);
		g_free (path);
		path = g_strdup_printf ("account_sent_folder_name_%d", i);
		gnome_config_set_string (path, account->sent_folder_name);
		g_free (path);
		path = g_strdup_printf ("account_sent_folder_uri_%d", i);
		gnome_config_set_string (path, account->sent_folder_uri);
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
		path = g_strdup_printf ("source_auto_check_%d", i);
		gnome_config_set_bool (path, account->source->auto_check);
		g_free (path);
		path = g_strdup_printf ("source_auto_check_time_%d", i);
		gnome_config_set_int (path, account->source->auto_check_time);
		g_free (path);
		path = g_strdup_printf ("source_enabled_%d", i);
		gnome_config_set_bool (path, account->source->enabled);
		g_free (path);
		path = g_strdup_printf ("source_save_passwd_%d", i);
		gnome_config_set_bool (path, account->source->save_passwd);
		g_free (path);
		
		/* transport info */
		path = g_strdup_printf ("transport_url_%d", i);
		gnome_config_set_string (path, account->transport->url ? account->transport->url : "");
		g_free (path);
		
		path = g_strdup_printf ("transport_save_passwd_%d", i);
		gnome_config_set_bool (path, account->transport->save_passwd);
		g_free (path);
	}
	gnome_config_pop_prefix ();
	
#ifdef ENABLE_NNTP
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
#endif
	
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

	/* Hide deleted automatically */
	str = g_strdup_printf ("=%s/config/Mail=/Display/hide_deleted", 
			       evolution_dir);
	gnome_config_set_bool (str, config->hide_deleted);
	g_free (str);
	
	/* Size of vpaned in mail view */
	str = g_strdup_printf ("=%s/config/Mail=/Display/paned_size", 
			       evolution_dir);
	gnome_config_set_int (str, config->paned_size);
	g_free (str);
	
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
	
	/* Citation */
	str = g_strdup_printf ("=%s/config/Mail=/Display/citation_highlight",
			       evolution_dir);
	gnome_config_set_bool (str, config->citation_highlight);
	g_free (str);
	str = g_strdup_printf ("=%s/config/Mail=/Display/citation_color", 
			       evolution_dir);
	gnome_config_set_int (str, config->citation_color);
	g_free (str);

	/* Empty Subject */
	str = g_strdup_printf ("=%s/config/Mail=/Prompts/empty_subject", 
			       evolution_dir);
	gnome_config_set_bool (str, config->prompt_empty_subject);
	g_free (str);
	
	/* PGP/GPG */
	str = g_strdup_printf ("=%s/config/Mail=/PGP/path", 
			       evolution_dir);
	gnome_config_set_string (str, config->pgp_path);
	g_free (str);
	str = g_strdup_printf ("=%s/config/Mail=/PGP/type", 
			       evolution_dir);
	gnome_config_set_int (str, config->pgp_type);
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
	
	url = camel_url_to_string (CAMEL_SERVICE (folder->parent_store)->url, CAMEL_URL_HIDE_PASSWORD | CAMEL_URL_HIDE_PARAMS);
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

static void check_service_check(struct _mail_msg *mm)
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
	mail_account->default_account = account->default_account;

	/* Copy ID */
	id = account->id;
	mail_id = g_new0 (MailConfigIdentity, 1);
	mail_id->name = g_strdup (id.name);
	mail_id->address = g_strdup (id.address);
	mail_id->organization = g_strdup (id.organization);
	mail_id->signature = g_strdup (id.signature);

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
evolution_mail_config_factory_fn (BonoboObject *factory,
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
	BonoboObject *factory;

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
