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

#include <pwd.h>
#include <ctype.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <string.h>

#include <glib.h>
#include <gtk/gtkdialog.h>
#include <gtkhtml/gtkhtml.h>
#include <glade/glade.h>

#include <gconf/gconf.h>
#include <gconf/gconf-client.h>

#include <libxml/tree.h>
#include <libxml/parser.h>

#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-context.h>
#include <bonobo/bonobo-moniker-util.h>
#include <bonobo/bonobo-exception.h>

#include <shell/evolution-shell-client.h>

#include <gal/util/e-util.h>
#include <gal/widgets/e-gui-utils.h>
#include <e-util/e-url.h>
#include <e-util/e-passwords.h>
#include "mail.h"
#include "mail-config.h"
#include "mail-mt.h"
#include "mail-tools.h"

#include "Mailer.h"


MailConfigLabel label_defaults[5] = {
	{ N_("Important"), 0x00ff0000, NULL },  /* red */
	{ N_("Work"),      0x00ff8c00, NULL },  /* orange */
	{ N_("Personal"),  0x00008b00, NULL },  /* forest green */
	{ N_("To Do"),     0x000000ff, NULL },  /* blue */
	{ N_("Later"),     0x008b008b, NULL }   /* magenta */
};

typedef struct {
	GConfClient *gconf;
	
	gboolean corrupt;
	
	GSList *accounts;
	
	GHashTable *threaded_hash;
	GHashTable *preview_hash;
	
	GList *signature_list;
	int signatures;
	
	MailConfigLabel labels[5];
	
	gboolean signature_info;
	
	/* readonly fields from calendar */
	int time_24hour;
} MailConfig;

static MailConfig *config = NULL;
static guint config_write_timeout = 0;

#define MAIL_CONFIG_IID "OAFIID:GNOME_Evolution_MailConfig_Factory"

/* Prototypes */
static void config_read (void);

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
	new->reply_to = g_strdup (id->reply_to);
	new->organization = g_strdup (id->organization);
	new->def_signature = id->def_signature;
	new->auto_signature = id->auto_signature;
	
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
	service_destroy ((MailConfigService *) item);
}

/* Account */
MailConfigAccount *
account_copy (const MailConfigAccount *account) 
{
	MailConfigAccount *new;
	
	g_return_val_if_fail (account != NULL, NULL);
	
	new = g_new0 (MailConfigAccount, 1);
	new->name = g_strdup (account->name);
	
	new->enabled = account->enabled;
	
	new->id = identity_copy (account->id);
	new->source = service_copy (account->source);
	new->transport = service_copy (account->transport);
	
	new->drafts_folder_uri = g_strdup (account->drafts_folder_uri);
	new->sent_folder_uri = g_strdup (account->sent_folder_uri);
	
	new->always_cc = account->always_cc;
	new->cc_addrs = g_strdup (account->cc_addrs);
	new->always_bcc = account->always_bcc;
	new->bcc_addrs = g_strdup (account->bcc_addrs);
	
	new->pgp_key = g_strdup (account->pgp_key);
	new->pgp_encrypt_to_self = account->pgp_encrypt_to_self;
	new->pgp_always_sign = account->pgp_always_sign;
	new->pgp_no_imip_sign = account->pgp_no_imip_sign;
	new->pgp_always_trust = account->pgp_always_trust;
	
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
	
	g_free (account->drafts_folder_uri);
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
	account_destroy ((MailConfigAccount *) item);
}


static gboolean
xml_get_bool (xmlNodePtr node, const char *name)
{
	gboolean bool = FALSE;
	char *buf;
	
	if ((buf = xmlGetProp (node, name))) {
		bool = (!strcmp (buf, "true") || !strcmp (buf, "yes"));
		xmlFree (buf);
	}
	
	return bool;
}

static int
xml_get_int (xmlNodePtr node, const char *name)
{
	int number = 0;
	char *buf;
	
	if ((buf = xmlGetProp (node, name))) {
		number = strtol (buf, NULL, 10);
		xmlFree (buf);
	}
	
	return number;
}

static char *
xml_get_prop (xmlNodePtr node, const char *name)
{
	char *buf, *val;
	
	buf = xmlGetProp (node, name);
	val = g_strdup (buf);
	g_free (buf);
	
	return val;
}

static char *
xml_get_content (xmlNodePtr node)
{
	char *buf, *val;
	
	buf = xmlNodeGetContent (node);
        val = g_strdup (buf);
	xmlFree (buf);
	
	return val;
}

static MailConfigSignature *
lookup_signature (int i)
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

