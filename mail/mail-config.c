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
	
	EAccountList *accounts;
	guint accounts_notify_id;
	
	GSList *signatures;
	int sig_nextid;
	
	MailConfigLabel labels[5];
	
	gboolean signature_info;
	
	/* readonly fields from calendar */
	int time_24hour;
} MailConfig;

static MailConfig *config = NULL;
static guint config_write_timeout = 0;

#define MAIL_CONFIG_IID "OAFIID:GNOME_Evolution_MailConfig_Factory"

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
	xmlFree (buf);
	
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
lookup_signature (int id)
{
	MailConfigSignature *sig;
	GSList *l;
	
	if (id == -1)
		return NULL;
	
	l = config->signatures;
	while (l != NULL) {
		sig = (MailConfigSignature *) l->data;
		if (sig->id == id)
			return sig;
		
		l = l->next;
	}
	
	return NULL;
}

void
mail_config_save_accounts (void)
{
	e_account_list_save (config->accounts);
}

static MailConfigSignature *
signature_new_from_xml (char *in, int id)
{
	MailConfigSignature *sig;
	xmlNodePtr node, cur;
	xmlDocPtr doc;
	char *buf;
	
	if (!(doc = xmlParseDoc (in)))
		return NULL;
	
	node = doc->children;
	if (strcmp (node->name, "signature") != 0) {
		xmlFreeDoc (doc);
		return NULL;
	}
	
	sig = g_new0 (MailConfigSignature, 1);
	sig->name = xml_get_prop (node, "name");
	sig->id = id;
	
	buf = xml_get_prop (node, "format");
	if (!strcmp (buf, "text/html"))
		sig->html = TRUE;
	else
		sig->html = FALSE;
	g_free (buf);
	
	cur = node->children;
	while (cur) {
		if (!strcmp (cur->name, "filename")) {
			g_free (sig->filename);
			sig->filename = xml_get_content (cur);
		} else if (!strcmp (cur->name, "script")) {
			g_free (sig->script);
			sig->script = xml_get_content (cur);
		}
		
		cur = cur->next;
	}
	
	xmlDocFree (doc);
	
	return sig;
}

static void
config_read_signatures (void)
{
	MailConfigSignature *sig;
	GSList *list, *l, *tail, *n;
	int i = 0;
	
	config->signatures = NULL;
	
	tail = NULL;
	list = gconf_client_get_list (config->gconf, "/apps/evolution/mail/signatures",
				      GCONF_VALUE_STRING, NULL);
	
	l = list;
	while (l != NULL) {
		MailConfigSignature *sig;
		
		if ((sig = signature_new_from_xml ((char *) l->data, i++))) {
			n = g_slist_alloc ();
			n->next = NULL;
			n->data = sig;
			
			if (tail == NULL)
				config->signatures = n;
			else
				tail->next = n;
			tail = n;
		}
		
		n = l->next;
		g_slist_free_1 (l);
		l = n;
	}
	
	config->sig_nextid = i + 1;
}

static char *
signature_to_xml (MailConfigSignature *sig)
{
	char *xmlbuf, *tmp;
	xmlNodePtr root;
	xmlDocPtr doc;
	int n;
	
	doc = xmlNewDoc ("1.0");
	
	root = xmlNewDocNode (doc, NULL, "signature", NULL);
	xmlDocSetRootElement (doc, root);
	
	xmlSetProp (root, "name", sig->name);
	xmlSetProp (root, "format", sig->html ? "text/html" : "text/plain");
	
	if (sig->filename)
		xmlNewTextChild (root, NULL, "filename", sig->filename);
	
	if (sig->script)
		xmlNewTextChild (root, NULL, "script", sig->script);
	
	xmlDocDumpMemory (doc, (xmlChar **) &xmlbuf, &n);
	xmlFreeDoc (doc);
	
	/* remap to glib memory */
	tmp = g_malloc (n + 1);
	memcpy (tmp, xmlbuf, n);
	tmp[n] = '\0';
	xmlFree (xmlbuf);
	
	return tmp;
}

static void
config_write_signatures (void)
{
	GSList *list, *tail, *n, *l;
	char *xmlbuf;
	
	list = NULL;
	tail = NULL;
	
	l = config->signatures;
	while (l != NULL) {
		if ((xmlbuf = signature_to_xml ((MailConfigSignature *) l->data))) {
			n = g_slist_alloc ();
			n->data = xmlbuf;
			n->next = NULL;
			
			if (tail == NULL)
				list = n;
			else
				tail->next = n;
			tail = n;
		}
		
		l = l->next;
	}
	
	gconf_client_set_list (config->gconf, "/apps/evolution/mail/signatures", GCONF_VALUE_STRING, list, NULL);
	
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
	
	mail_config_clear ();
	
	config_read_signatures ();
	
	config->accounts = e_account_list_new (config->gconf);
}

