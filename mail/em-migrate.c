/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2003 Ximian, Inc. (www.ximian.com)
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <utime.h>
#include <unistd.h>
#include <dirent.h>
#include <regex.h>
#include <errno.h>
#include <ctype.h>

#include <gtk/gtk.h>

#include <gconf/gconf-client.h>

#include <camel/camel.h>
#include <camel/camel-session.h>
#include <camel/camel-file-utils.h>
#include <camel/camel-disco-folder.h>

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

#include <gal/util/e-util.h>
#include <gal/util/e-iconv.h>
#include <gal/util/e-xml-utils.h>

#include "e-util/e-bconf-map.h"
#include "e-util/e-account-list.h"
#include "e-util/e-signature-list.h"
#include "e-util/e-path.h"

#include "mail-config.h"
#include "em-utils.h"
#include "em-migrate.h"

#define d(x) x

/* upgrade helper functions */

static xmlNodePtr
xml_find_node (xmlNodePtr parent, const char *name)
{
	xmlNodePtr node;
	
	node = parent->children;
	while (node != NULL) {
		if (node->name && !strcmp (node->name, name))
			return node;
		
		node = node->next;
	}
	
	return NULL;
}

static int
upgrade_xml_uris (xmlDocPtr doc, char * (* upgrade_uri) (const char *uri))
{
	xmlNodePtr root, node;
	char *uri, *new;
	
	if (!doc || !(root = xmlDocGetRootElement (doc)))
		return 0;
	
	if (!root->name || strcmp (root->name, "filteroptions") != 0) {
		/* root node is not <filteroptions>, nothing to upgrade */
		return 0;
	}
	
	if (!(node = xml_find_node (root, "ruleset"))) {
		/* no ruleset node, nothing to upgrade */
		return 0;
	}
	
	node = node->children;
	while (node != NULL) {
		if (node->name && !strcmp (node->name, "rule")) {
			xmlNodePtr actionset, part, val, n;
			
			if ((actionset = xml_find_node (node, "actionset"))) {
				/* filters.xml */
				part = actionset->children;
				while (part != NULL) {
					if (part->name && !strcmp (part->name, "part")) {
						val = part->children;
						while (val != NULL) {
							if (val->name && !strcmp (val->name, "value")) {
								char *type;
								
								type = xmlGetProp (val, "type");
								if (type && !strcmp (type, "folder")) {
									if ((n = xml_find_node (val, "folder"))) {
										uri = xmlGetProp (n, "uri");
										new = upgrade_uri (uri);
										xmlFree (uri);
										
										xmlSetProp (n, "uri", new);
										g_free (new);
									}
								}
								
								xmlFree (type);
							}
							
							val = val->next;
						}
					}
					
					part = part->next;
				}
			} else if ((actionset = xml_find_node (node, "sources"))) {
				/* vfolders.xml */
				n = actionset->children;
				while (n != NULL) {
					if (n->name && !strcmp (n->name, "folder")) {
						uri = xmlGetProp (n, "uri");
						new = upgrade_uri (uri);
						xmlFree (uri);
						
						xmlSetProp (n, "uri", new);
						g_free (new);
					}
					
					n = n->next;
				}
			}
		}
		
		node = node->next;
	}
	
	return 0;
}

/* 1.0 upgrade functions & data */

/* as much info as we have on a given account */
struct _account_info_1_0 {
	char *name;
	char *uri;
	char *base_uri;
	union {
		struct {
			/* for imap */
			char *namespace;
			char *namespace_full;
			guint32 capabilities;
			GHashTable *folders;
			char dir_sep;
		} imap;
	} u;
};

struct _imap_folder_info_1_0 {
	char *folder;
	/* encoded?  decoded?  canonicalised? */
	char dir_sep;
};

static GHashTable *accounts_1_0 = NULL;
static GHashTable *accounts_name_1_0 = NULL;

static void
imap_folder_info_1_0_free(gpointer key, gpointer value, gpointer user_data)
{
	struct _imap_folder_info_1_0 *fi = value;
	
	g_free(fi->folder);
	g_free(fi);
}

static void
account_info_1_0_free (struct _account_info_1_0 *ai)
{
	g_free(ai->name);
	g_free(ai->uri);
	g_free(ai->base_uri);
	g_free(ai->u.imap.namespace);
	g_free(ai->u.imap.namespace_full);
	g_hash_table_foreach(ai->u.imap.folders, (GHFunc) imap_folder_info_1_0_free, NULL);
	g_hash_table_destroy(ai->u.imap.folders);
	g_free(ai);
}

static void
accounts_1_0_free(gpointer key, gpointer value, gpointer user_data)
{
	account_info_1_0_free(value);
}

static char *
get_base_uri(const char *val)
{
	const char *tmp;
	
	tmp = strchr(val, ':');
	if (tmp) {
		tmp++;
		if (strncmp(tmp, "//", 2) == 0)
			tmp += 2;
		tmp = strchr(tmp, '/');
	}
	
	if (tmp)
		return g_strndup(val, tmp-val);
	else
		return g_strdup(val);
}

static char *
upgrade_xml_uris_1_0 (const char *uri)
{
	char *out = NULL;
	
	/* upgrades camel uri's */
	if (strncmp (uri, "imap:", 5) == 0) {
		char *base_uri, dir_sep, *folder, *p;
		struct _account_info_1_0 *ai;
		
		/* add namespace, canonicalise dir_sep to / */
		base_uri = get_base_uri (uri);
		ai = g_hash_table_lookup (accounts_1_0, base_uri);
		
		if (ai == NULL) {
			g_free (base_uri);
			return NULL;
		}
		
		dir_sep = ai->u.imap.dir_sep;
		if (dir_sep == 0) {
			/* no dir_sep listed, try get it from the namespace, if set */
			if (ai->u.imap.namespace != NULL) {
				p = ai->u.imap.namespace;
				while ((dir_sep = *p++)) {
					if (dir_sep < '0'
					    || (dir_sep > '9' && dir_sep < 'A')
					    || (dir_sep > 'Z' && dir_sep < 'a')
					    || (dir_sep > 'z')) {
						break;
					}
					p++;
				}
			}
			
			/* give up ... */
			if (dir_sep == 0) {
				g_free (base_uri);
				return NULL;
			}
		}
		
		folder = g_strdup (uri + strlen (base_uri) + 1);
		
		/* Add the namespace before the mailbox name, unless the mailbox is INBOX */
		if (ai->u.imap.namespace && strcmp (folder, "INBOX") != 0)
			out = g_strdup_printf ("%s/%s/%s", base_uri, ai->u.imap.namespace, folder);
		else
			out = g_strdup_printf ("%s/%s", base_uri, folder);
		
		p = out;
		while (*p) {
			if (*p == dir_sep)
				*p = '/';
			p++;
		}
		
		g_free (folder);
		g_free (base_uri);
	} else if (strncmp (uri, "exchange:", 9) == 0) {
		char *base_uri, *folder, *p;
		
		/*  exchange://user@host/exchange/ * -> exchange://user@host/personal/ * */
		/*  Any url encoding (%xx) in the folder name is also removed */
		base_uri = get_base_uri (uri);
		uri += strlen (base_uri) + 1;
		if (strncmp (uri, "exchange/", 9) == 0) {
			folder = e_bconf_url_decode (uri + 9);
			p = strchr (folder, '/');
			out = g_strdup_printf ("%s/personal%s", base_uri, p ? p : "/");
			g_free (folder);
		}
	} else if (strncmp (uri, "exchanget:", 10) == 0) {
		/* these should be converted in the accounts table when it is loaded */
		g_warning ("exchanget: uri not converted: '%s'", uri);
	}
	
	return out;
}

static int
em_upgrade_xml_1_0 (xmlDocPtr doc)
{
	return upgrade_xml_uris (doc, upgrade_xml_uris_1_0);
}

static char *
parse_lsub (const char *lsub, char *dir_sep)
{
	static int comp;
	static regex_t pat;
	regmatch_t match[3];
	char *m = "^\\* LSUB \\([^)]*\\) \"?([^\" ]+)\"? \"?(.*)\"?$";
	
	if (!comp) {
		if (regcomp (&pat, m, REG_EXTENDED|REG_ICASE) == -1) {
			g_warning ("reg comp '%s' failed: %s", m, g_strerror (errno));
			return NULL;
		}
		comp = 1;
	}
	
	if (regexec (&pat, lsub, 3, match, 0) == 0) {
		if (match[1].rm_so != -1 && match[2].rm_so != -1) {
			if (dir_sep)
				*dir_sep = (match[1].rm_eo - match[1].rm_so == 1) ? lsub[match[1].rm_so] : 0;
			return g_strndup (lsub + match[2].rm_so, match[2].rm_eo - match[2].rm_so);
		}
	}
	
	return NULL;
}