static MailConfigAccount *
account_new_from_xml (char *in)
{
	MailConfigAccount *account;
	xmlNodePtr node, cur;
	xmlDocPtr doc;
	char *buf;
	
	if (!(doc = xmlParseDoc (in)))
		return NULL;
	
	node = doc->children;
	if (strcmp (node->name, "account") != 0) {
		xmlFreeDoc (doc);
		return NULL;
	}
	
	account = g_new0 (MailConfigAccount, 1);
	account->name = xml_get_prop (node, "name");
	account->enabled = xml_get_bool (node, "enabled");
	
	node = node->children;
	while (node != NULL) {
		if (!strcmp (node->name, "identity")) {
			account->id = g_new0 (MailConfigIdentity, 1);
			
			cur = node->children;
			while (cur != NULL) {
				if (!strcmp (cur->name, "name")) {
					account->id->name = xml_get_content (cur);
				} else if (!strcmp (cur->name, "addr-spec")) {
					account->id->address = xml_get_content (cur);
				} else if (!strcmp (cur->name, "reply-to")) {
					account->id->reply_to = xml_get_content (cur);
				} else if (!strcmp (cur->name, "organization")) {
					account->id->organization = xml_get_content (cur);
				} else if (!strcmp (cur->name, "signature")) {
					account->id->auto_signature = xml_get_bool (cur, "auto");
					account->id->def_signature = lookup_signature (xml_get_int (cur, "default"));
				}
				
				cur = cur->next;
			}
		} else if (!strcmp (node->name, "source")) {
			account->source = g_new0 (MailConfigService, 1);
			account->source->save_passwd = xml_get_bool (node, "save-passwd");
			account->source->keep_on_server = xml_get_bool (node, "keep-on-server");
			account->source->auto_check = xml_get_bool (node, "auto-check");
			
			/* FIXME: account->source->auto_check_time */
			
			cur = node->children;
			while (cur != NULL) {
				if (!strcmp (cur->name, "url")) {
					account->source->url = xml_get_content (cur);
					break;
				}
				cur = cur->next;
			}
		} else if (!strcmp (node->name, "transport")) {
			account->transport = g_new0 (MailConfigService, 1);
			account->transport->save_passwd = xml_get_bool (node, "save-passwd");
			
			cur = node->children;
			while (cur != NULL) {
				if (!strcmp (cur->name, "url")) {
					account->transport->url = xml_get_content (cur);
					break;
				}
				cur = cur->next;
			}
		} else if (!strcmp (node->name, "drafts-folder")) {
			account->drafts_folder_uri = xml_get_content (node);
		} else if (!strcmp (node->name, "sent-folder")) {
			account->sent_folder_uri = xml_get_content (node);
		} else if (!strcmp (node->name, "auto-cc")) {
			account->always_cc = xml_get_bool (node, "always");
			account->cc_addrs = xml_get_content (node);
		} else if (!strcmp (node->name, "auto-bcc")) {
			account->always_cc = xml_get_bool (node, "always");
			account->bcc_addrs = xml_get_content (node);
		} else if (!strcmp (node->name, "pgp")) {
			account->pgp_encrypt_to_self = xml_get_bool (node, "encrypt-to-self");
			account->pgp_always_trust = xml_get_bool (node, "always-trust");
			account->pgp_always_sign = xml_get_bool (node, "always-sign");
			account->pgp_no_imip_sign = !xml_get_bool (node, "sign-imip");
			
			if (node->children) {
				cur = node->children;
				while (cur != NULL) {
					if (!strcmp (cur->name, "key-id")) {
						account->pgp_key = xml_get_content (cur);
						break;
					}
					
					cur = cur->next;
				}
			}
		} else if (!strcmp (node->name, "smime")) {
			account->smime_encrypt_to_self = xml_get_bool (node, "encrypt-to-self");
			account->smime_always_sign = xml_get_bool (node, "always-sign");
			
			if (node->children) {
				cur = node->children;
				while (cur != NULL) {
					if (!strcmp (cur->name, "key-id")) {
						account->smime_key = xml_get_content (cur);
						break;
					}
					
					cur = cur->next;
				}
			}
		}
		
		node = node->next;
	}
	
	xmlFreeDoc (doc);
	
	return account;
}