void
mail_config_clear (void)
{
	int i;
	
	if (!config)
		return;
	
	if (config->accounts) {
		g_object_unref (config->accounts);
		config->accounts = NULL;
	}
	
	for (i = 0; i < 5; i++) {
		g_free (config->labels[i].name);
		config->labels[i].name = NULL;
		g_free (config->labels[i].string);
		config->labels[i].string = NULL;
	}
}

void
mail_config_write_account_sig (EAccount *account, int id)
{
	/* FIXME: what is this supposed to do? */
	;
}

void
mail_config_write (void)
{
	if (!config)
		return;
	
	config_write_signatures ();
	e_account_list_save (config->accounts);
	
	gconf_client_suggest_sync (config->gconf, NULL);
}

void
mail_config_write_on_exit (void)
{
	EAccount *account;
	EIterator *iter;
	char *path, *p;
	int i;
	
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
		
		e_iterator_next (iter);
	}
	
	g_object_unref (iter);
	
	/* then we clear out our component passwords */
	e_passwords_clear_component_passwords ("Mail");
	
	/* then we remember them */
	iter = e_list_get_iterator ((EList *) config->accounts);
	while (e_iterator_is_valid (iter)) {
		account = (EAccount *) e_iterator_get (iter);
		
		if (account->source->save_passwd && account->source->url)
			mail_session_remember_password (account->source->url);
		
		if (account->transport->save_passwd && account->transport->url)
			mail_session_remember_password (account->transport->url);
		
		e_iterator_next (iter);
	}
	
	/* now do cleanup */
	mail_config_clear ();
}

/* Accessor functions */
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
	EAccount *account = NULL;
	EIterator *iter;
	int index, n;
	
	if (config == NULL)
		mail_config_init ();
	
	if (!config->accounts)
		return NULL;
	
	index = gconf_client_get_int (config->gconf, "/apps/evolution/mail/default_account", NULL);
	
	iter = e_list_get_iterator ((EList *) config->accounts);
	n = 0;
	
	while (e_iterator_is_valid (iter)) {
		if (n == index) {
			account = (EAccount *) e_iterator_get (iter);
			break;
		}
		
		n++;
		e_iterator_next (iter);
	}
	
	if (account == NULL) {
		/* Looks like we have no default, so make the first account
		   the default */
		e_iterator_reset (iter);
		account = (EAccount *) e_iterator_get (iter);
		
		gconf_client_set_int (config->gconf, "/apps/evolution/mail/default_account", 0, NULL);
	}
	
	g_object_unref (iter);
	
	return account;
}

EAccount *
mail_config_get_account_by_name (const char *account_name)
{
	EAccount *account;
	EIterator *iter;
	
	iter = e_list_get_iterator ((EList *) config->accounts);
	while (e_iterator_is_valid (iter)) {
		account = (EAccount *) e_iterator_get (iter);
		if (!strcmp (account->name, account_name)) {
			g_object_unref (iter);
			return account;
		}
		
		e_iterator_next (iter);
	}
	
	g_object_unref (iter);
	
	return NULL;
}