static int
read_imap_storeinfo (struct _account_info_1_0 *si)
{
	FILE *storeinfo;
	guint32 tmp;
	char *buf, *folder, dir_sep, *path, *name, *p;
	struct _imap_folder_info_1_0 *fi;
	
	si->u.imap.folders = g_hash_table_new (g_str_hash, g_str_equal);
	
	/* get details from uri first */
	name = strstr (si->uri, ";override_namespace");
	if (name) {
		name = strstr (si->uri, ";namespace=");
		if (name) {
			char *end;
			
			name += strlen (";namespace=");
			if (*name == '\"') {
				name++;
				end = strchr (name, '\"');
			} else {
				end = strchr (name, ';');
			}
			
			if (end) {
				/* try get the dir_sep from the namespace */
				si->u.imap.namespace = g_strndup (name, end-name);
				
				p = si->u.imap.namespace;
				while ((dir_sep = *p++)) {
					if (dir_sep < '0'
					    || (dir_sep > '9' && dir_sep < 'A')
					    || (dir_sep > 'Z' && dir_sep < 'a')
					    || (dir_sep > 'z')) {
						si->u.imap.dir_sep = dir_sep;
						break;
					}
					p++;
				}
			}
		}
	}
	
	/* now load storeinfo if it exists */
	path = g_build_filename (g_get_home_dir (), "evolution", "mail", "imap", si->base_uri + 7, "storeinfo", NULL);
	storeinfo = fopen (path, "r");
	g_free (path);
	if (storeinfo == NULL) {
		g_warning ("could not find imap store info '%s'", path);
		return -1;
	}
	
	/* ignore version */
	camel_file_util_decode_uint32 (storeinfo, &tmp);
	camel_file_util_decode_uint32 (storeinfo, &si->u.imap.capabilities);
	g_free (si->u.imap.namespace);
	camel_file_util_decode_string (storeinfo, &si->u.imap.namespace);
	camel_file_util_decode_uint32 (storeinfo, &tmp);
	si->u.imap.dir_sep = tmp;
	/* strip trailing dir_sep or / */
	if (si->u.imap.namespace
	    && (si->u.imap.namespace[strlen (si->u.imap.namespace) - 1] == si->u.imap.dir_sep
		|| si->u.imap.namespace[strlen (si->u.imap.namespace) - 1] == '/')) {
		si->u.imap.namespace[strlen (si->u.imap.namespace) - 1] = 0;
	}
	
	d(printf ("namespace '%s' dir_sep '%c'\n", si->u.imap.namespace, si->u.imap.dir_sep ? si->u.imap.dir_sep : '?'));
	
	while (camel_file_util_decode_string (storeinfo, &buf) == 0) {
		folder = parse_lsub (buf, &dir_sep);
		if (folder) {
			fi = g_new0 (struct _imap_folder_info_1_0, 1);
			fi->folder = folder;
			fi->dir_sep = dir_sep;
#if d(!)0
			printf (" add folder '%s' ", folder);
			if (dir_sep)
				printf ("'%c'\n", dir_sep);
			else
				printf ("NIL\n");
#endif
			g_hash_table_insert (si->u.imap.folders, fi->folder, fi);
		} else {
			g_warning ("Could not parse LIST result '%s'\n", buf);
		}
	}
	
	fclose (storeinfo);
	
	return 0;
}

static int
load_accounts_1_0 (xmlDocPtr doc)
{
	xmlNodePtr source;
	char *val, *tmp;
	int count = 0, i;
	char key[32];
	
	if (!(source = e_bconf_get_path (doc, "/Mail/Accounts")))
		return 0;
	
	if ((val = e_bconf_get_value (source, "num"))) {
		count = atoi (val);
		xmlFree (val);
	}
	
	/* load account upgrade info for each account */
	for (i = 0; i < count; i++) {
		struct _account_info_1_0 *ai;
		char *rawuri;
		
		sprintf (key, "source_url_%d", i);
		if (!(rawuri = e_bconf_get_value (source, key)))
			continue;
		
		ai = g_malloc0 (sizeof (struct _account_info_1_0));
		ai->uri = e_bconf_hex_decode (rawuri);
		ai->base_uri = get_base_uri (ai->uri);
		sprintf (key, "account_name_%d", i);
		ai->name = e_bconf_get_string (source, key);
		
		d(printf("load account '%s'\n", ai->uri));
		
		if (!strncmp (ai->uri, "imap:", 5)) {
			read_imap_storeinfo (ai);
		} else if (!strncmp (ai->uri, "exchange:", 9)) {
			xmlNodePtr node;
			
			d(printf (" upgrade exchange account\n"));
			/* small hack, poke the source_url into the transport_url for exchanget: transports
			   - this will be picked up later in the conversion */
			sprintf (key, "transport_url_%d", i);
			node = e_bconf_get_entry (source, key);
			if (node && (val = xmlGetProp (node, "value"))) {
				tmp = e_bconf_hex_decode (val);
				xmlFree (val);
				if (strncmp (tmp, "exchanget:", 10) == 0)
					xmlSetProp (node, "value", rawuri);
				g_free (tmp);
			} else {
				d(printf (" couldn't find transport uri?\n"));
			}
		}
		xmlFree (rawuri);
		
		g_hash_table_insert (accounts_1_0, ai->base_uri, ai);
		if (ai->name)
			g_hash_table_insert (accounts_name_1_0, ai->name, ai);
	}
	
	return 0;
}

static int
em_migrate_1_0 (const char *evolution_dir, xmlDocPtr config_xmldb, xmlDocPtr filters, xmlDocPtr vfolders, CamelException *ex)
{
	accounts_1_0 = g_hash_table_new (g_str_hash, g_str_equal);
	accounts_name_1_0 = g_hash_table_new (g_str_hash, g_str_equal);	
	load_accounts_1_0 (config_xmldb);
	
	em_upgrade_xml_1_0 (filters);
	em_upgrade_xml_1_0 (vfolders);
	
	g_hash_table_foreach (accounts_1_0, (GHFunc) accounts_1_0_free, NULL);
	g_hash_table_destroy (accounts_1_0);
	g_hash_table_destroy (accounts_name_1_0);
	
	return 0;
}

/* 1.2 upgrade functions */
static int
is_xml1encoded (const char *txt)
{
	const unsigned char *p;
	int isxml1 = FALSE;
	int is8bit = FALSE;
	
	p = (const unsigned char *)txt;
	while (*p) {
		if (p[0] == '\\' && p[1] == 'U' && p[2] == '+'
		    && isxdigit (p[3]) && isxdigit (p[4]) && isxdigit (p[5]) && isxdigit (p[6])
		    && p[7] == '\\') {
			isxml1 = TRUE;
			p+=7;
		} else if (p[0] >= 0x80)
			is8bit = TRUE;
		p++;
	}
	
	/* check for invalid utf8 that needs cleaning */
	if (is8bit && !isxml1)
		isxml1 = !g_utf8_validate (txt, -1, NULL);
	
	return isxml1;
}

static char *
decode_xml1 (const char *txt)
{
	GString *out = g_string_new ("");
	const unsigned char *p;
	char *res;
	
	/* convert:
	   \U+XXXX\ -> utf8
	   8 bit characters -> utf8 (iso-8859-1) */
	
	p = (const unsigned char *) txt;
	while (*p) {
		if (p[0] > 0x80
		    || (p[0] == '\\' && p[1] == 'U' && p[2] == '+'
			&& isxdigit (p[3]) && isxdigit (p[4]) && isxdigit (p[5]) && isxdigit (p[6])
			&& p[7] == '\\')) {
			char utf8[8];
			gunichar u;
			
			if (p[0] == '\\') {
				memcpy (utf8, p + 3, 4);
				utf8[4] = 0;
				u = strtoul (utf8, NULL, 16);
				p+=7;
			} else
				u = p[0];
			utf8[g_unichar_to_utf8 (u, utf8)] = 0;
			g_string_append (out, utf8);
		} else {
			g_string_append_c (out, *p);
		}
		p++;
	}
	
	res = out->str;
	g_string_free (out, FALSE);
	
	return res;
}

static char *
utf8_reencode (const char *txt)
{
	GString *out = g_string_new ("");
	const unsigned char *p;
	char *res;
	
	/* convert:
        libxml1  8 bit utf8 converted to xml entities byte-by-byte chars -> utf8 */
	
	p =  (const unsigned char *) txt;
	
	while (*p) {
		g_string_append_c (out,(char) g_utf8_get_char (p));
		p = g_utf8_next_char (p);
	}
	
	res = out->str;
	if (g_utf8_validate (res, -1, NULL)) {
		g_string_free (out, FALSE);
		return res;
	} else {
		g_string_free (out, TRUE);
		return g_strdup (txt);
	}
}

static int
upgrade_xml_1_2_rec (xmlNodePtr node)
{
	const char *value_tags[] = { "string", "address", "regex", "file", "command", NULL };
	const char *rule_tags[] = { "title", NULL };
	const char *item_props[] = { "name", NULL };
	struct {
		const char *name;
		const char **tags;
		const char **props;
	} tags[] = {
		{ "value", value_tags, NULL },
		{ "rule", rule_tags, NULL },
		{ "item", NULL, item_props },
		{ 0 },
	};
	xmlNodePtr work;
	int i,j;
	char *txt, *tmp;
	
	/* upgrades the content of a node, if the node has a specific parent/node name */
	
	for (i = 0; tags[i].name; i++) {
		if (!strcmp (node->name, tags[i].name)) {
			if (tags[i].tags != NULL) {
				work = node->children;
				while (work) {
					for (j = 0; tags[i].tags[j]; j++) {
						if (!strcmp (work->name, tags[i].tags[j])) {
							txt = xmlNodeGetContent (work);
							if (is_xml1encoded (txt)) {
								tmp = decode_xml1 (txt);
								d(printf ("upgrading xml node %s/%s '%s' -> '%s'\n",
									  tags[i].name, tags[i].tags[j], txt, tmp));
								xmlNodeSetContent (work, tmp);
								g_free (tmp);
							}
							xmlFree (txt);
						}
					}
					work = work->next;
				}
				break;
			}
			
			if (tags[i].props != NULL) {
				for (j = 0; tags[i].props[j]; j++) {
					txt = xmlGetProp (node, tags[i].props[j]);
					tmp = utf8_reencode (txt);
					d(printf ("upgrading xml property %s on node %s '%s' -> '%s'\n",
						  tags[i].props[j], tags[i].name, txt, tmp));
					xmlSetProp (node, tags[i].props[j], tmp);
					g_free (tmp);
					xmlFree (txt);
				}
			}
		}
	}
	
	node = node->children;
	while (node) {
		upgrade_xml_1_2_rec (node);
		node = node->next;
	}
	
	return 0;
}

static int
em_upgrade_xml_1_2 (xmlDocPtr doc)
{
	xmlNodePtr root;
	
	if (!doc || !(root = xmlDocGetRootElement (doc)))
		return 0;
	
	return upgrade_xml_1_2_rec (root);
}

/* ********************************************************************** */
/*  Tables for converting flat bonobo conf -> gconf xml blob		  */
/* ********************************************************************** */