static char *
account_to_xml (MailConfigAccount *account)
{
	xmlNodePtr root, node, id, src, xport;
	char *xmlbuf, *tmp, buf[20];
	xmlDocPtr doc;
	int n;
	
	doc = xmlNewDoc ("1.0");
	
	root = xmlNewDocNode (doc, NULL, "account", NULL);
	xmlDocSetRootElement (doc, root);
	
	xmlSetProp (root, "name", account->name);
	xmlSetProp (root, "enabled", account->enabled ? "true" : "false");
	
	id = xmlNewChild (root, NULL, "identity", NULL);
	if (account->id->name)
		xmlNewTextChild (id, NULL, "name", account->id->name);
	if (account->id->address)
		xmlNewTextChild (id, NULL, "addr-spec", account->id->address);
	if (account->id->reply_to)
		xmlNewTextChild (id, NULL, "reply-to", account->id->reply_to);
	if (account->id->organization)
		xmlNewTextChild (id, NULL, "organization", account->id->organization);
	
	node = xmlNewChild (id, NULL, "signature", NULL);
	xmlSetProp (node, "auto", account->id->auto_signature ? "true" : "false");
	sprintf (buf, "%d", account->id->def_signature);
	xmlSetProp (node, "default", buf);
	
	src = xmlNewChild (root, NULL, "source", NULL);
	xmlSetProp (src, "save-passwd", account->source->save_passwd ? "true" : "false");
	xmlSetProp (src, "keep-on-server", account->source->keep_on_server ? "true" : "false");
	xmlSetProp (src, "auto-check", account->source->auto_check ? "true" : "false");
	if (account->source->url)
		xmlNewTextChild (src, NULL, "url", account->source->url);
	
	/* FIXME: save auto-check timeout value */
	
	xport = xmlNewChild (root, NULL, "transport", NULL);
	xmlSetProp (xport, "save-passwd", account->transport->save_passwd ? "true" : "false");
	if (account->transport->url)
		xmlNewTextChild (xport, NULL, "url", account->transport->url);
	
	xmlNewTextChild (root, NULL, "drafts-folder", account->drafts_folder_uri);
	xmlNewTextChild (root, NULL, "sent-folder", account->sent_folder_uri);
	
	node = xmlNewChild (root, NULL, "auto-cc", NULL);
	xmlSetProp (node, "always", account->always_cc ? "true" : "false");
	if (account->cc_addrs)
		xmlNewTextChild (node, NULL, "recipients", account->cc_addrs);
	
	node = xmlNewChild (root, NULL, "auto-bcc", NULL);
	xmlSetProp (node, "always", account->always_bcc ? "true" : "false");
	if (account->bcc_addrs)
		xmlNewTextChild (node, NULL, "recipients", account->bcc_addrs);
	
	node = xmlNewChild (root, NULL, "pgp", NULL);
	xmlSetProp (node, "encrypt-to-self", account->pgp_encrypt_to_self ? "true" : "false");
	xmlSetProp (node, "always-trust", account->pgp_always_trust ? "true" : "false");
	xmlSetProp (node, "always-sign", account->pgp_always_sign ? "true" : "false");
	xmlSetProp (node, "sign-imip", !account->pgp_no_imip_sign ? "true" : "false");
	if (account->pgp_key)
		xmlNewTextChild (node, NULL, "key-id", account->pgp_key);
	
	node = xmlNewChild (root, NULL, "smime", NULL);
	xmlSetProp (node, "encrypt-to-self", account->smime_encrypt_to_self ? "true" : "false");
	xmlSetProp (node, "always-sign", account->smime_always_sign ? "true" : "false");
	if (account->smime_key)
		xmlNewTextChild (node, NULL, "key-id", account->smime_key);
	
	xmlDocDumpMemory (doc, &xmlbuf, &n);
	xmlFreeDoc (doc);
	
	if (!(tmp = realloc (xmlbuf, n + 1))) {
		g_free (xmlbuf);
		return NULL;
	}
	
	xmlbuf[n] = '\0';
	
	return xmlbuf;
}

static void
accounts_changed (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
	GSList *list, *l, *tail, *n;
	
	if (config->accounts != NULL) {
		l = config->accounts;
		while (l != NULL) {
			n = l->next;
			account_destroy ((MailConfigAccount *) l->data);
			g_slist_free_1 (l);
			l = n;
		}
		
		config->accounts = NULL;
	}
	
	tail = (GSList *) &config->accounts;
	
	list = gconf_client_get_list (config->gconf, "/apps/evolution/mail/accounts",
				      GCONF_VALUE_STRING, NULL);
	
	l = list;
	while (l != NULL) {
		MailConfigAccount *account;
		
		if ((account = account_new_from_xml ((char *) l->data))) {
			n = g_slist_alloc ();
			n->data = account;
			n->next = NULL;
			
			tail->next = n;
			tail = n;
		}
		
		n = l->next;
		g_slist_free_1 (l);
		l = n;
	}
}

static void
accounts_save (void)
{
	GSList *list, *tail, *n, *l;
	char *xmlbuf;
	
	list = NULL;
	tail = (GSList *) &list;
	
	l = config->accounts;
	while (l != NULL) {
		if ((xmlbuf = account_to_xml ((MailConfigAccount *) l->data))) {
			n = g_slist_alloc ();
			n->data = xmlbuf;
			n->next = NULL;
			
			tail->next = n;
			tail = n;
		}
		
		l = l->next;
	}
	
	gconf_client_set_list (config->gconf, "/apps/evolution/mail/accounts", GCONF_VALUE_STRING, list, NULL);
	
	l = list;
	while (l != NULL) {
		n = l->next;
		g_free (l->data);
		g_slist_free_1 (l);
		l = n;
	}
	
	gconf_client_suggest_sync (config->gconf, NULL);
}

