/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003 Ximian, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "e-account.h"

#include "e-uid.h"

#include <string.h>

#include <gal/util/e-util.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlmemory.h>

#include <gconf/gconf-client.h>

#define PARENT_TYPE G_TYPE_OBJECT
static GObjectClass *parent_class = NULL;

enum {
	CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

/*
lock mail accounts	Relatively difficult -- involves redesign of the XML blobs which describe accounts
disable adding mail accounts	Simple -- can be done with just a Gconf key and some UI work to make assoc. widgets unavailable
disable editing mail accounts	Relatively difficult -- involves redesign of the XML blobs which describe accounts
disable removing mail accounts	
lock default character encoding	Simple -- Gconf key + a little UI work to desensitize widgets, etc
disable free busy publishing	
disable specific mime types (from being viewed)	90% done already (Unknown MIME types still pose a problem)
lock image loading preference	
lock junk mail filtering settings	
**  junk mail per account
lock work week	
lock first day of work week	
lock working hours	
disable forward as icalendar	
lock color options for tasks	
lock default contact filing format	
* forbid signatures	Simple -- can be done with just a Gconf key and some UI work to make assoc. widgets unavailable
* lock user to having 1 specific signature	Simple -- can be done with just a Gconf key and some UI work to make assoc. widgets unavailable
* forbid adding/removing signatures	Simple -- can be done with just a Gconf key and some UI work to make assoc. widgets unavailable
* lock each account to a certain signature	Relatively difficult -- involved redesign of the XML blobs which describe accounts 
* set default folders	
set trash emptying frequency	
* lock displayed mail headers	Simple -- can be done with just a Gconf key and some UI work to make assoc. widgets unavailable
* lock authentication type (for incoming mail)	Relatively difficult -- involves redesign of the XML blobs which describe accounts
* lock authentication type (for outgoing mail)	Relatively difficult -- involves redesign of the XML blobs which describe accounts
* lock minimum check mail on server frequency	Simple -- can be done with just a Gconf key and some UI work to make assoc. widgets unavailable
** lock save password
* require ssl always	Relatively difficult -- involves redesign of the XML blobs which describe accounts
** lock imap subscribed folder option
** lock filtering of inbox
** lock source account/options
** lock destination account/options
*/

static void finalize (GObject *);

static void
class_init (GObjectClass *object_class)
{
	parent_class = g_type_class_ref (PARENT_TYPE);

	/* virtual method override */
	object_class->finalize = finalize;

	signals[CHANGED] =
		g_signal_new("changed",
			     G_OBJECT_CLASS_TYPE (object_class),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET (EAccountClass, changed),
			     NULL, NULL,
			     g_cclosure_marshal_VOID__INT,
			     G_TYPE_NONE, 1,
			     G_TYPE_INT);
}

static void
init (EAccount *account)
{
	account->id = g_new0 (EAccountIdentity, 1);
	account->source = g_new0 (EAccountService, 1);
	account->transport = g_new0 (EAccountService, 1);

	account->source->auto_check = FALSE;
	account->source->auto_check_time = 10;
}

static void
identity_destroy (EAccountIdentity *id)
{
	if (!id)
		return;

	g_free (id->name);
	g_free (id->address);
	g_free (id->reply_to);
	g_free (id->organization);
	g_free (id->sig_uid);
	
	g_free (id);
}

static void
service_destroy (EAccountService *service)
{
	if (!service)
		return;

	g_free (service->url);

	g_free (service);
}

static void
finalize (GObject *object)
{
	EAccount *account = E_ACCOUNT (object);

	g_free (account->name);
	g_free (account->uid);

	identity_destroy (account->id);
	service_destroy (account->source);
	service_destroy (account->transport);

	g_free (account->drafts_folder_uri);
	g_free (account->sent_folder_uri);

	g_free (account->cc_addrs);
	g_free (account->bcc_addrs);

	g_free (account->pgp_key);
	g_free (account->smime_sign_key);
	g_free (account->smime_encrypt_key);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

E_MAKE_TYPE (e_account, "EAccount", EAccount, class_init, init, PARENT_TYPE)


/**
 * e_account_new:
 *
 * Return value: a blank new account which can be filled in and
 * added to an #EAccountList.
 **/
EAccount *
e_account_new (void)
{
	EAccount *account;

	account = g_object_new (E_TYPE_ACCOUNT, NULL);
	account->uid = e_uid_new ();

	return account;
}

/**
 * e_account_new_from_xml:
 * @xml: an XML account description
 *
 * Return value: a new #EAccount based on the data in @xml, or %NULL
 * if @xml could not be parsed as valid account data.
 **/
EAccount *
e_account_new_from_xml (const char *xml)
{
	EAccount *account;

	account = g_object_new (E_TYPE_ACCOUNT, NULL);
	if (!e_account_set_from_xml (account, xml)) {
		g_object_unref (account);
		return NULL;
	}

	return account;
}


static gboolean
xml_set_bool (xmlNodePtr node, const char *name, gboolean *val)
{
	gboolean bool;
	char *buf;

	if ((buf = xmlGetProp (node, name))) {
		bool = (!strcmp (buf, "true") || !strcmp (buf, "yes"));
		xmlFree (buf);

		if (bool != *val) {
			*val = bool;
			return TRUE;
		}
	}

	return FALSE;
}

static gboolean
xml_set_int (xmlNodePtr node, const char *name, int *val)
{
	int number;
	char *buf;

	if ((buf = xmlGetProp (node, name))) {
		number = strtol (buf, NULL, 10);
		xmlFree (buf);

		if (number != *val) {
			*val = number;
			return TRUE;
		}
	}

	return FALSE;
}

static gboolean
xml_set_prop (xmlNodePtr node, const char *name, char **val)
{
	char *buf;
	int res;

	buf = xmlGetProp(node, name);
	if (buf == NULL) {
		res = (*val != NULL);
		if (res) {
			g_free(*val);
			*val = NULL;
		}
	} else {
		res = *val == NULL || strcmp(*val, buf) != 0;
		if (res) {
			g_free(*val);
			*val = g_strdup(buf);
		}
		xmlFree(buf);
	}

	return res;
}

static gboolean
xml_set_content (xmlNodePtr node, char **val)
{
	char *buf;
	int res;

	buf = xmlNodeGetContent(node);
	if (buf == NULL) {
		res = (*val != NULL);
		if (res) {
			g_free(*val);
			*val = NULL;
		}
	} else {
		res = *val == NULL || strcmp(*val, buf) != 0;
		if (res) {
			g_free(*val);
			*val = g_strdup(buf);
		}
		xmlFree(buf);
	}

	return res;
}

static gboolean
xml_set_identity (xmlNodePtr node, EAccountIdentity *id)
{
	gboolean changed = FALSE;

	for (node = node->children; node; node = node->next) {
		if (!strcmp (node->name, "name"))
			changed |= xml_set_content (node, &id->name);
		else if (!strcmp (node->name, "addr-spec"))
			changed |= xml_set_content (node, &id->address);
		else if (!strcmp (node->name, "reply-to"))
			changed |= xml_set_content (node, &id->reply_to);
		else if (!strcmp (node->name, "organization"))
			changed |= xml_set_content (node, &id->organization);
		else if (!strcmp (node->name, "signature")) {
			changed |= xml_set_prop (node, "uid", &id->sig_uid);
			if (!id->sig_uid) {

				/* WTF is this shit doing here?  Migrate is supposed to "handle this" */

				/* set a fake sig uid so the migrate code can handle this */
				gboolean autogen = FALSE;
				int sig_id = 0;
				
				xml_set_bool (node, "auto", &autogen);
				xml_set_int (node, "default", &sig_id);
				
				if (autogen) {
					id->sig_uid = g_strdup ("::0");
					changed = TRUE;
				} else if (sig_id) {
					id->sig_uid = g_strdup_printf ("::%d", sig_id + 1);
					changed = TRUE;
				}
			}
		}
	}

	return changed;
}

static gboolean
xml_set_service (xmlNodePtr node, EAccountService *service)
{
	gboolean changed = FALSE;

	changed |= xml_set_bool (node, "save-passwd", &service->save_passwd);
	changed |= xml_set_bool (node, "keep-on-server", &service->keep_on_server);

	changed |= xml_set_bool (node, "auto-check", &service->auto_check);
	changed |= xml_set_int (node, "auto-check-timeout", &service->auto_check_time);
	if (service->auto_check && service->auto_check_time <= 0) {
		service->auto_check = FALSE;
		service->auto_check_time = 0;
	}

	for (node = node->children; node; node = node->next) {
		if (!strcmp (node->name, "url")) {
			changed |= xml_set_content (node, &service->url);
			break;
		}
	}

	return changed;
}

/**
 * e_account_set_from_xml:
 * @account: an #EAccount
 * @xml: an XML account description.
 *
 * Changes @account to match @xml.
 *
 * Return value: %TRUE if @account was changed, %FALSE if @account
 * already matched @xml or @xml could not be parsed
 **/
gboolean
e_account_set_from_xml (EAccount *account, const char *xml)
{
	xmlNodePtr node, cur;
	xmlDocPtr doc;
	gboolean changed = FALSE;

	if (!(doc = xmlParseDoc ((char *)xml)))
		return FALSE;

	node = doc->children;
	if (strcmp (node->name, "account") != 0) {
		xmlFreeDoc (doc);
		return FALSE;
	}

	if (!account->uid)
		xml_set_prop (node, "uid", &account->uid);
	
	changed |= xml_set_prop (node, "name", &account->name);
	changed |= xml_set_bool (node, "enabled", &account->enabled);

	for (node = node->children; node; node = node->next) {
		if (!strcmp (node->name, "identity")) {
			changed |= xml_set_identity (node, account->id);
		} else if (!strcmp (node->name, "source")) {
			changed |= xml_set_service (node, account->source);
		} else if (!strcmp (node->name, "transport")) {
			changed |= xml_set_service (node, account->transport);
		} else if (!strcmp (node->name, "drafts-folder")) {
			changed |= xml_set_content (node, &account->drafts_folder_uri);
		} else if (!strcmp (node->name, "sent-folder")) {
			changed |= xml_set_content (node, &account->sent_folder_uri);
		} else if (!strcmp (node->name, "auto-cc")) {
			changed |= xml_set_bool (node, "always", &account->always_cc);
			changed |= xml_set_content (node, &account->cc_addrs);
		} else if (!strcmp (node->name, "auto-bcc")) {
			changed |= xml_set_bool (node, "always", &account->always_bcc);
			changed |= xml_set_content (node, &account->bcc_addrs);
		} else if (!strcmp (node->name, "pgp")) {
			changed |= xml_set_bool (node, "encrypt-to-self", &account->pgp_encrypt_to_self);
			changed |= xml_set_bool (node, "always-trust", &account->pgp_always_trust);
			changed |= xml_set_bool (node, "always-sign", &account->pgp_always_sign);
			changed |= xml_set_bool (node, "no-imip-sign", &account->pgp_no_imip_sign);

			if (node->children) {
				for (cur = node->children; cur; cur = cur->next) {
					if (!strcmp (cur->name, "key-id")) {
						changed |= xml_set_content (cur, &account->pgp_key);
						break;
					}
				}
			}
		} else if (!strcmp (node->name, "smime")) {
			changed |= xml_set_bool (node, "sign-default", &account->smime_sign_default);
			changed |= xml_set_bool (node, "encrypt-to-self", &account->smime_encrypt_to_self);
			changed |= xml_set_bool (node, "encrypt-default", &account->smime_encrypt_default);

			if (node->children) {
				for (cur = node->children; cur; cur = cur->next) {
					if (!strcmp (cur->name, "sign-key-id")) {
						changed |= xml_set_content (cur, &account->smime_sign_key);
					} else if (!strcmp (cur->name, "encrypt-key-id")) {
						changed |= xml_set_content (cur, &account->smime_encrypt_key);
						break;
					}
				}
			}
		}
	}

	xmlFreeDoc (doc);

	g_signal_emit(account, signals[CHANGED], 0, -1);

	return changed;
}

/**
 * e_account_import:
 * @dest: destination account object
 * @src: source account object
 *
 * Import the settings from @src to @dest.
 **/
void
e_account_import (EAccount *dest, EAccount *src)
{
	g_free (dest->name);
	dest->name = g_strdup (src->name);
	
	dest->enabled = src->enabled;
	
	g_free (dest->id->name);
	dest->id->name = g_strdup (src->id->name);
	g_free (dest->id->address);
	dest->id->address = g_strdup (src->id->address);
	g_free (dest->id->reply_to);
	dest->id->reply_to = g_strdup (src->id->reply_to);
	g_free (dest->id->organization);
	dest->id->organization = g_strdup (src->id->organization);
	dest->id->sig_uid = g_strdup (src->id->sig_uid);
	
	g_free (dest->source->url);
	dest->source->url = g_strdup (src->source->url);
	dest->source->keep_on_server = src->source->keep_on_server;
	dest->source->auto_check = src->source->auto_check;
	dest->source->auto_check_time = src->source->auto_check_time;
	dest->source->save_passwd = src->source->save_passwd;
	
	g_free (dest->transport->url);
	dest->transport->url = g_strdup (src->transport->url);
	dest->transport->save_passwd = src->transport->save_passwd;
	
	g_free (dest->drafts_folder_uri);
	dest->drafts_folder_uri = g_strdup (src->drafts_folder_uri);
	
	g_free (dest->sent_folder_uri);
	dest->sent_folder_uri = g_strdup (src->sent_folder_uri);
	
	dest->always_cc = src->always_cc;
	g_free (dest->cc_addrs);
	dest->cc_addrs = g_strdup (src->cc_addrs);
	
	dest->always_bcc = src->always_bcc;
	g_free (dest->bcc_addrs);
	dest->bcc_addrs = g_strdup (src->bcc_addrs);
	
	g_free (dest->pgp_key);
	dest->pgp_key = g_strdup (src->pgp_key);
	dest->pgp_encrypt_to_self = src->pgp_encrypt_to_self;
	dest->pgp_always_sign = src->pgp_always_sign;
	dest->pgp_no_imip_sign = src->pgp_no_imip_sign;
	dest->pgp_always_trust = src->pgp_always_trust;
	
	dest->smime_sign_default = src->smime_sign_default;
	g_free (dest->smime_sign_key);
	dest->smime_sign_key = g_strdup (src->smime_sign_key);

	dest->smime_encrypt_default = src->smime_encrypt_default;
	dest->smime_encrypt_to_self = src->smime_encrypt_to_self;
	g_free (dest->smime_encrypt_key);
	dest->smime_encrypt_key = g_strdup (src->smime_encrypt_key);

	g_signal_emit(dest, signals[CHANGED], 0, -1);
}

/**
 * e_account_to_xml:
 * @account: an #EAccount
 *
 * Return value: an XML representation of @account, which the caller
 * must free.
 **/
char *
e_account_to_xml (EAccount *account)
{
	xmlNodePtr root, node, id, src, xport;
	char *tmp, buf[20];
	xmlChar *xmlbuf;
	xmlDocPtr doc;
	int n;

	doc = xmlNewDoc ("1.0");

	root = xmlNewDocNode (doc, NULL, "account", NULL);
	xmlDocSetRootElement (doc, root);

	xmlSetProp (root, "name", account->name);
	xmlSetProp (root, "uid", account->uid);
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
	
	node = xmlNewChild (id, NULL, "signature",NULL);
	xmlSetProp (node, "uid", account->id->sig_uid);
	
	src = xmlNewChild (root, NULL, "source", NULL);
	xmlSetProp (src, "save-passwd", account->source->save_passwd ? "true" : "false");
	xmlSetProp (src, "keep-on-server", account->source->keep_on_server ? "true" : "false");
	xmlSetProp (src, "auto-check", account->source->auto_check ? "true" : "false");
	sprintf (buf, "%d", account->source->auto_check_time);
	xmlSetProp (src, "auto-check-timeout", buf);
	if (account->source->url)
		xmlNewTextChild (src, NULL, "url", account->source->url);

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
	xmlSetProp (node, "no-imip-sign", account->pgp_no_imip_sign ? "true" : "false");
	if (account->pgp_key)
		xmlNewTextChild (node, NULL, "key-id", account->pgp_key);

	node = xmlNewChild (root, NULL, "smime", NULL);
	xmlSetProp (node, "sign-default", account->smime_sign_default ? "true" : "false");
	xmlSetProp (node, "encrypt-default", account->smime_encrypt_default ? "true" : "false");
	xmlSetProp (node, "encrypt-to-self", account->smime_encrypt_to_self ? "true" : "false");
	if (account->smime_sign_key)
		xmlNewTextChild (node, NULL, "sign-key-id", account->smime_sign_key);
	if (account->smime_encrypt_key)
		xmlNewTextChild (node, NULL, "encrypt-key-id", account->smime_encrypt_key);

	xmlDocDumpMemory (doc, &xmlbuf, &n);
	xmlFreeDoc (doc);

	/* remap to glib memory */
	tmp = g_malloc (n + 1);
	memcpy (tmp, xmlbuf, n);
	tmp[n] = '\0';
	xmlFree (xmlbuf);

	return tmp;
}

/**
 * e_account_uid_from_xml:
 * @xml: an XML account description
 *
 * Return value: the permanent UID of the account described by @xml
 * (or %NULL if @xml could not be parsed or did not contain a uid).
 * The caller must free this string.
 **/
char *
e_account_uid_from_xml (const char *xml)
{
	xmlNodePtr node;
	xmlDocPtr doc;
	char *uid = NULL;

	if (!(doc = xmlParseDoc ((char *)xml)))
		return NULL;

	node = doc->children;
	if (strcmp (node->name, "account") != 0) {
		xmlFreeDoc (doc);
		return NULL;
	}

	xml_set_prop (node, "uid", &uid);
	xmlFreeDoc (doc);

	return uid;
}

enum {
	EAP_IMAP_SUBSCRIBED = 0,
	EAP_IMAP_NAMESPACE,
	EAP_FILTER_INBOX,
	EAP_FILTER_JUNK,
	EAP_FORCE_SSL,
	EAP_LOCK_SIGNATURE,
	EAP_LOCK_AUTH,
	EAP_LOCK_AUTOCHECK,
	EAP_LOCK_DEFAULT_FOLDERS,
	EAP_LOCK_SAVE_PASSWD,
	EAP_LOCK_SOURCE,
	EAP_LOCK_TRANSPORT,
};

static struct _system_info {
	const char *key;
	guint32 perm;
} system_perms[] = {
	{ "imap_subscribed", 1<<EAP_IMAP_SUBSCRIBED },
	{ "imap_namespace", 1<<EAP_IMAP_NAMESPACE },
	{ "filter_inbox", 1<<EAP_FILTER_INBOX },
	{ "filter_junk", 1<<EAP_FILTER_JUNK },
	{ "ssl", 1<<EAP_FORCE_SSL },
	{ "signature", 1<<EAP_LOCK_SIGNATURE },
	{ "authtype", 1<<EAP_LOCK_AUTH },
	{ "autocheck", 1<<EAP_LOCK_AUTOCHECK },
	{ "default_folders", 1<<EAP_LOCK_DEFAULT_FOLDERS },
	{ "save_passwd" , 1<<EAP_LOCK_SAVE_PASSWD },
	{ "source", 1<<EAP_LOCK_SOURCE },
	{ "transport", 1<<EAP_LOCK_TRANSPORT },
};

#define TYPE_STRING (1)
#define TYPE_INT (2)
#define TYPE_BOOL (3)
#define TYPE_MASK (0xff)
#define TYPE_STRUCT (1<<8)

static struct _account_info {
	guint32 perms;
	guint32 type;
	unsigned int offset;
	unsigned int struct_offset;
} account_info[E_ACCOUNT_ITEM_LAST] = {
	{ /* E_ACCOUNT_NAME */ 0, TYPE_STRING, G_STRUCT_OFFSET(EAccount, name) },

	{ /* E_ACCOUNT_ID_NAME, */ 0, TYPE_STRING|TYPE_STRUCT, G_STRUCT_OFFSET(EAccount, id), G_STRUCT_OFFSET(EAccountIdentity, name) },
	{ /* E_ACCOUNT_ID_ADDRESS, */ 0, TYPE_STRING|TYPE_STRUCT, G_STRUCT_OFFSET(EAccount, id), G_STRUCT_OFFSET(EAccountIdentity, address) },
	{ /* E_ACCOUNT_ID_REPLY_TO, */ 0, TYPE_STRING|TYPE_STRUCT, G_STRUCT_OFFSET(EAccount, id), G_STRUCT_OFFSET(EAccountIdentity, reply_to) },
	{ /* E_ACCOUNT_ID_ORGANIZATION */ 0, TYPE_STRING|TYPE_STRUCT, G_STRUCT_OFFSET(EAccount, id), G_STRUCT_OFFSET(EAccountIdentity, organization) },
	{ /* E_ACCOUNT_ID_SIGNATURE */ 1<<EAP_LOCK_SIGNATURE, TYPE_STRING|TYPE_STRUCT, G_STRUCT_OFFSET(EAccount, id), G_STRUCT_OFFSET(EAccountIdentity, sig_uid) },

	{ /* E_ACCOUNT_SOURCE_URL */ 1<<EAP_LOCK_SOURCE, TYPE_STRING|TYPE_STRUCT, G_STRUCT_OFFSET(EAccount, source), G_STRUCT_OFFSET(EAccountService, url) },
	{ /* E_ACCOUNT_SOURCE_KEEP_ON_SERVER */ 0, TYPE_BOOL|TYPE_STRUCT, G_STRUCT_OFFSET(EAccount, source), G_STRUCT_OFFSET(EAccountService, keep_on_server) },
	{ /* E_ACCOUNT_SOURCE_AUTO_CHECK */ 1<<EAP_LOCK_AUTOCHECK, TYPE_BOOL|TYPE_STRUCT, G_STRUCT_OFFSET(EAccount, source), G_STRUCT_OFFSET(EAccountService, auto_check) },
	{ /* E_ACCOUNT_SOURCE_AUTO_CHECK_TIME */ 1<<EAP_LOCK_AUTOCHECK, TYPE_INT|TYPE_STRUCT, G_STRUCT_OFFSET(EAccount, source), G_STRUCT_OFFSET(EAccountService, auto_check_time) },
	{ /* E_ACCOUNT_SOURCE_SAVE_PASSWD */ 1<<EAP_LOCK_SAVE_PASSWD, TYPE_BOOL|TYPE_STRUCT, G_STRUCT_OFFSET(EAccount, source), G_STRUCT_OFFSET(EAccountService, save_passwd) },

	{ /* E_ACCOUNT_TRANSPORT_URL */ 1<<EAP_LOCK_TRANSPORT, TYPE_STRING|TYPE_STRUCT, G_STRUCT_OFFSET(EAccount, transport), G_STRUCT_OFFSET(EAccountService, url) },
	{ /* E_ACCOUNT_TRANSPORT_SAVE_PASSWD */ 1<<EAP_LOCK_SAVE_PASSWD, TYPE_BOOL|TYPE_STRUCT, G_STRUCT_OFFSET(EAccount, transport), G_STRUCT_OFFSET(EAccountService, save_passwd) },

	{ /* E_ACCOUNT_DRAFTS_FOLDER_URI */ 1<<EAP_LOCK_DEFAULT_FOLDERS, TYPE_STRING, G_STRUCT_OFFSET(EAccount, drafts_folder_uri) },
	{ /* E_ACCOUNT_SENT_FOLDER_URI */ 1<<EAP_LOCK_DEFAULT_FOLDERS, TYPE_STRING, G_STRUCT_OFFSET(EAccount, sent_folder_uri) },

	{ /* E_ACCOUNT_CC_ALWAYS */ 0, TYPE_BOOL, G_STRUCT_OFFSET(EAccount, always_cc) },
	{ /* E_ACCOUNT_CC_ADDRS */ 0, TYPE_STRING, G_STRUCT_OFFSET(EAccount, cc_addrs) },

	{ /* E_ACCOUNT_BCC_ALWAYS */ 0, TYPE_BOOL, G_STRUCT_OFFSET(EAccount, always_bcc) },
	{ /* E_ACCOUNT_BCC_ADDRS */ 0, TYPE_STRING, G_STRUCT_OFFSET(EAccount, bcc_addrs) },

	{ /* E_ACCOUNT_PGP_KEY */ 0, TYPE_STRING, G_STRUCT_OFFSET(EAccount, pgp_key) },
	{ /* E_ACCOUNT_PGP_ENCRYPT_TO_SELF */ 0, TYPE_BOOL, G_STRUCT_OFFSET(EAccount, pgp_encrypt_to_self) },
	{ /* E_ACCOUNT_PGP_ALWAYS_SIGN */ 0, TYPE_BOOL, G_STRUCT_OFFSET(EAccount, pgp_always_sign) },
	{ /* E_ACCOUNT_PGP_NO_IMIP_SIGN */ 0, TYPE_BOOL, G_STRUCT_OFFSET(EAccount, pgp_no_imip_sign) },
	{ /* E_ACCOUNT_PGP_ALWAYS_TRUST */ 0, TYPE_BOOL, G_STRUCT_OFFSET(EAccount, pgp_always_trust) },

	{ /* E_ACCOUNT_SMIME_SIGN_KEY */ 0, TYPE_STRING, G_STRUCT_OFFSET(EAccount, smime_sign_key) },
	{ /* E_ACCOUNT_SMIME_ENCRYPT_KEY */ 0, TYPE_STRING, G_STRUCT_OFFSET(EAccount, smime_encrypt_key) },
	{ /* E_ACCOUNT_SMIME_SIGN_DEFAULT */ 0, TYPE_BOOL, G_STRUCT_OFFSET(EAccount, smime_sign_default) },
	{ /* E_ACCOUNT_SMIME_ENCRYPT_TO_SELF */ 0, TYPE_BOOL, G_STRUCT_OFFSET(EAccount, smime_encrypt_to_self) },
	{ /* E_ACCOUNT_SMIME_ENCRYPT_DEFAULT */ 0, TYPE_BOOL, G_STRUCT_OFFSET(EAccount, smime_encrypt_default) },
};

static GHashTable *ea_option_table;
static GHashTable *ea_system_table;
static guint32 ea_perms;

static struct _option_info {
	char *key;
	guint32 perms;
} ea_option_list[] = {
	{ "imap_use_lsub", 1<<EAP_IMAP_SUBSCRIBED },
	{ "imap_override_namespace", 1<<EAP_IMAP_NAMESPACE },
	{ "imap_filter", 1<<EAP_FILTER_INBOX },
	{ "imap_filter_junk", 1<<EAP_FILTER_JUNK },
	{ "imap_filter_junk_inbox", 1<<EAP_FILTER_JUNK },
	{ "*_use_ssl", 1<<EAP_FORCE_SSL },
	{ "*_auth", 1<<EAP_LOCK_AUTH },
};

#define LOCK_BASE "/apps/evolution/lock/mail/accounts"

static void
ea_setting_notify(GConfClient *gconf, guint cnxn_id, GConfEntry *entry, void *crap)
{
	GConfValue *value;
	char *tkey;
	struct _system_info *info;

	g_return_if_fail (gconf_entry_get_key (entry) != NULL);
	
	if (!(value = gconf_entry_get_value (entry)))
		return;
	
	tkey = strrchr(entry->key, '/');
	g_return_if_fail (tkey != NULL);

	info = g_hash_table_lookup(ea_system_table, tkey+1);
	if (info) {
		if (gconf_value_get_bool(value))
			ea_perms |= info->perm;
		else
			ea_perms &= ~info->perm;
	}
}

static void
ea_setting_setup(void)
{
	GConfClient *gconf = gconf_client_get_default();
	GConfEntry *entry;
	GError *err = NULL;
	int i;
	char key[64];

	if (ea_option_table != NULL)
		return;

	ea_option_table = g_hash_table_new(g_str_hash, g_str_equal);
	for (i=0;i<sizeof(ea_option_list)/sizeof(ea_option_list[0]);i++)
		g_hash_table_insert(ea_option_table, ea_option_list[i].key, &ea_option_list[i]);

	gconf_client_add_dir(gconf, LOCK_BASE, GCONF_CLIENT_PRELOAD_NONE, NULL);

	ea_system_table = g_hash_table_new(g_str_hash, g_str_equal);
	for (i=0;i<sizeof(system_perms)/sizeof(system_perms[0]);i++) {
		g_hash_table_insert(ea_system_table, (char *)system_perms[i].key, &system_perms[i]);
		sprintf(key, LOCK_BASE "/%s", system_perms[i].key);
		entry = gconf_client_get_entry(gconf, key, NULL, TRUE, &err);
		if (entry)
			ea_setting_notify(gconf, 0, entry, NULL);
		gconf_entry_free(entry);
	}

	if (err) {
		g_warning("Could not load account lock settings: %s", err->message);
		g_error_free(err);
	}

	gconf_client_notify_add(gconf, LOCK_BASE, (GConfClientNotifyFunc)ea_setting_notify, NULL, NULL, NULL);
	g_object_unref(gconf);
}

/* look up the item in the structure or the substructure using our table of reflection data */
#define addr(ea, type) \
	((account_info[type].type & TYPE_STRUCT)? \
	(((char **)(((char *)ea)+account_info[type].offset))[0] + account_info[type].struct_offset): \
	(((char *)ea)+account_info[type].offset))
	
const char *e_account_get_string(EAccount *ea, e_account_item_t type)
{
	return *((const char **)addr(ea, type));
}

int e_account_get_int(EAccount *ea, e_account_item_t type)
{
	return *((int *)addr(ea, type));
}

gboolean e_account_get_bool(EAccount *ea, e_account_item_t type)
{
	return *((gboolean *)addr(ea, type));
}

static void
dump_account(EAccount *ea)
{
	char *xml;

	printf("Account changed\n");
	xml = e_account_to_xml(ea);
	printf(" ->\n%s\n", xml);
	g_free(xml);
}

/* TODO: should it return true if it changed? */
void e_account_set_string(EAccount *ea, e_account_item_t type, const char *val)
{
	char **p;

	if (!e_account_writable(ea, type)) {
		g_warning("Trying to set non-writable option account value");
	} else {
		p = (char **)addr(ea, type);
		printf("Setting string %d: old '%s' new '%s'\n", type, *p, val);
		if (*p != val
		    && (*p == NULL || val == NULL || strcmp(*p, val) != 0)) {
			g_free(*p);
			*p = g_strdup(val);

			dump_account(ea);
			g_signal_emit(ea, signals[CHANGED], 0, type);
		}
	}
}

void e_account_set_int(EAccount *ea, e_account_item_t type, int val)
{
	if (!e_account_writable(ea, type)) {
		g_warning("Trying to set non-writable option account value");
	} else {
		int *p = (int *)addr(ea, type);

		if (*p != val) {
			*p = val;
			dump_account(ea);
			g_signal_emit(ea, signals[CHANGED], 0, type);
		}
	}
}

void e_account_set_bool(EAccount *ea, e_account_item_t type, gboolean val)
{
	if (!e_account_writable(ea, type)) {
		g_warning("Trying to set non-writable option account value");
	} else {
		gboolean *p = (gboolean *)addr(ea, type);

		if (*p != val) {
			*p = val;
			dump_account(ea);
			g_signal_emit(ea, signals[CHANGED], 0, type);
		}
	}
}

gboolean
e_account_writable_option(EAccount *ea, const char *protocol, const char *option)
{
	char *key;
	struct _option_info *info;

	ea_setting_setup();

	key = alloca(strlen(protocol)+strlen(option)+2);
	sprintf(key, "%s_%s", protocol, option);

	info = g_hash_table_lookup(ea_option_table, key);
	if (info == NULL) {
		sprintf(key, "*_%s", option);
		info = g_hash_table_lookup(ea_option_table, key);
	}

	printf("checking writable option '%s' perms=%08x\n", option, info?info->perms:0);

	return info == NULL
		|| (info->perms & ea_perms) == 0;
}

gboolean
e_account_writable(EAccount *ea, e_account_item_t type)
{
	ea_setting_setup();

	return (account_info[type].perms & ea_perms) == 0;
}