/* Mail/Accounts/ * */
static e_bconf_map_t cc_map[] = {
	{ "account_always_cc_%i", "always", E_BCONF_MAP_BOOL },
	{ "account_always_cc_addrs_%i", "recipients", E_BCONF_MAP_STRING|E_BCONF_MAP_CONTENT },
	{ NULL },
};

static e_bconf_map_t bcc_map[] = {
	{ "account_always_cc_%i", "always", E_BCONF_MAP_BOOL },
	{ "account_always_bcc_addrs_%i", "recipients", E_BCONF_MAP_STRING|E_BCONF_MAP_CONTENT },
	{ NULL },
};

static e_bconf_map_t pgp_map[] = {
	{ "account_pgp_encrypt_to_self_%i", "encrypt-to-self", E_BCONF_MAP_BOOL },
	{ "account_pgp_always_trust_%i", "always-trust", E_BCONF_MAP_BOOL },
	{ "account_pgp_always_sign_%i", "always-sign", E_BCONF_MAP_BOOL },
	{ "account_pgp_no_imip_sign_%i", "no-imip-sign", E_BCONF_MAP_BOOL },
	{ "account_pgp_key_%i", "key-id", E_BCONF_MAP_STRING|E_BCONF_MAP_CONTENT },
	{ NULL },
};

static e_bconf_map_t smime_map[] = {
	{ "account_smime_encrypt_to_self_%i", "encrypt-to-self", E_BCONF_MAP_BOOL },
	{ "account_smime_always_sign_%i", "always-sign", E_BCONF_MAP_BOOL },
	{ "account_smime_key_%i", "key-id", E_BCONF_MAP_STRING|E_BCONF_MAP_CONTENT },
	{ NULL },
};

static e_bconf_map_t identity_sig_map[] = {
	{ "identity_autogenerated_signature_%i", "auto", E_BCONF_MAP_BOOL },
	{ "identity_def_signature_%i", "default", E_BCONF_MAP_LONG },
	{ NULL },
};

static e_bconf_map_t identity_map[] = {
	{ "identity_name_%i", "name", E_BCONF_MAP_STRING|E_BCONF_MAP_CONTENT },
	{ "identity_address_%i", "addr-spec", E_BCONF_MAP_STRING|E_BCONF_MAP_CONTENT },
	{ "identity_reply_to_%i", "reply-to", E_BCONF_MAP_STRING|E_BCONF_MAP_CONTENT },
	{ "identity_organization_%i", "organization", E_BCONF_MAP_STRING|E_BCONF_MAP_CONTENT },
	{ NULL, "signature", E_BCONF_MAP_CHILD, identity_sig_map },
	{ NULL },
};

static e_bconf_map_t source_map[] = {
	{ "source_save_passwd_%i", "save-passwd", E_BCONF_MAP_BOOL },
	{ "source_keep_on_server_%i", "keep-on-server", E_BCONF_MAP_BOOL },
	{ "source_auto_check_%i", "auto-check", E_BCONF_MAP_BOOL },
	{ "source_auto_check_time_%i", "auto-check-timeout", E_BCONF_MAP_LONG },
	{ "source_url_%i", "url", E_BCONF_MAP_STRING|E_BCONF_MAP_CONTENT },
	{ NULL },
};

static e_bconf_map_t transport_map[] = {
	{ "transport_save_passwd_%i", "save-passwd", E_BCONF_MAP_BOOL },
	{ "transport_url_%i", "url", E_BCONF_MAP_STRING|E_BCONF_MAP_CONTENT },
	{ NULL },
};

static e_bconf_map_t account_map[] = {
	{ "account_name_%i", "name", E_BCONF_MAP_STRING },
	{ "source_enabled_%i", "enabled", E_BCONF_MAP_BOOL },
	{ NULL, "identity", E_BCONF_MAP_CHILD, identity_map },
	{ NULL, "source", E_BCONF_MAP_CHILD, source_map },
	{ NULL, "transport", E_BCONF_MAP_CHILD, transport_map },
	{ "account_drafts_folder_uri_%i", "drafts-folder", E_BCONF_MAP_STRING|E_BCONF_MAP_CONTENT },
	{ "account_sent_folder_uri_%i", "sent-folder", E_BCONF_MAP_STRING|E_BCONF_MAP_CONTENT },
	{ NULL, "auto-cc", E_BCONF_MAP_CHILD, cc_map },
	{ NULL, "auto-bcc", E_BCONF_MAP_CHILD, bcc_map },
	{ NULL, "pgp", E_BCONF_MAP_CHILD, pgp_map },
	{ NULL, "smime", E_BCONF_MAP_CHILD, smime_map },
	{ NULL },
};

/* /Mail/Signatures/ * */
static e_bconf_map_t signature_format_map[] = {
	{ "text/plain", },
	{ "text/html", },
	{ NULL }
};

static e_bconf_map_t signature_map[] = {
	{ "name_%i", "name", E_BCONF_MAP_STRING },
	{ "html_%i", "format", E_BCONF_MAP_ENUM, signature_format_map },
	{ "filename_%i", "filename", E_BCONF_MAP_STRING|E_BCONF_MAP_CONTENT },
	{ "script_%i", "script", E_BCONF_MAP_STRING|E_BCONF_MAP_CONTENT },
	{ NULL },
};

/* ********************************************************************** */
/*  Tables for bonobo conf -> gconf conversion				  */
/* ********************************************************************** */

static e_gconf_map_t mail_accounts_map[] = {
	/* /Mail/Accounts - most entries are processed via the xml blob routine */
	/* This also works because the initial uid mapping is 1:1 with the list order */
	{ "default_account", "mail/default_account", E_GCONF_MAP_SIMPLESTRING },
	{ 0 },
};

static e_gconf_map_t mail_display_map[] = {
	/* /Mail/Display */
	{ "thread_list", "mail/display/thread_list", E_GCONF_MAP_BOOL },
	{ "thread_subject", "mail/display/thread_subject", E_GCONF_MAP_BOOL },
	{ "hide_deleted", "mail/display/show_deleted", E_GCONF_MAP_BOOLNOT },
	{ "preview_pane", "mail/display/show_preview", E_GCONF_MAP_BOOL },
	{ "paned_size", "mail/display/paned_size", E_GCONF_MAP_INT },
	{ "seen_timeout", "mail/display/mark_seen_timeout", E_GCONF_MAP_INT },
	{ "do_seen_timeout", "mail/display/mark_seen", E_GCONF_MAP_BOOL },
	{ "http_images", "mail/display/load_http_images", E_GCONF_MAP_INT },
	{ "citation_highlight", "mail/display/mark_citations", E_GCONF_MAP_BOOL },
	{ "citation_color", "mail/display/citation_colour", E_GCONF_MAP_COLOUR },
	{ 0 },
};

static e_gconf_map_t mail_format_map[] = {
	/* /Mail/Format */
	{ "message_display_style", "mail/display/message_style", E_GCONF_MAP_INT },
	{ "send_html", "mail/composer/send_html", E_GCONF_MAP_BOOL },
	{ "default_reply_style", "mail/format/reply_style", E_GCONF_MAP_INT },
	{ "default_forward_style", "mail/format/forward_style", E_GCONF_MAP_INT },
	{ "default_charset", "mail/composer/charset", E_GCONF_MAP_STRING },
	{ "confirm_unwanted_html", "mail/prompts/unwanted_html", E_GCONF_MAP_BOOL },
	{ 0 },
};

static e_gconf_map_t mail_trash_map[] = {
	/* /Mail/Trash */
	{ "empty_on_exit", "mail/trash/empty_on_exit", E_GCONF_MAP_BOOL },
	{ 0 },
};

static e_gconf_map_t mail_prompts_map[] = {
	/* /Mail/Prompts */
	{ "confirm_expunge", "mail/prompts/expunge", E_GCONF_MAP_BOOL },
	{ "empty_subject", "mail/prompts/empty_subject", E_GCONF_MAP_BOOL },
	{ "only_bcc", "mail/prompts/only_bcc", E_GCONF_MAP_BOOL },
	{ 0 }
};

static e_gconf_map_t mail_filters_map[] = {
	/* /Mail/Filters */
	{ "log", "mail/filters/log", E_GCONF_MAP_BOOL },
	{ "log_path", "mail/filters/logfile", E_GCONF_MAP_STRING },
	{ 0 }
};

static e_gconf_map_t mail_notify_map[] = {
	/* /Mail/Notify */
	{ "new_mail_notification", "mail/notify/type", E_GCONF_MAP_INT },
	{ "new_mail_notification_sound_file", "mail/notify/sound", E_GCONF_MAP_STRING },
	{ 0 }
};

static e_gconf_map_t mail_filesel_map[] = {
	/* /Mail/Filesel */
	{ "last_filesel_dir", "mail/save_dir", E_GCONF_MAP_STRING },
	{ 0 }
};

static e_gconf_map_t mail_composer_map[] = {
	/* /Mail/Composer */
	{ "ViewFrom", "mail/composer/view/From", E_GCONF_MAP_BOOL },
	{ "ViewReplyTo", "mail/composer/view/ReplyTo", E_GCONF_MAP_BOOL },
	{ "ViewCC", "mail/composer/view/Cc", E_GCONF_MAP_BOOL },
	{ "ViewBCC", "mail/composer/view/Bcc", E_GCONF_MAP_BOOL },
	{ "ViewSubject", "mail/composer/view/Subject", E_GCONF_MAP_BOOL },
	{ 0 },
};

/* ********************************************************************** */

static e_gconf_map_t importer_elm_map[] = {
	/* /Importer/Elm */
	{ "mail", "importer/elm/mail", E_GCONF_MAP_BOOL },
	{ "mail-imported", "importer/elm/mail-imported", E_GCONF_MAP_BOOL },
	{ 0 },
};

static e_gconf_map_t importer_pine_map[] = {
	/* /Importer/Pine */
	{ "mail", "importer/elm/mail", E_GCONF_MAP_BOOL },
	{ "address", "importer/elm/address", E_GCONF_MAP_BOOL },
	{ 0 },
};