/* Config struct routines */
void
mail_config_init (void)
{
	if (config)
		return;
	
	config = g_new0 (MailConfig, 1);
	config->gconf = gconf_client_get_default ();
	
	gconf_client_add_dir (config->gconf, "/apps/evolution/mail/accounts",
			      GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
	
	gconf_client_notify_add (config->gconf, "/apps/evolution/mail/accounts",
				 accounts_changed, NULL, NULL, NULL);
	
	config_read ();
}

void
mail_config_clear (void)
{
	GSList *list, *l, *n;
	int i;
	
	if (!config)
		return;
	
	l = config->accounts;
	while (l != NULL) {
		n = l->next;
		account_destroy ((MailConfigAccount *) l->data);
		g_slist_free_1 (l);
		l = n;
	}
	
	config->accounts = NULL;
	
	for (i = 0; i < 5; i++) {
		g_free (config->labels[i].name);
		config->labels[i].name = NULL;
		g_free (config->labels[i].string);
		config->labels[i].string = NULL;
	}
}

static MailConfigSignature *
config_read_signature (int i)
{
	MailConfigSignature *sig;
	char *path, *val;
	
	sig = g_new0 (MailConfigSignature, 1);
	
	sig->id = i;

#warning "need to rewrite the config_read_signature()"
#if 0
	path = g_strdup_printf ("/apps/Evolution/Mail/Signatures/name_%d", i);
	val = e_config_listener_get_string (config->db, path);
	g_free (path);
	if (val && *val)
		sig->name = val;
	else
		g_free (val);
	
	path = g_strdup_printf ("/apps/Evolution/Mail/Signatures/filename_%d", i);
	val = e_config_listener_get_string (config->db, path);
	g_free (path);
	if (val && *val)
		sig->filename = val;
	else
		g_free (val);
	
	path = g_strdup_printf ("/apps/Evolution/Mail/Signatures/script_%d", i);
	val = e_config_listener_get_string (config->db, path);
	g_free (path);
	if (val && *val)
		sig->script = val;
	else
		g_free (val);
	
	path = g_strdup_printf ("/apps/Evolution/Mail/Signatures/html_%d", i);
	sig->html = e_config_listener_get_boolean_with_default (config->db, path, FALSE, NULL);
	g_free (path);
#endif
	
	return sig;
}

static void
config_read_signatures ()
{
	MailConfigSignature *sig;
	int i;
	
	config->signature_list = NULL;
	config->signatures = 0;
	
#warning "need to rewrite config_read_signatures()"
#if 0
	config->signatures = e_config_listener_get_long_with_default (config->db, "/apps/Evolution/Mail/Signatures/num", 0, NULL);
	
	for (i = 0; i < config->signatures; i ++) {
		sig = config_read_signature (i);
		config->signature_list = g_list_append (config->signature_list, sig);
	}
#endif
}

static void
config_write_signature (MailConfigSignature *sig, gint i)
{
#warning "need to rewrite config_write_signature()"
#if 0
	char *path;
	
	printf ("config_write_signature i: %d id: %d\n", i, sig->id);
	
	path = g_strdup_printf ("/apps/Evolution/Mail/Signatures/name_%d", i);
	e_config_listener_set_string (config->db, path, sig->name ? sig->name : "");
	g_free (path);
	
	path = g_strdup_printf ("/apps/Evolution/Mail/Signatures/filename_%d", i);
	e_config_listener_set_string (config->db, path, sig->filename ? sig->filename : "");
	g_free (path);
	
	path = g_strdup_printf ("/apps/Evolution/Mail/Signatures/script_%d", i);
	e_config_listener_set_string (config->db, path, sig->script ? sig->script : "");
	g_free (path);
	
	path = g_strdup_printf ("/apps/Evolution/Mail/Signatures/html_%d", i);
	e_config_listener_set_boolean (config->db, path, sig->html);
	g_free (path);
#endif
}

static void
config_write_signatures_num ()
{
#warning "need to rewrite config_write_signatures_num()"
	/*e_config_listener_set_long (config->db, "/apps/Evolution/Mail/Signatures/num", config->signatures);*/
}

static void
config_write_signatures ()
{
	GList *l;
	int id;
	
	for (id = 0, l = config->signature_list; l; l = l->next, id ++) {
		config_write_signature ((MailConfigSignature *) l->data, id);
	}
	
	config_write_signatures_num ();
}

void
mail_config_write_account_sig (MailConfigAccount *account, int id)
{
	/* FIXME: what is this supposed to do? */
	;
}

static void
config_read (void)
{
	int len, i, default_num;
	char *path, *val, *p;
	
	mail_config_clear ();
	
	config_read_signatures ();
	
	accounts_changed (config->gconf, 0, NULL, NULL);
}

void
mail_config_write (void)
{
	if (!config)
		return;
	
	config_write_signatures ();
	
	gconf_client_suggest_sync (config->gconf, NULL);
}

static gboolean
hash_save_state (gpointer key, gpointer value, gpointer user_data)
{
	char *path;
	gboolean bool = GPOINTER_TO_INT (value);
	
#warning "need to rewrite hash_save_state(), probably shouldn't use gconf tho"
#if 0
	path = g_strconcat ("/apps/Evolution/Mail/", (char *)user_data, "/", (char *)key, 
			    NULL);
	e_config_listener_set_boolean (config->db, path, bool);
	g_free (path);
	g_free (key);
#endif
	
	return TRUE;
}

void
mail_config_write_on_exit (void)
{
	MailConfigAccount *account;
	const GSList *accounts;
	char *path, *p;
	int i;
	
	if (config_write_timeout) {
		g_source_remove (config_write_timeout);
		config_write_timeout = 0;
		mail_config_write ();
	}
	
	/* Message Threading */
	if (config->threaded_hash)
		g_hash_table_foreach_remove (config->threaded_hash, hash_save_state, "Threads");
	
	/* Message Preview */
	if (config->preview_hash)
		g_hash_table_foreach_remove (config->preview_hash, hash_save_state, "Preview");
	
	/* Passwords */
	
	/* then we make sure the ones we want to remember are in the
           session cache */
	accounts = config->accounts;
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
	e_passwords_clear_component_passwords ("Mail");
	
	/* then we remember them */
	accounts = config->accounts;
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
mail_config_get_show_preview (const char *uri)
{
#warning "FIXME: need to rework how we save state, probably shouldn't use gconf"
#if 0
	if (uri && *uri) {
		gpointer key, val;
		char *dbkey;
		
		dbkey = uri_to_key (uri);
		
		if (!config->preview_hash)
			config->preview_hash = g_hash_table_new (g_str_hash, g_str_equal);
		
		if (!g_hash_table_lookup_extended (config->preview_hash, dbkey, &key, &val)) {
			gboolean value;
			char *str;
			
			str = g_strdup_printf ("/apps/Evolution/Mail/Preview/%s", dbkey);
			value = e_config_listener_get_boolean_with_default (config->db, str, TRUE, NULL);
			g_free (str);
			
			g_hash_table_insert (config->preview_hash, dbkey,
					     GINT_TO_POINTER (value));
			
			return value;
		} else {
			g_free (dbkey);
			return GPOINTER_TO_INT (val);
		}
	}
#endif
	
	/* return the default value */
	
	return gconf_client_get_bool (config->gconf, "/apps/evolution/mail/display/show_preview", NULL);
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
	}
}

gboolean
mail_config_get_thread_list (const char *uri)
{
#warning "FIXME: need to rework how we save state, probably shouldn't use gconf"
#if 0
	if (uri && *uri) {
		gpointer key, val;
		char *dbkey;
		
		dbkey = uri_to_key (uri);
		
		if (!config->threaded_hash)
			config->threaded_hash = g_hash_table_new (g_str_hash, g_str_equal);
		
		if (!g_hash_table_lookup_extended (config->threaded_hash, dbkey, &key, &val)) {
			gboolean value;
			char *str;
			
			str = g_strdup_printf ("/apps/Evolution/Mail/Threads/%s", dbkey);
			value = e_config_listener_get_boolean_with_default (config->db, str, FALSE, NULL);
			g_free (str);
			
			g_hash_table_insert (config->threaded_hash, dbkey,
					     GINT_TO_POINTER (value));
			
			return value;
		} else {
			g_free(dbkey);
			return GPOINTER_TO_INT (val);
		}
	}
#endif
	
	/* return the default value */
	
	return gconf_client_get_bool (config->gconf, "/apps/evolution/mail/display/thread_list", NULL);
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
	}
}