EAccount *
mail_config_get_account_by_source_url (const char *source_url)
{
	CamelProvider *provider;
	EAccount *account;
	CamelURL *source;
	EIterator *iter;
	
	g_return_val_if_fail (source_url != NULL, NULL);
	
	provider = camel_session_get_provider (session, source_url, NULL);
	if (!provider)
		return NULL;
	
	source = camel_url_new (source_url, NULL);
	if (!source)
		return NULL;
	
	iter = e_list_get_iterator ((EList *) config->accounts);
	while (e_iterator_is_valid (iter)) {
		account = (EAccount *) e_iterator_get (iter);
		
		if (account->source && account->source->url) {
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
	
	provider = camel_session_get_provider (session, transport_url, NULL);
	if (!provider)
		return NULL;
	
	transport = camel_url_new (transport_url, NULL);
	if (!transport)
		return NULL;
	
	iter = e_list_get_iterator ((EList *) config->accounts);
	while (e_iterator_is_valid (iter)) {
		account = (EAccount *) e_iterator_get (iter);
		
		if (account->transport && account->transport->url) {
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

EAccountList *
mail_config_get_accounts (void)
{
	g_assert (config != NULL);
	
	return config->accounts;
}

void
mail_config_add_account (EAccount *account)
{
	e_list_append ((EList *) config->accounts, account);
	g_signal_emit_by_name (config->accounts, "account-added", account);
	
	mail_config_save_accounts ();
}

void
mail_config_remove_account (EAccount *account)
{
	EAccount *acnt = NULL;
	EIterator *iter;
	int index, cur;
	
	cur = gconf_client_get_int (config->gconf, "/apps/evolution/mail/default_account", NULL);
	
	if (account == mail_config_get_default_account ()) {
		/* the default account has been deleted, the new
                   default becomes the first account in the list */
		gconf_client_set_int (config->gconf, "/apps/evolution/mail/default_account", 0, NULL);
	} else {
		/* adjust the default to make sure it points to the same one */
		index = 0;
		iter = e_list_get_iterator ((EList *) config->accounts);
		while (e_iterator_is_valid (iter)) {
			acnt = (EAccount *) e_iterator_get (iter);
			if (acnt == account)
				break;
			
			index++;
			e_iterator_next (iter);
		}
		
		g_object_unref (iter);
		
		if (cur > index)
			gconf_client_set_int (config->gconf, "/apps/evolution/mail/default_account", cur - 1, NULL);
	}
	
	g_object_ref (account);
	e_list_remove ((EList *) config->accounts, account);
	g_signal_emit_by_name (config->accounts, "account-removed", account);
	g_object_unref (account);
	
	mail_config_save_accounts ();
}

void
mail_config_set_default_account (EAccount *account)
{
	EIterator *iter;
	EAccount *acnt;
	int index = -1;
	int i = 0;
	
	iter = e_list_get_iterator ((EList *) config->accounts);
	while (e_iterator_is_valid (iter)) {
		acnt = (EAccount *) e_iterator_get (iter);
		if (acnt == account) {
			index = i;
			break;
		}
		
		i++;
		e_iterator_next (iter);
	}
	
	g_object_unref (iter);
	
	if (index == -1)
		return;
	
	gconf_client_set_int (config->gconf, "/apps/evolution/mail/default_account", index, NULL);
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
	if (account && account->transport && account->transport->url)
		return account->transport;
	
	/* return the first account with a transport? */
	iter = e_list_get_iterator ((EList *) config->accounts);
	while (e_iterator_is_valid (iter)) {
		account = (EAccount *) e_iterator_get (iter);
		
		if (account->transport && account->transport->url) {
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
mail_config_uri_renamed (GCompareFunc uri_cmp, const char *old, const char *new)
{
	EAccount *account;
	EIterator *iter;
	int i, work = 0;
	gpointer val;
	char *oldname, *newname;
	char *cachenames[] = { "config/hidestate-", 
			       "config/et-expanded-", 
			       "config/et-header-", 
			       "*views/mail/current_view-",
			       "*views/mail/custom_view-",
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
		rename (oldname, newname);
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
	extern char *default_sent_folder_uri, *default_drafts_folder_uri;
	
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

	dialog = gtk_dialog_new_with_buttons(_("Connecting to server..."), window,
					     GTK_DIALOG_DESTROY_WITH_PARENT,
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
#define PARENT_TYPE BONOBO_OBJECT_TYPE
static BonoboObjectClass *parent_class = NULL;

/* For the bonobo object */
typedef struct _EvolutionMailConfig EvolutionMailConfig;
typedef struct _EvolutionMailConfigClass EvolutionMailConfigClass;

struct _EvolutionMailConfig {
	BonoboObject parent;
};

struct _EvolutionMailConfigClass {
	BonoboObjectClass parent_class;

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
	EAccount *new;
	
	if (mail_config_get_account_by_name (account->name)) {
		/* FIXME: we need an exception. */
		return;
	}
	
	new = e_account_new ();
	new->name = g_strdup (account->name);
	new->enabled = source.enabled;
	
	/* Copy ID */
	id = account->id;
	new->id->name = g_strdup (id.name);
	new->id->address = g_strdup (id.address);
	new->id->reply_to = g_strdup (id.reply_to);
	new->id->organization = g_strdup (id.organization);
	
	/* Copy source */
	source = account->source;
	if (!(source.url == NULL || strcmp (source.url, "none://") == 0))
		new->source->url = g_strdup (source.url);
	
	new->source->keep_on_server = source.keep_on_server;
	new->source->auto_check = source.auto_check;
	new->source->auto_check_time = source.auto_check_time;
	new->source->save_passwd = source.save_passwd;
	
	/* Copy transport */
	transport = account->transport;
	if (transport.url != NULL)
		new->transport->url = g_strdup (transport.url);
	
	new->transport->url = g_strdup (transport.url);
	new->transport->save_passwd = transport.save_passwd;
	
	/* Add new account */
	mail_config_add_account (new);
	
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
	EAccount *account;
	
	if ((account = mail_config_get_account_by_name (name)))
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
	;
}

BONOBO_TYPE_FUNC_FULL (EvolutionMailConfig,
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

GSList *
mail_config_get_signature_list (void)
{
	return config->signatures;
}

static char *
get_new_signature_filename (void)
{
	char *filename, *id;
	struct stat st;
	int i;
	
	filename = g_build_filename (evolution_dir, "/signatures", NULL);
	if (lstat (filename, &st)) {
		if (errno == ENOENT) {
			if (mkdir (filename, 0700))
				g_warning ("Fatal problem creating %s/signatures directory.", evolution_dir);
		} else
			g_warning ("Fatal problem with %s/signatures directory.", evolution_dir);
	}
	g_free (filename);
	
	filename = g_malloc (strlen (evolution_dir) + sizeof ("/signatures/signature-") + 12);
	id = g_stpcpy (filename, evolution_dir);
	id = g_stpcpy (id, "/signatures/signature-");
	
	for (i = 0; i < (INT_MAX - 1); i++) {
		sprintf (id, "%d", i);
		if (lstat (filename, &st) == -1 && errno == ENOENT) {
			int fd;
			
			fd = creat (filename, 0600);
			if (fd >= 0) {
				close (fd);
				return filename;
			}
		}
	}
	
	g_free (filename);
	
	return NULL;
}

MailConfigSignature *
mail_config_signature_add (gboolean html, const char *script)
{
	MailConfigSignature *sig;
	
	sig = g_new0 (MailConfigSignature, 1);
	
	/* printf ("mail_config_signature_add %d\n", config->sig_nextid); */
	sig->id = config->sig_nextid++;
	sig->name = g_strdup (_("Unnamed"));
	if (script)
		sig->script = g_strdup (script);
	else
		sig->filename = get_new_signature_filename ();
	sig->html = html;
	
	config->signatures = g_slist_append (config->signatures, sig);
	
	config_write_signatures ();
	
	mail_config_signature_emit_event (MAIL_CONFIG_SIG_EVENT_ADDED, sig);
	/* printf ("mail_config_signature_add end\n"); */
	
	return sig;
}

static void
delete_unused_signature_file (const char *filename)
{
	char *signatures_dir;
	int len;
	
	signatures_dir = g_strconcat (evolution_dir, "/signatures", NULL);
	
	/* remove signature file if it's in evolution dir and no other signature uses it */
	len = strlen (signatures_dir);
	if (filename && !strncmp (filename, signatures_dir, len)) {
		gboolean only_one = TRUE;
		GSList *node;
		
		node = config->signatures;
		while (node != NULL) {
			MailConfigSignature *sig = node->data;
			
			if (sig->filename && !strcmp (filename, sig->filename)) {
				only_one = FALSE;
				break;
			}
			
			node = node->next;
		}
		
		if (only_one)
			unlink (filename);
	}
	
	g_free (signatures_dir);
}

void
mail_config_signature_delete (MailConfigSignature *sig)
{
	EAccount *account;
	EIterator *iter;
	GSList *node, *next;
	gboolean after = FALSE;
	int index;
	
	index = g_slist_index (config->signatures, sig);
	
	iter = e_list_get_iterator ((EList *) config->accounts);
	while (e_iterator_is_valid (iter)) {
		account = (EAccount *) e_iterator_get (iter);
		
		if (account->id->def_signature == index)
			account->id->def_signature = -1;
		else if (account->id->def_signature > index)
			account->id->def_signature--;
		
		e_iterator_next (iter);
	}
	
	g_object_unref (iter);
	
	node = config->signatures;
	while (node != NULL) {
		next = node->next;
		
		if (after) {
			((MailConfigSignature *) node->data)->id--;
		} else if (node->data == sig) {
			config->signatures = g_slist_remove_link (config->signatures, node);
			config->sig_nextid--;
			after = TRUE;
		}
		
		node = next;
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
mail_config_signature_set_filename (MailConfigSignature *sig, const char *filename)
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
mail_config_signature_set_name (MailConfigSignature *sig, const char *name)
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
		GConfClient *gconf;
		GByteArray *buffer;
		char *charset;
		char *content;
		
		/* parent process */
		close (in_fds[1]);
		
		gconf = gconf_client_get_default ();
		
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