static e_gconf_map_t importer_netscape_map[] = {
	/* /Importer/Netscape */
	{ "mail", "importer/netscape/mail", E_GCONF_MAP_BOOL },
	{ "settings", "importer/netscape/settings", E_GCONF_MAP_BOOL },
	{ "filters", "importer/netscape/filters", E_GCONF_MAP_BOOL },
	{ 0 },
};

/* ********************************************************************** */

static e_gconf_map_list_t gconf_remap_list[] = {
	{ "/Mail/Accounts", mail_accounts_map },
	{ "/Mail/Display", mail_display_map },
	{ "/Mail/Format", mail_format_map },
	{ "/Mail/Trash", mail_trash_map },
	{ "/Mail/Prompts", mail_prompts_map },
	{ "/Mail/Filters", mail_filters_map },
	{ "/Mail/Notify", mail_notify_map },
	{ "/Mail/Filesel", mail_filesel_map },
	{ "/Mail/Composer", mail_composer_map },
	
	{ "/Importer/Elm", importer_elm_map },
	{ "/Importer/Pine", importer_pine_map },
	{ "/Importer/Netscape", importer_netscape_map },
	
	{ 0 },
};

struct {
	char *label;
	char *colour;
} label_default[5] = {
	{ N_("Important"), "#ff0000" },  /* red */
	{ N_("Work"),      "#ff8c00" },  /* orange */
	{ N_("Personal"),  "#008b00" },  /* forest green */
	{ N_("To Do"),     "#0000ff" },  /* blue */
	{ N_("Later"),     "#8b008b" }   /* magenta */
};

/* remaps mail config from bconf to gconf */
static int
bconf_import(GConfClient *gconf, xmlDocPtr config_xmldb)
{
	xmlNodePtr source;
	char labx[16], colx[16];
	char *val, *lab, *col;
	GSList *list, *l;
	int i;
	
	e_bconf_import(gconf, config_xmldb, gconf_remap_list);
	
	/* Labels:
	   label string + label colour as integer
	   -> label string:# colour as hex */
	source = e_bconf_get_path(config_xmldb, "/Mail/Labels");
	if (source) {
		list = NULL;
		for (i = 0; i < 5; i++) {
			sprintf(labx, "label_%d", i);
			sprintf(colx, "color_%d", i);
			lab = e_bconf_get_string(source, labx);
			if ((col = e_bconf_get_value(source, colx))) {
				sprintf(colx, "#%06x", atoi(col) & 0xffffff);
				g_free(col);
			} else
				strcpy(colx, label_default[i].colour);
			
			val = g_strdup_printf("%s:%s", lab ? lab : label_default[i].label, colx);
			list = g_slist_append(list, val);
			g_free(lab);
		}
		
		gconf_client_set_list(gconf, "/apps/evolution/mail/labels", GCONF_VALUE_STRING, list, NULL);
		while (list) {
			l = list->next;
			g_free(list->data);
			g_slist_free_1(list);
			list = l;
		}
	} else {
		g_warning("could not find /Mail/Labels in old config database, skipping");
	}
	
	/* Accounts: The flat bonobo-config structure is remapped to a list of xml blobs.  Upgrades as necessary */
	e_bconf_import_xml_blob(gconf, config_xmldb, account_map, "/Mail/Accounts",
				"/apps/evolution/mail/accounts", "account", "uid");
	
	/* Same for signatures */
	e_bconf_import_xml_blob(gconf, config_xmldb, signature_map, "/Mail/Signatures",
				"/apps/evolution/mail/signatures", "signature", NULL);
	
	return 0;
}

static int
em_migrate_1_2(const char *evolution_dir, xmlDocPtr config_xmldb, xmlDocPtr filters, xmlDocPtr vfolders, CamelException *ex)
{
	GConfClient *gconf;
	
	gconf = gconf_client_get_default();
	bconf_import(gconf, config_xmldb);
	g_object_unref(gconf);
	
	em_upgrade_xml_1_2(filters);
	em_upgrade_xml_1_2(vfolders);
	
	return 0;
}

/* 1.4 upgrade functions */

#define EM_MIGRATE_SESSION_TYPE     (em_migrate_session_get_type ())
#define EM_MIGRATE_SESSION(obj)     (CAMEL_CHECK_CAST((obj), EM_MIGRATE_SESSION_TYPE, EMMigrateSession))
#define EM_MIGRATE_SESSION_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), EM_MIGRATE_SESSION_TYPE, EMMigrateSessionClass))
#define EM_MIGRATE_IS_SESSION(o)    (CAMEL_CHECK_TYPE((o), EM_MIGRATE_SESSION_TYPE))

typedef struct _EMMigrateSession {
	CamelSession parent_object;
	
	CamelStore *store;   /* new folder tree store */
	char *srcdir;        /* old folder tree path */
} EMMigrateSession;

typedef struct _EMMigrateSessionClass {
	CamelSessionClass parent_class;
	
} EMMigrateSessionClass;

static CamelType em_migrate_session_get_type (void);
static CamelSession *em_migrate_session_new (const char *path);

static void
class_init (EMMigrateSessionClass *klass)
{
	;
}

static CamelType
em_migrate_session_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (
			camel_session_get_type (),
			"EMMigrateSession",
			sizeof (EMMigrateSession),
			sizeof (EMMigrateSessionClass),
			(CamelObjectClassInitFunc) class_init,
			NULL,
			NULL,
			NULL);
	}
	
	return type;
}

static CamelSession *
em_migrate_session_new (const char *path)
{
	CamelSession *session;
	
	session = CAMEL_SESSION (camel_object_new (EM_MIGRATE_SESSION_TYPE));
	
	camel_session_construct (session, path);
	
	return session;
}


static GtkWidget *window;
static GtkLabel *label;
static GtkProgressBar *progress;

static void
em_migrate_setup_progress_dialog (void)
{
	GtkWidget *vbox, *hbox, *w;
	
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title ((GtkWindow *) window, _("Migrating..."));
	gtk_window_set_modal ((GtkWindow *) window, TRUE);
	gtk_container_set_border_width ((GtkContainer *) window, 6);
	
	vbox = gtk_vbox_new (FALSE, 6);
	gtk_widget_show (vbox);
	gtk_container_add ((GtkContainer *) window, vbox);
	
	w = gtk_label_new (_("The location and hierarchy of the Evolution mailbox "
			     "folders has changed since Evolution 1.x.\n\nPlease be "
			     "patient while Evolution migrates your folders..."));
	gtk_label_set_line_wrap ((GtkLabel *) w, TRUE);
	gtk_widget_show (w);
	gtk_box_pack_start_defaults ((GtkBox *) vbox, w);
	
	hbox = gtk_hbox_new (FALSE, 6);
	gtk_widget_show (hbox);
	gtk_box_pack_start_defaults ((GtkBox *) vbox, hbox);
	
	label = (GtkLabel *) gtk_label_new ("");
	gtk_widget_show ((GtkWidget *) label);
	gtk_box_pack_start_defaults ((GtkBox *) hbox, (GtkWidget *) label);
	
	progress = (GtkProgressBar *) gtk_progress_bar_new ();
	gtk_widget_show ((GtkWidget *) progress);
	gtk_box_pack_start_defaults ((GtkBox *) hbox, (GtkWidget *) progress);
	
	gtk_widget_show (window);
}

static void
em_migrate_close_progress_dialog (void)
{
	gtk_widget_destroy ((GtkWidget *) window);
}

static void
em_migrate_set_folder_name (const char *folder_name)
{
	char *text;
	
	text = g_strdup_printf (_("Migrating `%s':"), folder_name);
	gtk_label_set_text (label, text);
	g_free (text);
	
	gtk_progress_bar_set_fraction (progress, 0.0);
	
	while (gtk_events_pending ())
		gtk_main_iteration ();
}

static void
em_migrate_set_progress (double percent)
{
	char text[5];
	
	snprintf (text, sizeof (text), "%d%%", (int) (percent * 100.0f));
	
	gtk_progress_bar_set_fraction (progress, percent);
	gtk_progress_bar_set_text (progress, text);
	
	while (gtk_events_pending ())
		gtk_main_iteration ();
}

static gboolean
is_mail_folder (const char *metadata)
{
	xmlNodePtr node;
	xmlDocPtr doc;
	char *type;
	
	if (!(doc = xmlParseFile (metadata))) {
		g_warning ("Cannot parse `%s'", metadata);
		return FALSE;
	}
	
	if (!(node = xmlDocGetRootElement (doc))) {
		g_warning ("`%s' corrupt: document contains no root node", metadata);
		xmlFreeDoc (doc);
		return FALSE;
	}
	
	if (!node->name || strcmp (node->name, "efolder") != 0) {
		g_warning ("`%s' corrupt: root node is not 'efolder'", metadata);
		xmlFreeDoc (doc);
		return FALSE;
	}
	
	node = node->children;
	while (node != NULL) {
		if (node->name && !strcmp (node->name, "type")) {
			type = xmlNodeGetContent (node);
			if (!strcmp (type, "mail")) {
				xmlFreeDoc (doc);
				xmlFree (type);
				
				return TRUE;
			}
			
			xmlFree (type);
			
			break;
		}
		
		node = node->next;
	}
	
	xmlFreeDoc (doc);
	
	return FALSE;
}

static int
get_local_et_expanded (const char *dirname)
{
	xmlNodePtr node;
	xmlDocPtr doc;
	struct stat st;
	char *buf, *p;
	int thread_list;
	
	buf = g_strdup_printf ("%s/evolution/config/file:%s", g_get_home_dir (), dirname);
	p = buf + strlen (g_get_home_dir ()) + strlen ("/evolution/config/file:");
	e_filename_make_safe (p);
	
	if (stat (buf, &st) == -1) {
		g_free (buf);
		return -1;
	}
	
	if (!(doc = xmlParseFile (buf))) {
		g_free (buf);
		return -1;
	}
	
	g_free (buf);
	
	if (!(node = xmlDocGetRootElement (doc)) || strcmp (node->name, "expanded_state") != 0) {
		xmlFreeDoc (doc);
		return -1;
	}
	
	if (!(buf = xmlGetProp (node, "default"))) {
		xmlFreeDoc (doc);
		return -1;
	}
	
	thread_list = strcmp (buf, "0") == 0 ? 0 : 1;
	xmlFree (buf);
	
	xmlFreeDoc (doc);
	
	return thread_list;
}