const char *
mail_config_get_label_name (int label)
{
	g_return_val_if_fail (label >= 0 && label < 5, NULL);
	
	if (!config->labels[label].name)
		config->labels[label].name = g_strdup (_(label_defaults[label].name));
	
	return config->labels[label].name;
}

void
mail_config_set_label_name (int label, const char *name)
{
	g_return_if_fail (label >= 0 && label < 5);
	
	if (!name)
		name = _(label_defaults[label].name);
	
	g_free (config->labels[label].name);
	config->labels[label].name = g_strdup (name);
}

guint32
mail_config_get_label_color (int label)
{
	g_return_val_if_fail (label >= 0 && label < 5, 0);
	
	return config->labels[label].color;
}

void
mail_config_set_label_color (int label, guint32 color)
{
	g_return_if_fail (label >= 0 && label < 5);
	
	g_free (config->labels[label].string);
	config->labels[label].string = NULL;
	
	config->labels[label].color = color;
}

const char *
mail_config_get_label_color_string (int label)
{
	g_return_val_if_fail (label >= 0 && label < 5, NULL);
	
	if (!config->labels[label].string) {
		guint32 rgb = config->labels[label].color;
		char *colour;
		
		colour = g_strdup_printf ("#%.2x%.2x%.2x",
					  (rgb & 0xff0000) >> 16,
					  (rgb & 0xff00) >> 8,
					  rgb & 0xff);
		
		config->labels[label].string = colour;
	}
	
	return config->labels[label].string;
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
	int index;
	
	if (config == NULL)
		mail_config_init ();
	
	if (!config->accounts)
		return NULL;
	
	index = gconf_client_get_int (config->gconf, "/apps/evolution/mail/default_account", NULL);
	account = g_slist_nth_data (config->accounts, index);
	
	/* Looks like we have no default, so make the first account
           the default */
	if (account == NULL) {
		gconf_client_set_int (config->gconf, "/apps/evolution/mail/default_account", 0, NULL);
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

void
mail_config_add_account (MailConfigAccount *account)
{
	config->accounts = g_slist_append (config->accounts, account);
	
	accounts_save ();
}

const GSList *
mail_config_remove_account (MailConfigAccount *account)
{
	int index, cur;
	
	cur = gconf_client_get_int (config->gconf, "/apps/evolution/mail/default_account", NULL);
	
	if (account == mail_config_get_default_account ()) {
		/* the default account has been deleted, the new
                   default becomes the first account in the list */
		gconf_client_set_int (config->gconf, "/apps/evolution/mail/default_account", 0, NULL);
	} else {
		/* adjust the default to make sure it points to the same one */
		index = g_slist_index (config->accounts, account);
		if (cur > index)
			gconf_client_set_int (config->gconf, "/apps/evolution/mail/default_account", cur - 1, NULL);
	}
	
	config->accounts = g_slist_remove (config->accounts, account);
	account_destroy (account);
	
	accounts_save ();
	
	return config->accounts;
}

int
mail_config_get_default_account_num (void)
{
	return gconf_client_get_int (config->gconf, "/apps/evolution/mail/default_account", NULL);
}

void
mail_config_set_default_account (const MailConfigAccount *account)
{
	int index;
	
	index = g_slist_index (config->accounts, (void *) account);
	if (index == -1)
		return;
	
	gconf_client_set_int (config->gconf, "/apps/evolution/mail/default_account", index, NULL);
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
	const GSList *accounts;
	
	account = mail_config_get_default_account ();
	if (account && account->transport && account->transport->url)
		return account->transport;
	
	/* return the first account with a transport? */
	accounts = config->accounts;
	while (accounts) {
		account = accounts->data;
		
		if (account->transport && account->transport->url)
			return account->transport;
		
		accounts = accounts->next;
	}
	
	return NULL;
}

static char *
uri_to_evname (const char *uri, const char *prefix)
{
	char *safe;
	char *tmp;

	safe = g_strdup (uri);
	e_filename_make_safe (safe);
	/* blah, easiest thing to do */
	if (prefix[0] == '*')
		tmp = g_strdup_printf ("%s/%s%s.xml", evolution_dir, prefix + 1, safe);
	else
		tmp = g_strdup_printf ("%s/%s%s", evolution_dir, prefix, safe);
	g_free (safe);
	return tmp;
}

void
mail_config_uri_renamed(GCompareFunc uri_cmp, const char *old, const char *new)
{
	MailConfigAccount *ac;
	const GSList *l;
	int work = 0;
	gpointer oldkey, newkey, hashkey;
	gpointer val;
	char *oldname, *newname;
	char *cachenames[] = { "config/hidestate-", 
			       "config/et-expanded-", 
			       "config/et-header-", 
			       "*views/mail/current_view-",
			       "*views/mail/custom_view-",
			       NULL };
	int i;

	l = mail_config_get_accounts();
	while (l) {
		ac = l->data;
		if (ac->sent_folder_uri && uri_cmp(ac->sent_folder_uri, old)) {
			g_free(ac->sent_folder_uri);
			ac->sent_folder_uri = g_strdup(new);
			work = 1;
		}
		if (ac->drafts_folder_uri && uri_cmp(ac->drafts_folder_uri, old)) {
			g_free(ac->drafts_folder_uri);
			ac->drafts_folder_uri = g_strdup(new);
			work = 1;
		}
		l = l->next;
	}

	oldkey = uri_to_key (old);
	newkey = uri_to_key (new);

	/* call this to load the hash table and the key */
	mail_config_get_thread_list (old);
	if (g_hash_table_lookup_extended (config->threaded_hash, oldkey, &hashkey, &val)) {
		/*printf ("changing key in threaded_hash\n");*/
		g_hash_table_remove (config->threaded_hash, hashkey);
		g_hash_table_insert (config->threaded_hash, g_strdup(newkey), val);
		work = 2;
	}

	/* ditto */
	mail_config_get_show_preview (old);
	if (g_hash_table_lookup_extended (config->preview_hash, oldkey, &hashkey, &val)) {
		/*printf ("changing key in preview_hash\n");*/
		g_hash_table_remove (config->preview_hash, hashkey);
		g_hash_table_insert (config->preview_hash, g_strdup(newkey), val);
		work = 2;
	}

	g_free (oldkey);
	g_free (newkey);

	/* ignore return values or if the files exist or
	 * not, doesn't matter */

	for (i = 0; cachenames[i]; i++) {
		oldname = uri_to_evname (old, cachenames[i]);
		newname = uri_to_evname (new, cachenames[i]);
		/*printf ("** renaming %s to %s\n", oldname, newname);*/
		rename (oldname, newname);
		g_free (oldname);
		g_free (newname);
	}

	/* nasty ... */
	if (work)
		mail_config_write();
}

void
mail_config_uri_deleted(GCompareFunc uri_cmp, const char *uri)
{
	MailConfigAccount *ac;
	const GSList *l;
	int work = 0;
	/* assumes these can't be removed ... */
	extern char *default_sent_folder_uri, *default_drafts_folder_uri;

	l = mail_config_get_accounts();
	while (l) {
		ac = l->data;
		if (ac->sent_folder_uri && uri_cmp(ac->sent_folder_uri, uri)) {
			g_free(ac->sent_folder_uri);
			ac->sent_folder_uri = g_strdup(default_sent_folder_uri);
			work = 1;
		}
		if (ac->drafts_folder_uri && uri_cmp(ac->drafts_folder_uri, uri)) {
			g_free(ac->drafts_folder_uri);
			ac->drafts_folder_uri = g_strdup(default_drafts_folder_uri);
			work = 1;
		}
		l = l->next;
	}

	/* nasty again */
	if (work)
		mail_config_write();
}

void 
mail_config_service_set_save_passwd (MailConfigService *service, gboolean save_passwd)
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

	camel_object_unref (service);
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
check_response (GtkDialog *dialog, int button, gpointer data)
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

	dialog = gtk_dialog_new_with_buttons(_("Connecting to server..."), window, GTK_DIALOG_DESTROY_WITH_PARENT,
					     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					     NULL);
	label = gtk_label_new (_("Connecting to server..."));
	gtk_box_pack_start (GTK_BOX(GTK_DIALOG (dialog)->vbox),
			    label, TRUE, TRUE, 10);
	g_signal_connect(dialog, "response", G_CALLBACK (check_response), &id);
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

static gboolean
do_config_write (gpointer data)
{
	config_write_timeout = 0;
	mail_config_write ();
	return FALSE;
}

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
	mail_account->enabled = source.enabled;
	
	/* Copy ID */
	id = account->id;
	mail_id = g_new0 (MailConfigIdentity, 1);
	mail_id->name = g_strdup (id.name);
	mail_id->address = g_strdup (id.address);
	mail_id->reply_to = g_strdup (id.reply_to);
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
	
	mail_account->transport = mail_service;
	
	/* Add new account */
	mail_config_add_account (mail_account);
	
	/* Don't write out the config right away in case the remote
	 * component is creating or removing multiple accounts.
	 */
	if (!config_write_timeout)
		config_write_timeout = g_timeout_add (2000, do_config_write, NULL);
}

static void
impl_GNOME_Evolution_MailConfig_removeAccount (PortableServer_Servant servant,
					       const CORBA_char *name,
					       CORBA_Environment *ev)
{
	MailConfigAccount *account;

	account = (MailConfigAccount *)mail_config_get_account_by_name (name);
	if (account)
		mail_config_remove_account (account);

	/* Don't write out the config right away in case the remote
	 * component is creating or removing multiple accounts.
	 */
	if (!config_write_timeout)
		config_write_timeout = g_timeout_add (2000, do_config_write, NULL);
}

static void
evolution_mail_config_class_init (EvolutionMailConfigClass *klass)
{
	POA_GNOME_Evolution_MailConfig__epv *epv = &klass->epv;

	parent_class = g_type_class_ref(PARENT_TYPE);
	epv->addAccount = impl_GNOME_Evolution_MailConfig_addAccount;
	epv->removeAccount = impl_GNOME_Evolution_MailConfig_removeAccount;
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
				  const char *id,
				  void *closure)
{
	EvolutionMailConfig *config;

	config = g_object_new (evolution_mail_config_get_type (), NULL);

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

	filename = g_build_filename (evolution_dir, "/signatures", NULL);
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
			gint fd;

			fd = creat (filename, 0600);
			if (fd >= 0) {
				close (fd);
				return filename;
			}
		}
		g_free (filename);
	}

	return NULL;
}

MailConfigSignature *
mail_config_signature_add (gboolean html, const gchar *script)
{
	MailConfigSignature *sig;

	sig = g_new0 (MailConfigSignature, 1);

	/* printf ("mail_config_signature_add %d\n", config->signatures); */
	sig->id = config->signatures;
	sig->name = g_strdup (_("Unnamed"));
	if (script)
		sig->script = g_strdup (script);
	else
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
	gchar *signatures_dir;

	signatures_dir = g_strconcat (evolution_dir, "/signatures", NULL);

	/* remove signature file if it's in evolution dir and no other signature uses it */
	len = strlen (signatures_dir);
	if (filename && !strncmp (filename, signatures_dir, len)) {
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

	g_free (signatures_dir);
}

void
mail_config_signature_delete (MailConfigSignature *sig)
{
	GList *l, *next;
	GSList *al;
	gboolean after = FALSE;

	for (al = config->accounts; al; al = al->next) {
		MailConfigAccount *account;

		account = (MailConfigAccount *) al->data;

		if (account->id->def_signature == sig)
			account->id->def_signature = NULL;
	}

	for (l = config->signature_list; l; l = next) {
		next = l->next;
		if (after)
			((MailConfigSignature *) l->data)->id --;
		else if (l->data == sig) {
			config->signature_list = g_list_remove_link (config->signature_list, l);
			after = TRUE;
			config->signatures --;
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
	mail_config_signature_write (sig);
}

void
mail_config_signature_set_name (MailConfigSignature *sig, const gchar *name)
{
	g_free (sig->name);
	sig->name = g_strdup (name);

	mail_config_signature_write (sig);
	mail_config_signature_emit_event (MAIL_CONFIG_SIG_EVENT_NAME_CHANGED, sig);
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

gchar *
mail_config_signature_run_script (gchar *script)
{
	GConfClient *gconf;
	int result, status;
	int in_fds[2];
	pid_t pid;
	
	if (pipe (in_fds) == -1) {
		g_warning ("Failed to create pipe to '%s': %s", script, g_strerror (errno));
		return NULL;
	}
	
	gconf = gconf_client_get_default ();
	
	if (!(pid = fork ())) {
		/* child process */
		int maxfd, i;
		
		close (in_fds [0]);
		if (dup2 (in_fds[1], STDOUT_FILENO) < 0)
			_exit (255);
		close (in_fds [1]);
		
		setsid ();
		
		maxfd = sysconf (_SC_OPEN_MAX);
		if (maxfd > 0) {
			for (i = 0; i < maxfd; i++) {
				if (i != STDIN_FILENO && i != STDOUT_FILENO && i != STDERR_FILENO)
					close (i);
			}
		}
		
		
		execlp (script, script, NULL);
		g_warning ("Could not execute %s: %s\n", script, g_strerror (errno));
		_exit (255);
	} else if (pid < 0) {
		g_warning ("Failed to create create child process '%s': %s", script, g_strerror (errno));
		return NULL;
	} else {
		CamelStreamFilter *filtered_stream;
		CamelStreamMem *memstream;
		CamelMimeFilter *charenc;
		CamelStream *stream;
		GByteArray *buffer;
		const char *charset;
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
		if (!g_utf8_validate (buffer->data, buffer->len, NULL)) {
			stream = (CamelStream *) memstream;
			memstream = (CamelStreamMem *) camel_stream_mem_new ();
			camel_stream_mem_set_byte_array (memstream, g_byte_array_new ());
			
			filtered_stream = camel_stream_filter_new_with_stream (stream);
			camel_object_unref (stream);
			
			charset = gconf_client_get_string (gconf, "/apps/evolution/mail/composer/charset", NULL);
			charenc = (CamelMimeFilter *) camel_mime_filter_charset_new_convert (charset, "utf-8");
			camel_stream_filter_add (filtered_stream, charenc);
			camel_object_unref (charenc);
			g_free (charset);
			
			camel_stream_write_to_stream ((CamelStream *) filtered_stream, (CamelStream *) memstream);
			camel_object_unref (filtered_stream);
			g_byte_array_free (buffer, TRUE);
			
			buffer = memstream->buffer;
		}
		
		camel_object_unref (memstream);
		
		g_byte_array_append (buffer, "", 1);
		content = buffer->data;
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
}

void
mail_config_signature_set_html (MailConfigSignature *sig, gboolean html)
{
	if (sig->html != html) {
		sig->html = html;
		mail_config_signature_write (sig);
		mail_config_signature_emit_event (MAIL_CONFIG_SIG_EVENT_HTML_CHANGED, sig);
	}
}

int
mail_config_get_time_24hour(void)
{
	return config->time_24hour;
}