static char *
get_local_store_uri (const char *dirname, const char *metadata, char **namep, int *index, CamelException *ex)
{
	char *protocol, *name, *buf;
	struct stat st;
	xmlNodePtr node;
	xmlDocPtr doc;
	
	if (stat (metadata, &st) == -1 || !S_ISREG (st.st_mode)) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM, "`%s' is not a regular file", metadata);
		return NULL;
	}
	
	if (!(doc = xmlParseFile (metadata))) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM, "cannot parse `%s'", metadata);
		return NULL;
	}
	
	if (!(node = xmlDocGetRootElement (doc)) || strcmp (node->name, "folderinfo") != 0) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM, "`%s' is malformed", metadata);
		xmlFreeDoc (doc);
		return NULL;
	}
	
	node = node->children;
	while (node != NULL) {
		if (node->name && !strcmp (node->name, "folder")) {
			protocol = xmlGetProp (node, "type");
			name = xmlGetProp (node, "name");
			buf = xmlGetProp (node, "index");
			if (buf != NULL) {
				*index = atoi (buf);
				xmlFree (buf);
			} else {
				*index = 0;
			}
			
			xmlFreeDoc (doc);
			
			buf = g_strdup_printf ("%s:%s", protocol, dirname);
			xmlFree (protocol);
			
			*namep = g_strdup (name);
			xmlFree (name);
			
			return buf;
		}
		
		node = node->next;
	}
	
	xmlFreeDoc (doc);
	
	camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM, "`%s' does not contain needed info", metadata);
	
	return NULL;
}

static int
cp (const char *src, const char *dest, gboolean show_progress)
{
	unsigned char readbuf[65536];
	ssize_t nread, nwritten;
	int errnosav, readfd, writefd;
	size_t total = 0;
	struct stat st;
	struct utimbuf ut;
	
	/* if the dest file exists and has content, abort - we don't
	 * want to corrupt their existing data */
	if (stat (dest, &st) == 0 && st.st_size > 0)
		return -1;
	
	if (stat (src, &st) == -1)
		return -1;
	
	if ((readfd = open (src, O_RDONLY)) == -1)
		return -1;
	
	if ((writefd = open (dest, O_WRONLY | O_CREAT | O_TRUNC, 0666)) == -1) {
		errnosav = errno;
		close (readfd);
		errno = errnosav;
		return -1;
	}
	
	do {
		do {
			nread = read (readfd, readbuf, sizeof (readbuf));
		} while (nread == -1 && errno == EINTR);
		
		if (nread == 0)
			break;
		else if (nread < 0)
			goto exception;
		
		do {
			nwritten = write (writefd, readbuf, nread);
		} while (nwritten == -1 && errno == EINTR);
		
		if (nwritten < nread)
			goto exception;
		
		total += nwritten;
		
		if (show_progress)
			em_migrate_set_progress (((double) total) / ((double) st.st_size));
	} while (total < st.st_size);
	
	if (fsync (writefd) == -1)
		goto exception;
	
	close (readfd);
	if (close (writefd) == -1)
		goto failclose;
	
	ut.actime = st.st_atime;
	ut.modtime = st.st_mtime;
	utime (dest, &ut);
	chmod (dest, st.st_mode);
	
	return 0;
	
 exception:
	
	errnosav = errno;
	close (readfd);
	close (writefd);
	errno = errnosav;
	
 failclose:
	
	errnosav = errno;
	unlink (dest);
	errno = errnosav;
	
	return -1;
}

static int
cp_r (const char *src, const char *dest, const char *pattern)
{
	GString *srcpath, *destpath;
	struct dirent *dent;
	size_t slen, dlen;
	struct stat st;
	DIR *dir;
	
	if (camel_mkdir (dest, 0777) == -1)
		return -1;
	
	if (!(dir = opendir (src)))
		return -1;
	
	srcpath = g_string_new (src);
	g_string_append_c (srcpath, '/');
	slen = srcpath->len;
	
	destpath = g_string_new (dest);
	g_string_append_c (destpath, '/');
	dlen = destpath->len;
	
	while ((dent = readdir (dir))) {
		if (!strcmp (dent->d_name, ".") || !strcmp (dent->d_name, ".."))
			continue;
		
		g_string_truncate (srcpath, slen);
		g_string_truncate (destpath, dlen);
		
		g_string_append (srcpath, dent->d_name);
		g_string_append (destpath, dent->d_name);
		
		if (stat (srcpath->str, &st) == -1)
			continue;
		
		if (S_ISDIR (st.st_mode)) {
			cp_r (srcpath->str, destpath->str, pattern);
		} else if (!pattern || !strcmp (dent->d_name, pattern)) {
			cp (srcpath->str, destpath->str, FALSE);
		}
	}
	
	closedir (dir);
	
	g_string_free (destpath, TRUE);
	g_string_free (srcpath, TRUE);
	
	return 0;
}

static GString *
mbox_build_filename (const char *toplevel_dir, const char *full_name)
{
	const char *start, *inptr = full_name;
	int subdirs = 0;
	GString *path;
	
	while (*inptr != '\0') {
		if (*inptr == '/')
			subdirs++;
		inptr++;
	}
	
	path = g_string_new (toplevel_dir);
	g_string_append_c (path, '/');
	
	inptr = full_name;
	while (*inptr != '\0') {
		start = inptr;
		while (*inptr != '/' && *inptr != '\0')
			inptr++;
		
		g_string_append_len (path, start, inptr - start);
		
		if (*inptr == '/') {
			g_string_append (path, ".sbd/");
			inptr++;
			
			/* strip extranaeous '/'s */
			while (*inptr == '/')
				inptr++;
		}
	}
	
	return path;
}

#define mbox_store_build_filename(s,n) mbox_build_filename (((CamelService *) s)->url->path, n)

static void
em_migrate_dir (EMMigrateSession *session, const char *dirname, const char *full_name)
{
	guint32 flags = CAMEL_STORE_FOLDER_CREATE;
	CamelFolder *old_folder, *new_folder;
	CamelStore *local_store;
	struct dirent *dent;
	CamelException ex;
	char *path, *name, *uri;
	GPtrArray *uids;
	struct stat st;
	int thread_list;
	int index, i;
	DIR *dir;
	
	path = g_strdup_printf ("%s/folder-metadata.xml", dirname);
	if (stat (path, &st) == -1 || !S_ISREG (st.st_mode)) {
		g_free (path);
		return;
	}
	
	if (!is_mail_folder (path)) {
		g_free (path);
		goto try_subdirs;
	}
	
	g_free (path);
	
	camel_exception_init (&ex);
	
	/* get old store & folder */
	path = g_strdup_printf ("%s/local-metadata.xml", dirname);
	if (!(uri = get_local_store_uri (dirname, path, &name, &index, &ex))) {
		g_warning ("error opening old store for `%s': %s", full_name, ex.desc);
		camel_exception_clear (&ex);
		g_free (path);
		
		/* try subfolders anyway? */
		goto try_subdirs;
	}
	
	g_free (path);
	
	em_migrate_set_folder_name (full_name);
	
	thread_list = get_local_et_expanded (dirname);
	
	if (!strncmp (uri, "mbox:", 5)) {
		static char *ibex_ext[] = { ".ibex.index", ".ibex.index.data" };
		GString *src, *dest;
		size_t slen, dlen;
		FILE *fp;
		char *p;
		int i;
		
		src = g_string_new ("");
		g_string_append_printf (src, "%s/%s", uri + 5, name);
		g_free (name);
		
		dest = mbox_store_build_filename (session->store, full_name);
		p = strrchr (dest->str, '/');
		*p = '\0';
		
		slen = src->len;
		dlen = dest->len;
		
		if (camel_mkdir (dest->str, 0777) == -1 && errno != EEXIST) {
			g_string_free (dest, TRUE);
			g_string_free (src, TRUE);
			goto try_subdirs;
		}
		
		*p = '/';
		if (cp (src->str, dest->str, TRUE) == -1) {
			g_string_free (dest, TRUE);
			g_string_free (src, TRUE);
			goto try_subdirs;
		}
		
		g_string_append (src, ".ev-summary");
		g_string_append (dest, ".ev-summary");
		cp (src->str, dest->str, FALSE);
		
		/* create a .cmeta file specifying to index and/or thread the folder */
		g_string_truncate (dest, dlen);
		g_string_append (dest, ".cmeta");
		if ((fp = fopen (dest->str, "w")) != NULL) {
			int fd = fileno (fp);
			
			/* write the magic string */
			if (fwrite ("CLMD", 4, 1, fp) != 1)
				goto cmeta_err;
			
			/* write the version (1) */
			if (camel_file_util_encode_uint32 (fp, 1) == -1)
				goto cmeta_err;
			
			/* write the meta count */
			if (camel_file_util_encode_uint32 (fp, thread_list != -1 ? 1 : 0) == -1)
				goto cmeta_err;
			
			if (thread_list != -1) {
				if (camel_file_util_encode_string (fp, "evolution:thread_list") == -1)
					goto cmeta_err;
				
				if (camel_file_util_encode_string (fp, thread_list ? "1" : "0") == -1)
					goto cmeta_err;
			}
			
			/* write the prop count (only prop is the index prop) */
			if (camel_file_util_encode_uint32 (fp, 1) == -1)
				goto cmeta_err;
			
			/* write the index prop tag (== CAMEL_FOLDER_ARG_LAST|CAMEL_ARG_BOO) */
			if (camel_file_util_encode_uint32 (fp, CAMEL_FOLDER_ARG_LAST|CAMEL_ARG_BOO) == -1)
				goto cmeta_err;
			
			/* write the index prop value */
			if (camel_file_util_encode_uint32 (fp, 1) == -1)
				goto cmeta_err;
			
			fflush (fp);
			
			if (fsync (fd) == -1) {
			cmeta_err:
				fclose (fp);
				unlink (dest->str);
			} else {
				fclose (fp);
			}
		}
		
		/* copy over the ibex files */
		for (i = 0; i < 2; i++) {
			g_string_truncate (src, slen);
			g_string_truncate (dest, dlen);
			
			g_string_append (src, ibex_ext[i]);
			g_string_append (dest, ibex_ext[i]);
			cp (src->str, dest->str, FALSE);
		}
		
		g_string_free (dest, TRUE);
		g_string_free (src, TRUE);
	} else {
		if (!(local_store = camel_session_get_store ((CamelSession *) session, uri, &ex))) {
			g_warning ("error opening old store for `%s': %s", full_name, ex.desc);
			camel_exception_clear (&ex);
			g_free (name);
			g_free (uri);
			
			/* try subfolders anyway? */
			goto try_subdirs;
		}
		
		if (!(old_folder = camel_store_get_folder (local_store, name, 0, &ex))) {
			g_warning ("error opening old folder `%s': %s", full_name, ex.desc);
			camel_object_unref (local_store);
			camel_exception_clear (&ex);
			g_free (name);
			
			/* try subfolders anyway? */
			goto try_subdirs;
		}
		
		g_free (name);
		
		flags |= (index ? CAMEL_STORE_FOLDER_BODY_INDEX : 0);
		if (!(new_folder = camel_store_get_folder (session->store, full_name, flags, &ex))) {
			g_warning ("error creating new mbox folder `%s': %s", full_name, ex.desc);
			camel_object_unref (local_store);
			camel_object_unref (old_folder);
			camel_exception_clear (&ex);
			
			/* try subfolders anyway? */
			goto try_subdirs;
		}
		
		if (thread_list != -1) {
			camel_object_meta_set (new_folder, "evolution:thread_list", thread_list ? "1" : "0");
			camel_object_state_write (new_folder);
		}
		
		uids = camel_folder_get_uids (old_folder);
		for (i = 0; i < uids->len; i++) {
			CamelMimeMessage *message;
			CamelMessageInfo *info;
			
			if (!(info = camel_folder_get_message_info (old_folder, uids->pdata[i])))
				continue;
			
			if (!(message = camel_folder_get_message (old_folder, uids->pdata[i], &ex))) {
				camel_folder_free_message_info (old_folder, info);
				break;
			}
			
			camel_folder_append_message (new_folder, message, info, NULL, &ex);
			camel_folder_free_message_info (old_folder, info);
			camel_object_unref (message);
			
			if (camel_exception_is_set (&ex))
				break;
			
			em_migrate_set_progress (((double) i + 1) / ((double) uids->len));
		}
		
		camel_folder_free_uids (old_folder, uids);
		
		if (camel_exception_is_set (&ex)) {
			g_warning ("error migrating folder `%s': %s", full_name, ex.desc);
			camel_object_unref (local_store);
			camel_object_unref (old_folder);
			camel_object_unref (new_folder);
			camel_exception_clear (&ex);
			
			/* try subfolders anyway? */
			goto try_subdirs;
		}
		
		/*camel_object_unref (local_store);*/
		camel_object_unref (old_folder);
		camel_object_unref (new_folder);
	}
	
 try_subdirs:
	
	path = g_strdup_printf ("%s/subfolders", dirname);
	if (stat (path, &st) == -1 || !S_ISDIR (st.st_mode)) {
		g_free (path);
		return;
	}
	
	if (!(dir = opendir (path))) {
		g_warning ("cannot open `%s': %s", path, strerror (errno));
		g_free (path);
		return;
	}
	
	while ((dent = readdir (dir))) {
		char *full_path;
		
		if (dent->d_name[0] == '.')
			continue;
		
		full_path = g_strdup_printf ("%s/%s", path, dent->d_name);
		if (stat (full_path, &st) == -1 || !S_ISDIR (st.st_mode)) {
			g_free (full_path);
			continue;
		}
		
		name = g_strdup_printf ("%s/%s", full_name, dent->d_name);
		em_migrate_dir (session, full_path, name);
		g_free (full_path);
		g_free (name);
	}
	
	closedir (dir);
	
	g_free (path);
}

static void
em_migrate_local_folders_1_4 (EMMigrateSession *session)
{
	struct dirent *dent;
	struct stat st;
	DIR *dir;
	
	if (!(dir = opendir (session->srcdir))) {
		g_warning ("cannot open `%s': %s", session->srcdir, strerror (errno));
		return;
	}
	
	em_migrate_setup_progress_dialog ();
	
	while ((dent = readdir (dir))) {
		char *full_path;
		
		if (dent->d_name[0] == '.')
			continue;
		
		full_path = g_strdup_printf ("%s/%s", session->srcdir, dent->d_name);
		if (stat (full_path, &st) == -1 || !S_ISDIR (st.st_mode)) {
			g_free (full_path);
			continue;
		}
		
		em_migrate_dir (session, full_path, dent->d_name);
		g_free (full_path);
	}
	
	closedir (dir);
	
	em_migrate_close_progress_dialog ();
}

static char *
upgrade_xml_uris_1_4 (const char *uri)
{
	char *path, *prefix, *p;
	CamelURL *url;
	
	if (!strncmp (uri, "file:", 5)) {
		url = camel_url_new (uri, NULL);
		camel_url_set_protocol (url, "email");
		camel_url_set_user (url, "local");
		camel_url_set_host (url, "local");
		
		prefix = g_build_filename (g_get_home_dir (), "evolution", "local", NULL);
		if (strncmp (url->path, prefix, strlen (prefix)) != 0) {
			/* uri is busticated - user probably copied from another user's home directory */
			camel_url_free (url);
			g_free (prefix);
			
			return g_strdup (uri);
		}
		path = g_strdup (url->path + strlen (prefix));
		g_free (prefix);
		
		/* modify the path in-place */
		p = path + strlen (path) - 12;
		while (p > path) {
			if (!strncmp (p, "/subfolders/", 12))
				memmove (p, p + 11, strlen (p + 11) + 1);
			
			p--;
		}
		
		camel_url_set_path (url, path);
		g_free (path);
		
		path = camel_url_to_string (url, 0);
		camel_url_free (url);
		
		return path;
	} else {
		return em_uri_from_camel (uri);
	}
}

static void
upgrade_vfolder_sources_1_4 (xmlDocPtr doc)
{
	xmlNodePtr root, node;
	
	if (!doc || !(root = xmlDocGetRootElement (doc)))
		return;
	
	if (!root->name || strcmp (root->name, "filteroptions") != 0) {
		/* root node is not <filteroptions>, nothing to upgrade */
		return;
	}
	
	if (!(node = xml_find_node (root, "ruleset"))) {
		/* no ruleset node, nothing to upgrade */
		return;
	}
	
	node = node->children;
	while (node != NULL) {
		if (node->name && !strcmp (node->name, "rule")) {
			xmlNodePtr sources;
			char *src;
			
			if (!(src = xmlGetProp (node, "source")))
				src = xmlStrdup ("local");  /* default to all local folders? */
			
			xmlSetProp (node, "source", "incoming");
			
			if (!(sources = xml_find_node (node, "sources")))
				sources = xmlNewChild (node, NULL, "sources", NULL);
			
			xmlSetProp (sources, "with", src);
			xmlFree (src);
		}
		
		node = node->next;
	}
}

static int
em_upgrade_xml_1_4 (xmlDocPtr doc, gboolean vfolders_xml)
{
	if (vfolders_xml)
		upgrade_vfolder_sources_1_4 (doc);
	
	return upgrade_xml_uris (doc, upgrade_xml_uris_1_4);
}

static char *
get_nth_sig (int id)
{
	ESignatureList *list;
	ESignature *sig;
	EIterator *iter;
	char *uid = NULL;
	int i = 0;
	
	list = mail_config_get_signatures ();
	iter = e_list_get_iterator ((EList *) list);
	
	while (e_iterator_is_valid (iter) && i < id) {
		e_iterator_next (iter);
		i++;
	}
	
	if (i == id && e_iterator_is_valid (iter)) {
		sig = (ESignature *) e_iterator_get (iter);
		uid = g_strdup (sig->uid);
	}
	
	g_object_unref (iter);
	
	return uid;
}

static int
em_upgrade_accounts_1_4 (void)
{
	EAccountList *accounts;
	EIterator *iter;
	
	if (!(accounts = mail_config_get_accounts ()))
		return 0;
	
	iter = e_list_get_iterator ((EList *) accounts);
	while (e_iterator_is_valid (iter)) {
		EAccount *account = (EAccount *) e_iterator_get (iter);
		char *url;
		
		if (account->drafts_folder_uri) {
			url = upgrade_xml_uris_1_4 (account->drafts_folder_uri);
			g_free (account->drafts_folder_uri);
			account->drafts_folder_uri = url;
		}
		
		if (account->sent_folder_uri) {
			url = upgrade_xml_uris_1_4 (account->sent_folder_uri);
			g_free (account->sent_folder_uri);
			account->sent_folder_uri = url;
		}
		
		if (account->id->sig_uid && !strncmp (account->id->sig_uid, "::", 2)) {
			int sig_id;
			
			sig_id = strtol (account->id->sig_uid + 2, NULL, 10);
			g_free (account->id->sig_uid);
			account->id->sig_uid = get_nth_sig (sig_id);
		}
		
		e_iterator_next (iter);
	}
	
	g_object_unref (iter);
	
	mail_config_save_accounts ();
	
	return 0;
}

static int
em_migrate_pop_uid_caches_1_4 (const char *evolution_dir, CamelException *ex)
{
	GString *oldpath, *newpath;
	struct dirent *dent;
	size_t olen, nlen;
	char *cache_dir;
	DIR *dir;
	
	/* open the old cache dir */
	cache_dir = g_build_filename (g_get_home_dir (), "evolution", "mail", "pop3", NULL);
	if (!(dir = opendir (cache_dir))) {
		if (errno == ENOENT) {
			g_free(cache_dir);
			return 0;
		}
		
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Failed to migrate pop3 uid caches: %s"),
				      g_strerror (errno));
		g_warning ("cannot open `%s': %s", cache_dir, strerror (errno));
		g_free (cache_dir);
		return -1;
	}
	
	oldpath = g_string_new (cache_dir);
	g_string_append_c (oldpath, '/');
	olen = oldpath->len;
	g_free (cache_dir);
	
	cache_dir = g_build_filename (evolution_dir, "mail", "pop", NULL);
	if (camel_mkdir (cache_dir, 0777) == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Failed to migrate pop3 uid caches: %s"),
				      g_strerror (errno));
		g_warning ("cannot create `%s': %s", cache_dir, strerror (errno));
		g_string_free (oldpath, TRUE);
		g_free (cache_dir);
		closedir (dir);
		return -1;
	}
	
	newpath = g_string_new (cache_dir);
	g_string_append_c (newpath, '/');
	nlen = newpath->len;
	g_free (cache_dir);
	
	while ((dent = readdir (dir))) {
		if (strncmp (dent->d_name, "cache-pop:__", 12) != 0)
			continue;
		
		g_string_truncate (oldpath, olen);
		g_string_truncate (newpath, nlen);
		
		g_string_append (oldpath, dent->d_name);
		g_string_append (newpath, dent->d_name + 12);
		
		/* strip the trailing '_' */
		g_string_truncate (newpath, newpath->len - 1);
		
		if (camel_mkdir (newpath->str, 0777) == -1) {
			g_warning ("cannot create `%s': %s", newpath->str, strerror (errno));
			continue;
		}
		
		g_string_append (newpath, "/uid-cache");
		cp (oldpath->str, newpath->str, FALSE);
	}
	
	g_string_free (oldpath, TRUE);
	g_string_free (newpath, TRUE);
	
	closedir (dir);
	
	return 0;
}

static int
em_migrate_imap_caches_1_4 (const char *evolution_dir, CamelException *ex)
{
	char *src, *dest;
	struct stat st;
	
	src = g_build_filename (g_get_home_dir (), "evolution", "mail", "imap", NULL);
	if (stat (src, &st) == -1 || !S_ISDIR (st.st_mode)) {
		g_free (src);
		return 0;
	}
	
	dest = g_build_filename (evolution_dir, "mail", "imap", NULL);
	
	/* we don't care if this fails, it's only a cache... */
	cp_r (src, dest, "summary");
	
	g_free (dest);
	g_free (src);
	
	return 0;
}

static int
em_migrate_folder_expand_state_1_4 (const char *evolution_dir, CamelException *ex)
{
	GString *srcpath, *destpath;
	size_t slen, dlen, rlen;
	char *evo14_mbox_root;
	struct dirent *dent;
	struct stat st;
	DIR *dir;
	
	srcpath = g_string_new (g_get_home_dir ());
	g_string_append (srcpath, "/evolution/config");
	if (stat (srcpath->str, &st) == -1 || !S_ISDIR (st.st_mode)) {
		g_string_free (srcpath, TRUE);
		return 0;
	}
	
	destpath = g_string_new (evolution_dir);
	g_string_append (destpath, "/mail/config");
	if (camel_mkdir (destpath->str, 0777) == -1 || !(dir = opendir (srcpath->str))) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Failed to migrate folder expand state: %s"), g_strerror (errno));
		g_warning ("Failed to migrate folder expand state: %s", g_strerror (errno));
		g_string_free (destpath, TRUE);
		g_string_free (srcpath, TRUE);
		return -1;
	}
	
	g_string_append (srcpath, "/et-expanded-");
	slen = srcpath->len;
	g_string_append (destpath, "/et-expanded-");
	dlen = destpath->len;
	
	evo14_mbox_root = g_build_filename (g_get_home_dir (), "evolution", "local", NULL);
	e_filename_make_safe (evo14_mbox_root);
	rlen = strlen (evo14_mbox_root);
	evo14_mbox_root = g_realloc (evo14_mbox_root, rlen + 2);
	evo14_mbox_root[rlen++] = '_';
	evo14_mbox_root[rlen] = '\0';
	
	while ((dent = readdir (dir))) {
		char *full_name, *inptr, *buf = NULL;
		const char *filename;
		GString *new;
		
		if (strncmp (dent->d_name, "et-expanded-", 12) != 0)
			continue;
		
		if (!strncmp (dent->d_name + 12, "file:", 5)) {
			/* need to munge the filename */
			inptr = dent->d_name + 17;
			
			if (!strncmp (inptr, evo14_mbox_root, rlen)) {
				/* this should always be the case afaik... */
				inptr += rlen;
				new = g_string_new ("mbox:");
				g_string_append_printf (new, "%s/mail/local#", evolution_dir);
				
				full_name = g_strdup (inptr);
				inptr = full_name + strlen (full_name) - 12;
				while (inptr > full_name) {
					if (!strncmp (inptr, "_subfolders_", 12))
						memmove (inptr, inptr + 11, strlen (inptr + 11) + 1);
					
					inptr--;
				}
				
				g_string_append (new, full_name);
				g_free (full_name);
				
				filename = buf = new->str;
				g_string_free (new, FALSE);
				e_filename_make_safe (buf);
			} else {
				/* but just in case... */
				filename = dent->d_name + 12;
			}
		} else {
			/* no munging needed */
			filename = dent->d_name + 12;
		}
		
		g_string_append (srcpath, dent->d_name + 12);
		g_string_append (destpath, filename);
		g_free (buf);
		
		cp (srcpath->str, destpath->str, FALSE);
		
		g_string_truncate (srcpath, slen);
		g_string_truncate (destpath, dlen);
	}
	
	closedir (dir);
	
	g_free (evo14_mbox_root);
	g_string_free (destpath, TRUE);
	g_string_free (srcpath, TRUE);
	
	return 0;
}

static int
em_migrate_folder_view_settings_1_4 (const char *evolution_dir, CamelException *ex)
{
	GString *srcpath, *destpath;
	size_t slen, dlen, rlen;
	char *evo14_mbox_root;
	struct dirent *dent;
	struct stat st;
	DIR *dir;
	
	srcpath = g_string_new (g_get_home_dir ());
	g_string_append (srcpath, "/evolution/views/mail");
	if (stat (srcpath->str, &st) == -1 || !S_ISDIR (st.st_mode)) {
		g_string_free (srcpath, TRUE);
		return 0;
	}
	
	destpath = g_string_new (evolution_dir);
	g_string_append (destpath, "/mail/views");
	if (camel_mkdir (destpath->str, 0777) == -1 || !(dir = opendir (srcpath->str))) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Failed to migrate folder expand state: %s"), g_strerror (errno));
		g_warning ("Failed to migrate folder expand state: %s", g_strerror (errno));
		g_string_free (destpath, TRUE);
		g_string_free (srcpath, TRUE);
		return -1;
	}
	
	g_string_append_c (srcpath, '/');
	slen = srcpath->len;
	g_string_append_c (destpath, '/');
	dlen = destpath->len;
	
	evo14_mbox_root = g_build_filename (g_get_home_dir (), "evolution", "local", NULL);
	e_filename_make_safe (evo14_mbox_root);
	rlen = strlen (evo14_mbox_root);
	evo14_mbox_root = g_realloc (evo14_mbox_root, rlen + 2);
	evo14_mbox_root[rlen++] = '_';
	evo14_mbox_root[rlen] = '\0';
	
	while ((dent = readdir (dir))) {
		char *full_name, *inptr, *buf = NULL;
		const char *filename, *ext;
		size_t prelen = 0;
		GString *new;
		
		if (dent->d_name[0] == '.')
			continue;
		
		if (!(ext = strrchr (dent->d_name, '.')))
			continue;
		
		if (!strcmp (ext, ".galview") || !strcmp (dent->d_name, "galview.xml")) {
			/* just copy the file */
			filename = dent->d_name;
			goto copy;
		} else if (strcmp (ext, ".xml") != 0) {
			continue;
		}
		
		if (!strncmp (dent->d_name, "current_view-", 13)) {
			prelen = 13;
		} else if (!strncmp (dent->d_name, "custom_view-", 12)) {
			prelen = 12;
		} else {
			/* huh? wtf is this file? */
			continue;
		}
		
		if (!strncmp (dent->d_name + prelen, "file:", 5)) {
			/* need to munge the filename */
			inptr = dent->d_name + prelen + 5;
			
			if (!strncmp (inptr, evo14_mbox_root, rlen)) {
				/* this should always be the case afaik... */
				inptr += rlen;
				new = g_string_new ("mbox:");
				g_string_append_printf (new, "%s/mail/local#", evolution_dir);
				
				full_name = g_strdup (inptr);
				inptr = full_name + strlen (full_name) - 12;
				while (inptr > full_name) {
					if (!strncmp (inptr, "_subfolders_", 12))
						memmove (inptr, inptr + 11, strlen (inptr + 11) + 1);
					
					inptr--;
				}
				
				g_string_append (new, full_name);
				g_free (full_name);
				
				filename = buf = new->str;
				g_string_free (new, FALSE);
				e_filename_make_safe (buf);
			} else {
				/* but just in case... */
				filename = dent->d_name + prelen;
			}
		} else {
			/* no munging needed */
			filename = dent->d_name + prelen;
		}
		
	copy:
		g_string_append (srcpath, dent->d_name);
		if (prelen > 0)
			g_string_append_len (destpath, dent->d_name, prelen);
		g_string_append (destpath, filename);
		g_free (buf);
		
		cp (srcpath->str, destpath->str, FALSE);
		
		g_string_truncate (srcpath, slen);
		g_string_truncate (destpath, dlen);
	}
	
	closedir (dir);
	
	g_free (evo14_mbox_root);
	g_string_free (destpath, TRUE);
	g_string_free (srcpath, TRUE);
	
	return 0;
}

static int
em_migrate_imap_cmeta_1_4(const char *evolution_dir, CamelException *ex)
{
	GConfClient *gconf;
	GSList *paths, *p;
	EAccountList *accounts;
	const EAccount *account;

	if (!(accounts = mail_config_get_accounts()))
		return 0;

	gconf = gconf_client_get_default();
	paths = gconf_client_get_list(gconf, "/apps/evolution/shell/offline/folder_paths", GCONF_VALUE_STRING, NULL);
	for (p = paths;p;p = g_slist_next(p)) {
		char *name, *path;

		name = p->data;
		if (*name)
			name++;
		path = strchr(name, '/');
		if (path) {
			*path++ = 0;
			account = e_account_list_find(accounts, E_ACCOUNT_FIND_NAME, name);
			if (account && !strncmp(account->source->url, "imap:", 5)) {
				CamelURL *url = camel_url_new(account->source->url, NULL);

				if (url) {
					char *dir, *base;

					base = g_strdup_printf("%s/mail/imap/%s@%s/folders",
							       evolution_dir,
							       url->user?url->user:"",
							       url->host?url->host:"");

					dir = e_path_to_physical(base, path);
					if (camel_mkdir(dir, 0777) == 0) {
						char *cmeta;
						FILE *fp;

						cmeta = g_build_filename(dir, "cmeta", NULL);
						fp = fopen(cmeta, "w");
						if (fp) {
							/* header/version */
							fwrite("CLMD", 4, 1, fp);
							camel_file_util_encode_uint32(fp, 1);
							/* meta count, do we have any metadata? */
							camel_file_util_encode_uint32(fp, 0);
							/* prop count */
							camel_file_util_encode_uint32(fp, 1);
							/* sync offline property */
							camel_file_util_encode_uint32(fp, CAMEL_DISCO_FOLDER_OFFLINE_SYNC);
							camel_file_util_encode_uint32(fp, 1);
							fclose(fp);
						} else {
							g_warning("couldn't create imap folder cmeta file '%s'", cmeta);
						}
						g_free(cmeta);
					} else {
						g_warning("couldn't create imap folder directory '%s'", dir);
					}
					g_free(dir);
					g_free(base);
					camel_url_free(url);
				}
			} else
				g_warning("can't find offline folder '%s' '%s'", name, path);
		}
		g_free(p->data);
	}
	g_slist_free(paths);
	g_object_unref(gconf);

	/* we couldn't care less if this doesn't work */

	return 0;
}

static int
em_migrate_1_4 (const char *evolution_dir, xmlDocPtr filters, xmlDocPtr vfolders, CamelException *ex)
{
	EMMigrateSession *session;
	CamelException lex;
	struct stat st;
	char *path;
	
	path = g_build_filename (evolution_dir, "mail", NULL);
	
	camel_init (path, TRUE);
	session = (EMMigrateSession *) em_migrate_session_new (path);
	g_free (path);
	
	session->srcdir = g_build_filename (g_get_home_dir (), "evolution", "local", NULL);
	
	path = g_strdup_printf ("mbox:%s/.evolution/mail/local", g_get_home_dir ());
	if (stat (path + 5, &st) == -1) {
		if (errno != ENOENT || camel_mkdir (path + 5, 0777) == -1) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      _("Failed to create directory `%s': %s"),
					      path + 5, g_strerror (errno));
			g_free (session->srcdir);
			camel_object_unref (session);
			g_free (path);
			return -1;
		}
	}
	
	camel_exception_init (&lex);
	if (!(session->store = camel_session_get_store ((CamelSession *) session, path, &lex))) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Failed to open store for `%s': %s"),
				      path, lex.desc);
		g_free (session->srcdir);
		camel_object_unref (session);
		camel_exception_clear (&lex);
		g_free (path);
		return -1;
	}
	g_free (path);
	
	em_migrate_local_folders_1_4 (session);
	
	camel_object_unref (session->store);
	g_free (session->srcdir);
	
	camel_object_unref (session);
	
	if (em_upgrade_accounts_1_4 () == -1)
		return -1;
	
	if (em_upgrade_xml_1_4 (filters, FALSE) == -1)
		return -1;
	
	if (em_upgrade_xml_1_4 (vfolders, TRUE) == -1)
		return -1;
	
	path = g_build_filename (g_get_home_dir (), "evolution", "searches.xml", NULL);
	if (stat (path, &st) == 0 && S_ISREG (st.st_mode)) {
		char *dest;
		
		dest = g_build_filename (evolution_dir, "mail", "searches.xml", NULL);
		cp (path, dest, FALSE);
		g_free (dest);
	}
	g_free (path);
	
	if (em_migrate_pop_uid_caches_1_4 (evolution_dir, ex) == -1)
		return -1;
	
	if (em_migrate_imap_caches_1_4 (evolution_dir, ex) == -1)
		return -1;
	
	/* these are non-fatal */
	em_migrate_folder_expand_state_1_4 (evolution_dir, ex);
	camel_exception_clear(ex);
	em_migrate_folder_view_settings_1_4 (evolution_dir, ex);
	camel_exception_clear(ex);
	em_migrate_imap_cmeta_1_4(evolution_dir, ex);
	camel_exception_clear(ex);

	return 0;
}


static xmlDocPtr
emm_load_xml (const char *dirname, const char *filename)
{
	xmlDocPtr doc;
	struct stat st;
	char *path;
	
	path = g_strdup_printf ("%s/%s", dirname, filename);
	if (stat (path, &st) == -1 || !(doc = xmlParseFile (path))) {
		g_free (path);
		return NULL;
	}
	
	g_free (path);
	
	return doc;
}

static int
emm_save_xml (xmlDocPtr doc, const char *dirname, const char *filename)
{
	char *path;
	int retval;
	
	path = g_strdup_printf ("%s/%s", dirname, filename);
	retval = e_xml_save_file (path, doc);
	g_free (path);
	
	return retval;
}

static int
emm_setup_initial(const char *evolution_dir)
{
	DIR *dir;
	struct dirent *d;
	struct stat st;
	char *base, *local, *lang;

	/* special-case - this means brand new install of evolution */
	/* FIXME: create default folders and stuff... */

	base = (char *) e_iconv_locale_language ();
	if (base) {
		lang = g_alloca(strlen(base)+1);
		strcpy(lang, base);
	} else
		lang = NULL;

	d(printf("Setting up initial mail tree\n"));
	
	base = g_build_filename(evolution_dir, "/mail/local", NULL);
	if (camel_mkdir(base, 0777) == -1 && errno != EEXIST) {
		g_free(base);
		return -1;
	}

	/* e.g. try en-AU then en, etc */
	while (lang != NULL) {
		local = g_build_filename(EVOLUTION_PRIVDATADIR "/default", lang, "mail/local", NULL);
		if (stat(local, &st) == 0)
			goto gotlocal;

		g_free(local);
		if (strlen(lang)>2 && lang[2] == '-')
			lang[2] = 0;
		else
			lang = NULL;
	}

	local = g_build_filename(EVOLUTION_PRIVDATADIR "/default/C/mail/local", NULL);
gotlocal:

	dir = opendir(local);
	if (dir) {
		while ((d = readdir(dir))) {
			char *src, *dest;

			if (!strcmp(d->d_name, ".") || !strcmp(d->d_name, ".."))
				continue;

			src = g_build_filename(local, d->d_name, NULL);
			dest = g_build_filename(base, d->d_name, NULL);

			cp(src, dest, FALSE);
			g_free(dest);
			g_free(src);
		}
		closedir(dir);
	}

	g_free(local);
	g_free(base);

	return 0;
}

int
em_migrate (const char *evolution_dir, int major, int minor, int revision, CamelException *ex)
{
	struct stat st;
	char *path;
	
	/* make sure ~/.evolution/mail exists */
	path = g_build_filename (evolution_dir, "mail", NULL);
	if (stat (path, &st) == -1) {
		if (errno != ENOENT || camel_mkdir (path, 0777) == -1) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM, 
					      _("Failed to create directory `%s': %s"),
					      path, g_strerror (errno));
			g_free (path);
			return -1;
		}
	}
	
	g_free (path);
	
	if (major == 0)
		return emm_setup_initial(evolution_dir);
	
	if (major == 1 && minor < 5) {
		xmlDocPtr config_xmldb = NULL, filters, vfolders;
		
		path = g_build_filename (g_get_home_dir (), "evolution", NULL);
		if (minor <= 2 && !(config_xmldb = emm_load_xml (path, "config.xmldb"))) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      "Cannot migrate mail settings/data from Evolution %d.%d.%d: "
					      "~/evolution/config.xmldb doesn't exist or is corrupt!",
					      major, minor, revision);
			g_warning ("Cannot migrate mail settings/data from Evolution %d.%d.%d: "
				   "~/evolution/config.xmldb doesn't exist or is corrupt!",
				   major, minor, revision);
			g_free (path);
			return -1;
		}
		filters = emm_load_xml (path, "filters.xml");
		vfolders = emm_load_xml (path, "vfolders.xml");
		g_free (path);
		
		if (minor == 0) {
			if (em_migrate_1_0 (evolution_dir, config_xmldb, filters, vfolders, ex) == -1) {
				xmlFreeDoc (config_xmldb);
				xmlFreeDoc (filters);
				xmlFreeDoc (vfolders);
				return -1;
			}
		}
		
		if (minor <= 2) {
			if (em_migrate_1_2 (evolution_dir, config_xmldb, filters, vfolders, ex) == -1) {
				xmlFreeDoc (config_xmldb);
				xmlFreeDoc (filters);
				xmlFreeDoc (vfolders);
				return -1;
			}
			
			xmlFreeDoc (config_xmldb);
		}
		
		if (minor <= 4) {
			if (em_migrate_1_4 (evolution_dir, filters, vfolders, ex) == -1) {
				xmlFreeDoc (filters);
				xmlFreeDoc (vfolders);
				return -1;
			}
		}
		
		path = g_build_filename (evolution_dir, "mail", NULL);
		
		if (filters) {
			emm_save_xml (filters, path, "filters.xml");
			xmlFreeDoc (filters);
		}
		
		if (vfolders) {
			emm_save_xml (vfolders, path, "vfolders.xml");
			xmlFreeDoc (vfolders);
		}
		
		g_free (path);
	}
	
	return 0;
}
