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
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
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

#include <glib.h>
#include <glib/gstdio.h>

#include <gtk/gtk.h>

#include <gconf/gconf-client.h>

#include <camel/camel.h>
#include <camel/camel-store.h>
#include <camel/camel-session.h>
#include <camel/camel-file-utils.h>
#include <camel/camel-disco-folder.h>

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

#include <glib/gi18n.h>

#include <e-util/e-util.h>
#include <libedataserver/e-xml-utils.h>
#include <libedataserver/e-data-server-util.h>
#include <e-util/e-xml-utils.h>

#include "e-util/e-bconf-map.h"
#include "libedataserver/e-account-list.h"
#include "e-util/e-signature-list.h"
#include "e-util/e-error.h"
#include "e-util/e-util-private.h"
#include "e-util/e-plugin.h"

#include "mail-component.h"
#include "mail-config.h"
#include "mail-session.h"
#include "em-utils.h"
#include "em-migrate.h"

#define d(x)

#ifndef G_OS_WIN32
/* No versions previous to 2.8 or thereabouts have been available on
 * Windows, so don't bother with upgrade support from earlier versions
 * on Win32. Do try to support upgrades from 2.12 and later to the
 * current version.
 */

/* upgrade helper functions */
static xmlDocPtr
emm_load_xml (const gchar *dirname, const gchar *filename)
{
	xmlDocPtr doc;
	struct stat st;
	gchar *path;

	path = g_strdup_printf ("%s/%s", dirname, filename);
	if (stat (path, &st) == -1 || !(doc = xmlParseFile (path))) {
		g_free (path);
		return NULL;
	}

	g_free (path);

	return doc;
}

static gint
emm_save_xml (xmlDocPtr doc, const gchar *dirname, const gchar *filename)
{
	gchar *path;
	gint retval;

	path = g_strdup_printf ("%s/%s", dirname, filename);
	retval = e_xml_save_file (path, doc);
	g_free (path);

	return retval;
}

static xmlNodePtr
xml_find_node (xmlNodePtr parent, const gchar *name)
{
	xmlNodePtr node;

	node = parent->children;
	while (node != NULL) {
		if (node->name && !strcmp ((gchar *)node->name, name))
			return node;

		node = node->next;
	}

	return NULL;
}

static void
upgrade_xml_uris (xmlDocPtr doc, gchar * (* upgrade_uri) (const gchar *uri))
{
	xmlNodePtr root, node;
	gchar *uri, *new;

	if (!doc || !(root = xmlDocGetRootElement (doc)))
		return;

	if (!root->name || strcmp ((gchar *)root->name, "filteroptions") != 0) {
		/* root node is not <filteroptions>, nothing to upgrade */
		return;
	}

	if (!(node = xml_find_node (root, "ruleset"))) {
		/* no ruleset node, nothing to upgrade */
		return;
	}

	node = node->children;
	while (node != NULL) {
		if (node->name && !strcmp ((gchar *)node->name, "rule")) {
			xmlNodePtr actionset, part, val, n;

			if ((actionset = xml_find_node (node, "actionset"))) {
				/* filters.xml */
				part = actionset->children;
				while (part != NULL) {
					if (part->name && !strcmp ((gchar *)part->name, "part")) {
						val = part->children;
						while (val != NULL) {
							if (val->name && !strcmp ((gchar *)val->name, "value")) {
								gchar *type;

								type = (gchar *)xmlGetProp (val, (const guchar *)"type");
								if (type && !strcmp ((gchar *)type, "folder")) {
									if ((n = xml_find_node (val, "folder"))) {
										uri = (gchar *)xmlGetProp (n, (const guchar *)"uri");
										new = upgrade_uri (uri);
										xmlFree (uri);

										xmlSetProp (n, (const guchar *)"uri", (guchar *)new);
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
					if (n->name && !strcmp ((gchar *)n->name, "folder")) {
						uri = (gchar *)xmlGetProp (n, (const guchar *)"uri");
						new = upgrade_uri (uri);
						xmlFree (uri);

						xmlSetProp (n, (const guchar *)"uri", (guchar *)new);
						g_free (new);
					}

					n = n->next;
				}
			}
		}

		node = node->next;
	}
}

/* 1.0 upgrade functions & data */

/* as much info as we have on a given account */
struct _account_info_1_0 {
	gchar *name;
	gchar *uri;
	gchar *base_uri;
	union {
		struct {
			/* for imap */
			gchar *namespace;
			gchar *namespace_full;
			guint32 capabilities;
			GHashTable *folders;
			gchar dir_sep;
		} imap;
	} u;
};

struct _imap_folder_info_1_0 {
	gchar *folder;
	/* encoded?  decoded?  canonicalised? */
	gchar dir_sep;
};

static GHashTable *accounts_1_0 = NULL;
static GHashTable *accounts_name_1_0 = NULL;

static void
imap_folder_info_1_0_free (struct _imap_folder_info_1_0 *fi)
{
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
	g_hash_table_destroy(ai->u.imap.folders);
	g_free(ai);
}

static gchar *
get_base_uri(const gchar *val)
{
	const gchar *tmp;

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

static gchar *
upgrade_xml_uris_1_0 (const gchar *uri)
{
	gchar *out = NULL;

	/* upgrades camel uri's */
	if (strncmp (uri, "imap:", 5) == 0) {
		gchar *base_uri, dir_sep, *folder, *p;
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
		if (ai->u.imap.namespace && strcmp ((gchar *)folder, "INBOX") != 0)
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
		gchar *base_uri, *folder, *p;

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

static gchar *
parse_lsub (const gchar *lsub, gchar *dir_sep)
{
	static gint comp;
	static regex_t pat;
	regmatch_t match[3];
	const gchar *m = "^\\* LSUB \\([^)]*\\) \"?([^\" ]+)\"? \"?(.*)\"?$";

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

static gint
read_imap_storeinfo (struct _account_info_1_0 *si)
{
	FILE *storeinfo;
	guint32 tmp;
	gchar *buf, *folder, dir_sep, *path, *name, *p;
	struct _imap_folder_info_1_0 *fi;

	si->u.imap.folders = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) NULL,
		(GDestroyNotify) imap_folder_info_1_0_free);

	/* get details from uri first */
	name = strstr (si->uri, ";override_namespace");
	if (name) {
		name = strstr (si->uri, ";namespace=");
		if (name) {
			gchar *end;

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

static gint
load_accounts_1_0 (xmlDocPtr doc)
{
	xmlNodePtr source;
	gchar *val, *tmp;
	gint count = 0, i;
	gchar key[32];

	if (!(source = e_bconf_get_path (doc, "/Mail/Accounts")))
		return 0;

	if ((val = e_bconf_get_value (source, "num"))) {
		count = atoi (val);
		xmlFree (val);
	}

	/* load account upgrade info for each account */
	for (i = 0; i < count; i++) {
		struct _account_info_1_0 *ai;
		gchar *rawuri;

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
			if (node && (val = (gchar *)xmlGetProp (node, (const guchar *)"value"))) {
				tmp = e_bconf_hex_decode (val);
				xmlFree (val);
				if (strncmp (tmp, "exchanget:", 10) == 0)
					xmlSetProp (node, (const guchar *)"value", (guchar *)rawuri);
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

static gint
em_migrate_1_0 (const gchar *evolution_dir, xmlDocPtr config_xmldb, xmlDocPtr filters, xmlDocPtr vfolders, CamelException *ex)
{
	accounts_1_0 = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) NULL,
		(GDestroyNotify) account_info_1_0_free);
	accounts_name_1_0 = g_hash_table_new (g_str_hash, g_str_equal);
	load_accounts_1_0 (config_xmldb);

	upgrade_xml_uris(filters, upgrade_xml_uris_1_0);
	upgrade_xml_uris(vfolders, upgrade_xml_uris_1_0);

	g_hash_table_destroy (accounts_1_0);
	g_hash_table_destroy (accounts_name_1_0);

	return 0;
}

/* 1.2 upgrade functions */
static gint
is_xml1encoded (const gchar *txt)
{
	const guchar *p;
	gint isxml1 = FALSE;
	gint is8bit = FALSE;

	p = (const guchar *)txt;
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

static gchar *
decode_xml1 (const gchar *txt)
{
	GString *out = g_string_new ("");
	const guchar *p;
	gchar *res;

	/* convert:
	   \U+XXXX\ -> utf8
	   8 bit characters -> utf8 (iso-8859-1) */

	p = (const guchar *) txt;
	while (*p) {
		if (p[0] > 0x80
		    || (p[0] == '\\' && p[1] == 'U' && p[2] == '+'
			&& isxdigit (p[3]) && isxdigit (p[4]) && isxdigit (p[5]) && isxdigit (p[6])
			&& p[7] == '\\')) {
			gchar utf8[8];
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

static gchar *
utf8_reencode (const gchar *txt)
{
	GString *out = g_string_new ("");
	gchar *p;
	gchar *res;

	/* convert:
        libxml1  8 bit utf8 converted to xml entities byte-by-byte chars -> utf8 */

	p =  (gchar *)txt;

	while (*p) {
		g_string_append_c (out, (gchar)g_utf8_get_char ((const gchar *)p));
		p = (gchar *)g_utf8_next_char (p);
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

static gint
upgrade_xml_1_2_rec (xmlNodePtr node)
{
	const gchar *value_tags[] = { "string", "address", "regex", "file", "command", NULL };
	const gchar *rule_tags[] = { "title", NULL };
	const gchar *item_props[] = { "name", NULL };
	struct {
		const gchar *name;
		const gchar **tags;
		const gchar **props;
	} tags[] = {
		{ "value", value_tags, NULL },
		{ "rule", rule_tags, NULL },
		{ "item", NULL, item_props },
		{ 0 },
	};
	xmlNodePtr work;
	gint i,j;
	gchar *txt, *tmp;

	/* upgrades the content of a node, if the node has a specific parent/node name */

	for (i = 0; tags[i].name; i++) {
		if (!strcmp ((gchar *)node->name, tags[i].name)) {
			if (tags[i].tags != NULL) {
				work = node->children;
				while (work) {
					for (j = 0; tags[i].tags[j]; j++) {
						if (!strcmp ((gchar *)work->name, tags[i].tags[j])) {
							txt = (gchar *)xmlNodeGetContent (work);
							if (is_xml1encoded (txt)) {
								tmp = decode_xml1 (txt);
								d(printf ("upgrading xml node %s/%s '%s' -> '%s'\n",
									  tags[i].name, tags[i].tags[j], txt, tmp));
								xmlNodeSetContent (work, (guchar *)tmp);
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
					txt = (gchar *)xmlGetProp (node, (guchar *)tags[i].props[j]);
					tmp = utf8_reencode (txt);
					d(printf ("upgrading xml property %s on node %s '%s' -> '%s'\n",
						  tags[i].props[j], tags[i].name, txt, tmp));
					xmlSetProp (node, (const guchar *)tags[i].props[j], (guchar *)tmp);
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

static gint
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
	{ "side_bar_search", "mail/display/side_bar_search", E_GCONF_MAP_BOOL },
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

static struct {
	const gchar *label;
	const gchar *colour;
} label_default[5] = {
	{ N_("Important"), "#EF2929" },  /* red */
	{ N_("Work"),      "#F57900" },  /* orange */
	{ N_("Personal"),  "#4E9A06" },  /* green */
	{ N_("To Do"),     "#3465A4" },  /* blue */
	{ N_("Later"),     "#75507B" }   /* purple */
};

/* remaps mail config from bconf to gconf */
static gint
bconf_import(GConfClient *gconf, xmlDocPtr config_xmldb)
{
	xmlNodePtr source;
	gchar labx[16], colx[16];
	gchar *val, *lab, *col;
	GSList *list, *l;
	gint i;

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

static gint
em_migrate_1_2(const gchar *evolution_dir, xmlDocPtr config_xmldb, xmlDocPtr filters, xmlDocPtr vfolders, CamelException *ex)
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
	gchar *srcdir;        /* old folder tree path */
} EMMigrateSession;

typedef struct _EMMigrateSessionClass {
	CamelSessionClass parent_class;

} EMMigrateSessionClass;

static CamelType em_migrate_session_get_type (void);
static CamelSession *em_migrate_session_new (const gchar *path);

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
em_migrate_session_new (const gchar *path)
{
	CamelSession *session;

	session = CAMEL_SESSION (camel_object_new (EM_MIGRATE_SESSION_TYPE));

	camel_session_construct (session, path);

	return session;
}

#endif	/* !G_OS_WIN32 */

static GtkWidget *window;
static GtkLabel *label;
static GtkProgressBar *progress;

static void
em_migrate_setup_progress_dialog (const gchar *title, const gchar *desc)
{
	GtkWidget *vbox, *hbox, *w;
	gchar *markup;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	gtk_window_set_title (GTK_WINDOW (window), _("Migrating..."));
	gtk_window_set_modal (GTK_WINDOW (window), TRUE);
	gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (window), 6);

	hbox = gtk_hbox_new (FALSE, 24);

	/* Install the info image */
	w = gtk_image_new_from_stock (GTK_STOCK_DIALOG_INFO, GTK_ICON_SIZE_DIALOG);
	gtk_misc_set_alignment (GTK_MISC (w), 0.5, 0.0);
	gtk_box_pack_start (GTK_BOX (hbox), w, TRUE, TRUE, 0);

	/* Prepare the message */
	vbox = gtk_vbox_new (FALSE, 12);
	gtk_widget_show (vbox);
	gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);

	w = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (w), 0.0, 0.0);
	markup = g_strconcat ("<big><b>", title ? title : _("Migration"), "</b></big>", NULL);
	gtk_label_set_markup (GTK_LABEL (w), markup);
	gtk_box_pack_start (GTK_BOX (vbox), w, TRUE, TRUE, 0);
	g_free (markup);

	w = gtk_label_new (desc);
	gtk_misc_set_alignment (GTK_MISC (w), 0.0, 0.0);
	gtk_label_set_line_wrap (GTK_LABEL (w), TRUE);
	gtk_box_pack_start (GTK_BOX (vbox), w, TRUE, TRUE, 0);

	/* Progress bar */
	w = gtk_vbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (vbox), w, TRUE, TRUE, 0);

	label = GTK_LABEL (gtk_label_new (""));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
	gtk_label_set_line_wrap (label, TRUE);
	gtk_widget_show (GTK_WIDGET (label));
	gtk_box_pack_start (GTK_BOX (w), GTK_WIDGET (label), TRUE, TRUE, 0);

	progress = GTK_PROGRESS_BAR (gtk_progress_bar_new ());
	gtk_widget_show (GTK_WIDGET (progress));
	gtk_box_pack_start (GTK_BOX (w), GTK_WIDGET (progress), TRUE, TRUE, 0);

	gtk_container_add (GTK_CONTAINER (window), hbox);
	gtk_widget_show_all (hbox);
	gtk_widget_show (window);
}

static void
em_migrate_close_progress_dialog (void)
{
	gtk_widget_destroy (GTK_WIDGET (window));
}

static void
em_migrate_set_folder_name (const gchar *folder_name)
{
	gchar *text;

	text = g_strdup_printf (_("Migrating '%s':"), folder_name);
	gtk_label_set_text (label, text);
	g_free (text);

	gtk_progress_bar_set_fraction (progress, 0.0);
	while (gtk_events_pending ())
		gtk_main_iteration ();
}

static void
em_migrate_set_progress (double percent)
{
	gchar text[5];

	snprintf (text, sizeof (text), "%d%%", (gint) (percent * 100.0f));

	gtk_progress_bar_set_fraction (progress, percent);
	gtk_progress_bar_set_text (progress, text);

	while (gtk_events_pending ())
		gtk_main_iteration ();
}

#ifndef G_OS_WIN32

static gboolean
is_mail_folder (const gchar *metadata)
{
	xmlNodePtr node;
	xmlDocPtr doc;
	gchar *type;

	if (!(doc = xmlParseFile (metadata))) {
		g_warning ("Cannot parse `%s'", metadata);
		return FALSE;
	}

	if (!(node = xmlDocGetRootElement (doc))) {
		g_warning ("`%s' corrupt: document contains no root node", metadata);
		xmlFreeDoc (doc);
		return FALSE;
	}

	if (!node->name || strcmp ((gchar *)node->name, "efolder") != 0) {
		g_warning ("`%s' corrupt: root node is not 'efolder'", metadata);
		xmlFreeDoc (doc);
		return FALSE;
	}

	node = node->children;
	while (node != NULL) {
		if (node->name && !strcmp ((gchar *)node->name, "type")) {
			type = (gchar *)xmlNodeGetContent (node);
			if (!strcmp ((gchar *)type, "mail")) {
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

static gint
get_local_et_expanded (const gchar *dirname)
{
	xmlNodePtr node;
	xmlDocPtr doc;
	struct stat st;
	gchar *buf, *p;
	gint thread_list;

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

	if (!(node = xmlDocGetRootElement (doc)) || strcmp ((gchar *)node->name, "expanded_state") != 0) {
		xmlFreeDoc (doc);
		return -1;
	}

	if (!(buf = (gchar *)xmlGetProp (node, (const guchar *)"default"))) {
		xmlFreeDoc (doc);
		return -1;
	}

	thread_list = strcmp (buf, "0") == 0 ? 0 : 1;
	xmlFree (buf);

	xmlFreeDoc (doc);

	return thread_list;
}

static gchar *
get_local_store_uri (const gchar *dirname, gchar **namep, gint *indexp)
{
	gchar *name, *protocol, *metadata, *tmp;
	gint index;
	struct stat st;
	xmlNodePtr node;
	xmlDocPtr doc;

	metadata = g_build_filename(dirname, "local-metadata.xml", NULL);

	/* in 1.4, any errors are treated as defaults, this function cannot fail */

	/* defaults */
	name = (gchar *) "mbox";
	protocol = (gchar *) "mbox";
	index = TRUE;

	if (stat (metadata, &st) == -1 || !S_ISREG (st.st_mode))
		goto nofile;

	doc = xmlParseFile(metadata);
	if (doc == NULL)
		goto nofile;

	node = doc->children;
	if (strcmp((gchar *)node->name, "folderinfo"))
		goto dodefault;

	for (node = node->children; node; node = node->next) {
		if (node->name && !strcmp ((gchar *)node->name, "folder")) {
			tmp = (gchar *)xmlGetProp (node, (const guchar *)"type");
			if (tmp) {
				protocol = alloca(strlen(tmp)+1);
				strcpy(protocol, tmp);
				xmlFree(tmp);
			}
			tmp = (gchar *)xmlGetProp (node, (const guchar *)"name");
			if (tmp) {
				name = alloca(strlen(tmp)+1);
				strcpy(name, tmp);
				xmlFree(tmp);
			}
			tmp = (gchar *)xmlGetProp (node, (const guchar *)"index");
			if (tmp) {
				index = atoi(tmp);
				xmlFree(tmp);
			}
		}
	}
dodefault:
	xmlFreeDoc (doc);
nofile:
	g_free(metadata);

	*namep = g_strdup(name);
	*indexp = index;

	return g_strdup_printf("%s:%s", protocol, dirname);
}

#endif	/* !G_OS_WIN32 */

enum {
	CP_UNIQUE = 0,
	CP_OVERWRITE,
	CP_APPEND
};

static gint open_flags[3] = {
	O_WRONLY | O_CREAT | O_TRUNC,
	O_WRONLY | O_CREAT | O_TRUNC,
	O_WRONLY | O_CREAT | O_APPEND,
};

static gint
cp (const gchar *src, const gchar *dest, gboolean show_progress, gint mode)
{
	guchar readbuf[65536];
	gssize nread, nwritten;
	gint errnosav, readfd, writefd;
	gsize total = 0;
	struct stat st;
	struct utimbuf ut;

	/* if the dest file exists and has content, abort - we don't
	 * want to corrupt their existing data */
	if (g_stat (dest, &st) == 0 && st.st_size > 0 && mode == CP_UNIQUE) {
		errno = EEXIST;
		return -1;
	}

	if (g_stat (src, &st) == -1
	    || (readfd = g_open (src, O_RDONLY | O_BINARY, 0)) == -1)
		return -1;

	if ((writefd = g_open (dest, open_flags[mode] | O_BINARY, 0666)) == -1) {
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

#ifndef G_OS_WIN32

static gint
cp_r (const gchar *src, const gchar *dest, const gchar *pattern, gint mode)
{
	GString *srcpath, *destpath;
	struct dirent *dent;
	gsize slen, dlen;
	struct stat st;
	DIR *dir;

	if (g_mkdir_with_parents (dest, 0777) == -1)
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
			cp_r (srcpath->str, destpath->str, pattern, mode);
		} else if (!pattern || !strcmp (dent->d_name, pattern)) {
			cp (srcpath->str, destpath->str, FALSE, mode);
		}
	}

	closedir (dir);

	g_string_free (destpath, TRUE);
	g_string_free (srcpath, TRUE);

	return 0;
}

static void
mbox_build_filename (GString *path, const gchar *toplevel_dir, const gchar *full_name)
{
	const gchar *start, *inptr = full_name;
	gint subdirs = 0;

	while (*inptr != '\0') {
		if (*inptr == '/')
			subdirs++;
		inptr++;
	}

	g_string_assign(path, toplevel_dir);
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
}

static gint
em_migrate_folder(EMMigrateSession *session, const gchar *dirname, const gchar *full_name, CamelException *ex)
{
	CamelFolder *old_folder = NULL, *new_folder = NULL;
	CamelStore *local_store = NULL;
	gchar *name, *uri;
	GPtrArray *uids;
	struct stat st;
	gint thread_list;
	gint index, i;
	GString *src, *dest;
	gint res = -1;

	src = g_string_new("");

	g_string_printf(src, "%s/folder-metadata.xml", dirname);
	if (stat (src->str, &st) == -1
	    || !S_ISREG (st.st_mode)
	    || !is_mail_folder(src->str)) {
		/* Not an evolution mail folder */
		g_string_free(src, TRUE);
		return 0;
	}

	dest = g_string_new("");
	uri = get_local_store_uri(dirname, &name, &index);
	em_migrate_set_folder_name (full_name);
	thread_list = get_local_et_expanded (dirname);

	/* Manually copy local mbox files, its much faster */
	if (!strncmp (uri, "mbox:", 5)) {
		static const gchar *meta_ext[] = { ".summary", ".ibex.index", ".ibex.index.data" };
		gsize slen, dlen;
		FILE *fp;
		gchar *p;
		gint mode;

		g_string_printf (src, "%s/%s", uri + 5, name);
		mbox_build_filename (dest, ((CamelService *)session->store)->url->path, full_name);
		p = strrchr (dest->str, '/');
		*p = '\0';

		slen = src->len;
		dlen = dest->len;

		if (g_mkdir_with_parents (dest->str, 0777) == -1 && errno != EEXIST) {
			camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
					     _("Unable to create new folder `%s': %s"),
					     dest->str, g_strerror(errno));
			goto fatal;
		}

		*p = '/';
		mode = CP_UNIQUE;
	retry_copy:
		if (cp (src->str, dest->str, TRUE, mode) == -1) {
			if (errno == EEXIST) {
				gint save = errno;

				switch (e_error_run(NULL, "mail:ask-migrate-existing", src->str, dest->str, NULL)) {
				case GTK_RESPONSE_ACCEPT:
					mode = CP_OVERWRITE;
					goto retry_copy;
				case GTK_RESPONSE_OK:
					mode = CP_APPEND;
					goto retry_copy;
				case GTK_RESPONSE_REJECT:
					goto ignore;
				}

				errno = save;
			}
			camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
					     _("Unable to copy folder `%s' to `%s': %s"),
					     src->str, dest->str, g_strerror(errno));
			goto fatal;
		}
	ignore:

		/* create a .cmeta file specifying to index and/or thread the folder */
		g_string_truncate (dest, dlen);
		g_string_append (dest, ".cmeta");
		if ((fp = fopen (dest->str, "w")) != NULL) {
			gint fd = fileno (fp);

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

		/* copy over the metadata files */
		for (i = 0; i < sizeof(meta_ext)/sizeof(meta_ext[0]); i++) {
			g_string_truncate (src, slen);
			g_string_truncate (dest, dlen);

			g_string_append (src, meta_ext[i]);
			g_string_append (dest, meta_ext[i]);
			cp (src->str, dest->str, FALSE, CP_OVERWRITE);
		}
	} else {
		guint32 flags = CAMEL_STORE_FOLDER_CREATE;

		if (!(local_store = camel_session_get_store ((CamelSession *) session, uri, ex))
		    || !(old_folder = camel_store_get_folder (local_store, name, 0, ex)))
			goto fatal;

		flags |= (index ? CAMEL_STORE_FOLDER_BODY_INDEX : 0);
		if (!(new_folder = camel_store_get_folder (session->store, full_name, flags, ex)))
			goto fatal;

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

			if (!(message = camel_folder_get_message (old_folder, uids->pdata[i], ex))) {
				camel_folder_free_message_info (old_folder, info);
				camel_folder_free_uids (old_folder, uids);
				goto fatal;
			}

			camel_folder_append_message (new_folder, message, info, NULL, ex);
			camel_folder_free_message_info (old_folder, info);
			camel_object_unref (message);

			if (camel_exception_is_set (ex))
				break;

			em_migrate_set_progress (((double) i + 1) / ((double) uids->len));
		}

		camel_folder_free_uids (old_folder, uids);

		if (camel_exception_is_set (ex))
			goto fatal;
	}
	res = 0;
fatal:
	g_free (uri);
	g_free (name);
	g_string_free(src, TRUE);
	g_string_free(dest, TRUE);
	if (local_store)
		camel_object_unref(local_store);
	if (old_folder)
		camel_object_unref(old_folder);
	if (new_folder)
		camel_object_unref(new_folder);

	return res;
}

static gint
em_migrate_dir (EMMigrateSession *session, const gchar *dirname, const gchar *full_name, CamelException *ex)
{
	gchar *path;
	DIR *dir;
	struct stat st;
	struct dirent *dent;
	gint res = 0;

	if (em_migrate_folder(session, dirname, full_name, ex) == -1)
		return -1;

	/* no subfolders, not readable, don't care */
	path = g_strdup_printf ("%s/subfolders", dirname);
	if (stat (path, &st) == -1 || !S_ISDIR (st.st_mode)) {
		g_free (path);
		return 0;
	}

	if (!(dir = opendir (path))) {
		g_free (path);
		return 0;
	}

	while (res == 0 && (dent = readdir (dir))) {
		gchar *full_path;
		gchar *name;

		if (dent->d_name[0] == '.')
			continue;

		full_path = g_strdup_printf ("%s/%s", path, dent->d_name);
		if (stat (full_path, &st) == -1 || !S_ISDIR (st.st_mode)) {
			g_free (full_path);
			continue;
		}

		name = g_strdup_printf ("%s/%s", full_name, dent->d_name);
		res = em_migrate_dir (session, full_path, name, ex);
		g_free (full_path);
		g_free (name);
	}

	closedir (dir);

	g_free (path);

	return res;
}

static gint
em_migrate_local_folders_1_4 (EMMigrateSession *session, CamelException *ex)
{
	struct dirent *dent;
	struct stat st;
	DIR *dir;
	gint res = 0;

	if (!(dir = opendir (session->srcdir))) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Unable to scan for existing mailboxes at `%s': %s"),
				     session->srcdir, g_strerror(errno));
		return -1;
	}

	em_migrate_setup_progress_dialog (NULL, _("The location and hierarchy of the Evolution mailbox "
			     "folders has changed since Evolution 1.x.\n\nPlease be "
			     "patient while Evolution migrates your folders..."));

	while (res == 0 && (dent = readdir (dir))) {
		gchar *full_path;

		if (dent->d_name[0] == '.')
			continue;

		full_path = g_strdup_printf ("%s/%s", session->srcdir, dent->d_name);
		if (stat (full_path, &st) == -1 || !S_ISDIR (st.st_mode)) {
			g_free (full_path);
			continue;
		}

		res = em_migrate_dir (session, full_path, dent->d_name, ex);
		g_free (full_path);
	}

	closedir (dir);

	em_migrate_close_progress_dialog ();

	return res;
}

static gchar *
upgrade_xml_uris_1_4 (const gchar *uri)
{
	gchar *path, *prefix, *p;
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

	if (!root->name || strcmp ((gchar *)root->name, "filteroptions") != 0) {
		/* root node is not <filteroptions>, nothing to upgrade */
		return;
	}

	if (!(node = xml_find_node (root, "ruleset"))) {
		/* no ruleset node, nothing to upgrade */
		return;
	}

	node = node->children;
	while (node != NULL) {
		if (node->name && !strcmp ((gchar *)node->name, "rule")) {
			xmlNodePtr sources;
			gchar *src;

			if (!(src = (gchar *)xmlGetProp (node, (const guchar *)"source")))
				src = (gchar *)xmlStrdup ((const guchar *)"local");  /* default to all local folders? */

			xmlSetProp (node, (const guchar *)"source", (const guchar *)"incoming");

			if (!(sources = xml_find_node (node, "sources")))
				sources = xmlNewChild (node, NULL, (const guchar *)"sources", NULL);

			xmlSetProp (sources, (const guchar *)"with", (guchar *)src);
			xmlFree (src);
		}

		node = node->next;
	}
}

static gchar *
get_nth_sig (gint id)
{
	ESignatureList *list;
	ESignature *sig;
	EIterator *iter;
	gchar *uid = NULL;
	gint i = 0;

	list = mail_config_get_signatures ();
	iter = e_list_get_iterator ((EList *) list);

	while (e_iterator_is_valid (iter) && i < id) {
		e_iterator_next (iter);
		i++;
	}

	if (i == id && e_iterator_is_valid (iter)) {
		sig = (ESignature *) e_iterator_get (iter);
		uid = g_strdup (e_signature_get_uid (sig));
	}

	g_object_unref (iter);

	return uid;
}

static void
em_upgrade_accounts_1_4 (void)
{
	EAccountList *accounts;
	EIterator *iter;

	if (!(accounts = mail_config_get_accounts ()))
		return;

	iter = e_list_get_iterator ((EList *) accounts);
	while (e_iterator_is_valid (iter)) {
		EAccount *account = (EAccount *) e_iterator_get (iter);
		gchar *url;

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
			gint sig_id;

			sig_id = strtol (account->id->sig_uid + 2, NULL, 10);
			g_free (account->id->sig_uid);
			account->id->sig_uid = get_nth_sig (sig_id);
		}

		e_iterator_next (iter);
	}

	g_object_unref (iter);

	mail_config_save_accounts ();
}

static gint
em_migrate_pop_uid_caches_1_4 (const gchar *evolution_dir, CamelException *ex)
{
	GString *oldpath, *newpath;
	struct dirent *dent;
	gsize olen, nlen;
	gchar *cache_dir;
	DIR *dir;
	gint res = 0;

	/* Sigh, too many unique strings to translate, for cases which shouldn't ever happen */

	/* open the old cache dir */
	cache_dir = g_build_filename (g_get_home_dir (), "evolution", "mail", "pop3", NULL);
	if (!(dir = opendir (cache_dir))) {
		if (errno == ENOENT) {
			g_free(cache_dir);
			return 0;
		}

		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Unable to open old POP keep-on-server data `%s': %s"),
				      cache_dir, g_strerror (errno));
		g_free (cache_dir);
		return -1;
	}

	oldpath = g_string_new (cache_dir);
	g_string_append_c (oldpath, '/');
	olen = oldpath->len;
	g_free (cache_dir);

	cache_dir = g_build_filename (evolution_dir, "mail", "pop", NULL);
	if (g_mkdir_with_parents (cache_dir, 0777) == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Unable to create POP3 keep-on-server data directory `%s': %s"),
				      cache_dir, g_strerror(errno));
		g_string_free (oldpath, TRUE);
		g_free (cache_dir);
		closedir (dir);
		return -1;
	}

	newpath = g_string_new (cache_dir);
	g_string_append_c (newpath, '/');
	nlen = newpath->len;
	g_free (cache_dir);

	while (res == 0 && (dent = readdir (dir))) {
		if (strncmp (dent->d_name, "cache-pop:__", 12) != 0)
			continue;

		g_string_truncate (oldpath, olen);
		g_string_truncate (newpath, nlen);

		g_string_append (oldpath, dent->d_name);
		g_string_append (newpath, dent->d_name + 12);

		/* strip the trailing '_' */
		g_string_truncate (newpath, newpath->len - 1);

		if (g_mkdir_with_parents (newpath->str, 0777) == -1
		    || cp(oldpath->str, (g_string_append(newpath, "/uid-cache"))->str, FALSE, CP_UNIQUE)) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      _("Unable to copy POP3 keep-on-server data `%s': %s"),
					      oldpath->str, g_strerror(errno));
			res = -1;
		}

	}

	g_string_free (oldpath, TRUE);
	g_string_free (newpath, TRUE);

	closedir (dir);

	return res;
}

static gint
em_migrate_imap_caches_1_4 (const gchar *evolution_dir, CamelException *ex)
{
	gchar *src, *dest;
	struct stat st;

	src = g_build_filename (g_get_home_dir (), "evolution", "mail", "imap", NULL);
	if (stat (src, &st) == -1 || !S_ISDIR (st.st_mode)) {
		g_free (src);
		return 0;
	}

	dest = g_build_filename (evolution_dir, "mail", "imap", NULL);

	/* we don't care if this fails, it's only a cache... */
	cp_r (src, dest, "summary", CP_OVERWRITE);

	g_free (dest);
	g_free (src);

	return 0;
}

static gint
em_migrate_folder_expand_state_1_4 (const gchar *evolution_dir, CamelException *ex)
{
	GString *srcpath, *destpath;
	gsize slen, dlen, rlen;
	gchar *evo14_mbox_root;
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
	if (g_mkdir_with_parents (destpath->str, 0777) == -1 || !(dir = opendir (srcpath->str))) {
		g_string_free (destpath, TRUE);
		g_string_free (srcpath, TRUE);
		return 0;
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
		gchar *full_name, *inptr, *buf = NULL;
		const gchar *filename;
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

		cp (srcpath->str, destpath->str, FALSE, CP_UNIQUE);

		g_string_truncate (srcpath, slen);
		g_string_truncate (destpath, dlen);
	}

	closedir (dir);

	g_free (evo14_mbox_root);
	g_string_free (destpath, TRUE);
	g_string_free (srcpath, TRUE);

	return 0;
}

static gint
em_migrate_folder_view_settings_1_4 (const gchar *evolution_dir, CamelException *ex)
{
	GString *srcpath, *destpath;
	gsize slen, dlen, rlen;
	gchar *evo14_mbox_root;
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
	if (g_mkdir_with_parents (destpath->str, 0777) == -1 || !(dir = opendir (srcpath->str))) {
		g_string_free (destpath, TRUE);
		g_string_free (srcpath, TRUE);
		return 0;
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
		gchar *full_name, *inptr, *buf = NULL;
		const gchar *filename, *ext;
		gsize prelen = 0;
		GString *new;

		if (dent->d_name[0] == '.')
			continue;

		if (!(ext = strrchr (dent->d_name, '.')))
			continue;

		if (!strcmp (ext, ".galview") || !strcmp ((gchar *)dent->d_name, "galview.xml")) {
			/* just copy the file */
			filename = dent->d_name;
			goto copy;
		} else if (strcmp (ext, ".xml") != 0) {
			continue;
		}

		if (!strncmp ((const gchar *)dent->d_name, "current_view-", 13)) {
			prelen = 13;
		} else if (!strncmp ((const gchar *)dent->d_name, "custom_view-", 12)) {
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

		cp (srcpath->str, destpath->str, FALSE, CP_UNIQUE);

		g_string_truncate (srcpath, slen);
		g_string_truncate (destpath, dlen);
	}

	closedir (dir);

	g_free (evo14_mbox_root);
	g_string_free (destpath, TRUE);
	g_string_free (srcpath, TRUE);

	return 0;
}

#define SUBFOLDER_DIR_NAME     "subfolders"
#define SUBFOLDER_DIR_NAME_LEN 10

static gchar *
e_path_to_physical (const gchar *prefix, const gchar *vpath)
{
	const gchar *p, *newp;
	gchar *dp;
	gchar *ppath;
	gint ppath_len;
	gint prefix_len;

	while (*vpath == '/')
		vpath++;
	if (!prefix)
		prefix = "";

	/* Calculate the length of the real path. */
	ppath_len = strlen (vpath);
	ppath_len++;	/* For the ending zero.  */

	prefix_len = strlen (prefix);
	ppath_len += prefix_len;
	ppath_len++;	/* For the separating slash.  */

	/* Take account of the fact that we need to translate every
	 * separator into `subfolders/'.
	 */
	p = vpath;
	while (1) {
		newp = strchr (p, '/');
		if (newp == NULL)
			break;

		ppath_len += SUBFOLDER_DIR_NAME_LEN;
		ppath_len++; /* For the separating slash.  */

		/* Skip consecutive slashes.  */
		while (*newp == '/')
			newp++;

		p = newp;
	};

	ppath = g_malloc (ppath_len);
	dp = ppath;

	memcpy (dp, prefix, prefix_len);
	dp += prefix_len;
	*(dp++) = '/';

	/* Copy the mangled path.  */
	p = vpath;
	while (1) {
		newp = strchr (p, '/');
		if (newp == NULL) {
			strcpy (dp, p);
			break;
		}

		memcpy (dp, p, newp - p + 1); /* `+ 1' to copy the slash too.  */
		dp += newp - p + 1;

		memcpy (dp, SUBFOLDER_DIR_NAME, SUBFOLDER_DIR_NAME_LEN);
		dp += SUBFOLDER_DIR_NAME_LEN;

		*(dp++) = '/';

		/* Skip consecutive slashes.  */
		while (*newp == '/')
			newp++;

		p = newp;
	}

	return ppath;
}

static gint
em_migrate_imap_cmeta_1_4(const gchar *evolution_dir, CamelException *ex)
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
		gchar *name, *path;

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
					gchar *dir, *base;

					base = g_strdup_printf("%s/mail/imap/%s@%s/folders",
							       evolution_dir,
							       url->user?url->user:"",
							       url->host?url->host:"");

					dir = e_path_to_physical(base, path);
					if (g_mkdir_with_parents(dir, 0777) == 0) {
						gchar *cmeta;
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

static void
remove_system_searches(xmlDocPtr searches)
{
	xmlNodePtr node;

	/* in pre 2.0, system searches were stored in the user
	 * searches.xml file with the source set to 'demand'.  In 2.0+
	 * the system searches are stored in the system
	 * searchtypes.xml file instead */

	node = xmlDocGetRootElement(searches);
	if (!node->name || strcmp((gchar *)node->name, "filteroptions"))
		return;

	if (!(node = xml_find_node(node, "ruleset")))
		return;

	node = node->children;
	while (node != NULL) {
		xmlNodePtr nnode = node->next;

		if (node->name && !strcmp ((gchar *)node->name, "rule")) {
			gchar *src;

			src = (gchar *)xmlGetProp(node, (guchar *)"source");
			if (src && !strcmp((gchar *)src, "demand")) {
				xmlUnlinkNode(node);
				xmlFreeNodeList(node);
			}
			xmlFree (src);
		}

		node = nnode;
	}
}

static gint
em_migrate_1_4 (const gchar *evolution_dir, xmlDocPtr filters, xmlDocPtr vfolders, CamelException *ex)
{
	EMMigrateSession *session;
	CamelException lex;
	struct stat st;
	gchar *path;
	xmlDocPtr searches;

	path = g_build_filename (evolution_dir, "mail", NULL);

	camel_init (path, TRUE);
	camel_provider_init();
	session = (EMMigrateSession *) em_migrate_session_new (path);
	g_free (path);

	session->srcdir = g_build_filename (g_get_home_dir (), "evolution", "local", NULL);

	path = g_strdup_printf ("mbox:%s/.evolution/mail/local", g_get_home_dir ());
	if (stat (path + 5, &st) == -1) {
		if (errno != ENOENT || g_mkdir_with_parents (path + 5, 0777) == -1) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      _("Failed to create local mail storage `%s': %s"),
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
				      _("Failed to create local mail storage `%s': %s"),
				      path, lex.desc);
		g_free (session->srcdir);
		camel_object_unref (session);
		camel_exception_clear (&lex);
		g_free (path);
		return -1;
	}
	g_free (path);

	if (em_migrate_local_folders_1_4 (session, ex) == -1)
		return -1;

	camel_object_unref (session->store);
	g_free (session->srcdir);

	camel_object_unref (session);

	em_upgrade_accounts_1_4();

	upgrade_xml_uris(filters, upgrade_xml_uris_1_4);
	upgrade_vfolder_sources_1_4(vfolders);
	upgrade_xml_uris(vfolders, upgrade_xml_uris_1_4);

	path = g_build_filename(g_get_home_dir(), "evolution", NULL);
	searches = emm_load_xml(path, "searches.xml");
	g_free(path);
	if (searches) {
		remove_system_searches(searches);
		path = g_build_filename(evolution_dir, "mail", NULL);
		emm_save_xml(searches, path, "searches.xml");
		g_free(path);
		xmlFreeDoc(searches);
	}

	if (em_migrate_pop_uid_caches_1_4 (evolution_dir, ex) == -1)
		return -1;

	/* these are non-fatal */
	em_migrate_imap_caches_1_4 (evolution_dir, ex);
	camel_exception_clear(ex);
	em_migrate_folder_expand_state_1_4 (evolution_dir, ex);
	camel_exception_clear(ex);
	em_migrate_folder_view_settings_1_4 (evolution_dir, ex);
	camel_exception_clear(ex);
	em_migrate_imap_cmeta_1_4(evolution_dir, ex);
	camel_exception_clear(ex);

	return 0;
}

static void
em_update_accounts_2_11 (void)
{
	EAccountList *accounts;
	EIterator *iter;
	gboolean changed = FALSE;

	if (!(accounts = mail_config_get_accounts ()))
		return;

	iter = e_list_get_iterator ((EList *) accounts);
	while (e_iterator_is_valid (iter)) {
		EAccount *account = (EAccount *) e_iterator_get (iter);

		if (g_str_has_prefix (account->source->url, "spool://")) {
			if (g_file_test (account->source->url + 8, G_FILE_TEST_IS_DIR)) {
				gchar *str = g_strdup_printf ("spooldir://%s", account->source->url + 8);

				g_free (account->source->url);
				account->source->url = str;
				changed = TRUE;
			}
		}

		e_iterator_next (iter);
	}

	g_object_unref (iter);

	if (changed)
		mail_config_save_accounts ();
}

#endif	/* !G_OS_WIN32 */

static gint
emm_setup_initial(const gchar *evolution_dir)
{
	GDir *dir;
	const gchar *d;
	gchar *local = NULL, *base;
	const gchar * const *language_names;

	/* special-case - this means brand new install of evolution */
	/* FIXME: create default folders and stuff... */

	d(printf("Setting up initial mail tree\n"));

	base = g_build_filename(evolution_dir, "mail", "local", NULL);
	if (g_mkdir_with_parents(base, 0777) == -1 && errno != EEXIST) {
		g_free(base);
		return -1;
	}

	/* e.g. try en-AU then en, etc */
	language_names = g_get_language_names ();
	while (*language_names != NULL) {
		local = g_build_filename (
			EVOLUTION_PRIVDATADIR, "default",
			*language_names, "mail", "local", NULL);
		if (g_file_test (local, G_FILE_TEST_EXISTS))
			break;
		g_free (local);
		language_names++;
	}

	/* Make sure we found one. */
	g_return_val_if_fail (*language_names != NULL, 0);

	dir = g_dir_open(local, 0, NULL);
	if (dir) {
		while ((d = g_dir_read_name(dir))) {
			gchar *src, *dest;

			src = g_build_filename(local, d, NULL);
			dest = g_build_filename(base, d, NULL);

			cp(src, dest, FALSE, CP_UNIQUE);
			g_free(dest);
			g_free(src);
		}
		g_dir_close(dir);
	}

	g_free(local);
	g_free(base);

	return 0;
}

static gboolean
is_in_plugs_list (GSList *list, const gchar *value)
{
	GSList *l;

	for (l = list; l; l = l->next) {
		if (l->data && !strcmp (l->data, value))
			return TRUE;
	}

	return FALSE;
}

/*
 * em_update_message_notify_settings_2_21
 * DBus plugin and sound email notification was merged to mail-notification plugin,
 * so move these options to new locations.
 */
static void
em_update_message_notify_settings_2_21 (void)
{
	GConfClient *client;
	GConfValue  *is_key;
	gboolean dbus, status;
	GSList *list;
	gchar *str;
	gint val;

	client = gconf_client_get_default ();

	is_key = gconf_client_get (client, "/apps/evolution/eplugin/mail-notification/dbus-enabled", NULL);
	if (is_key) {
		/* already migrated, so do not migrate again */
		gconf_value_free (is_key);
		g_object_unref (client);

		return;
	}

	gconf_client_set_bool (client, "/apps/evolution/eplugin/mail-notification/status-blink-icon",
				gconf_client_get_bool (client, "/apps/evolution/mail/notification/blink-status-icon", NULL), NULL);
	gconf_client_set_bool (client, "/apps/evolution/eplugin/mail-notification/status-notification",
				gconf_client_get_bool (client, "/apps/evolution/mail/notification/notification", NULL), NULL);

	list = gconf_client_get_list (client, "/apps/evolution/eplugin/disabled", GCONF_VALUE_STRING, NULL);
	dbus = !is_in_plugs_list (list, "org.gnome.evolution.new_mail_notify");
	status = !is_in_plugs_list (list, "org.gnome.evolution.mail_notification");

	gconf_client_set_bool (client, "/apps/evolution/eplugin/mail-notification/dbus-enabled", dbus, NULL);
	gconf_client_set_bool (client, "/apps/evolution/eplugin/mail-notification/status-enabled", status, NULL);

	if (!status) {
		/* enable this plugin, because it holds all those other things */
		GSList *plugins, *l;

		plugins = e_plugin_list_plugins ();

		for (l = plugins; l; l = l->next) {
			EPlugin *p = l->data;

			if (p && p->id && !strcmp (p->id, "org.gnome.evolution.mail_notification")) {
				e_plugin_enable (p, 1);
				break;
			}
		}

		g_slist_foreach (plugins, (GFunc)g_object_unref, NULL);
		g_slist_free (plugins);
	}

	g_slist_foreach (list, (GFunc) g_free, NULL);
	g_slist_free (list);

	val = gconf_client_get_int (client, "/apps/evolution/mail/notify/type", NULL);
	gconf_client_set_bool (client, "/apps/evolution/eplugin/mail-notification/sound-enabled", val == 1 || val == 2, NULL);
	gconf_client_set_bool (client, "/apps/evolution/eplugin/mail-notification/sound-beep", val == 0 || val == 1, NULL);

	str = gconf_client_get_string (client, "/apps/evolution/mail/notify/sound", NULL);
	gconf_client_set_string (client, "/apps/evolution/eplugin/mail-notification/sound-file", str ? str : "", NULL);
	g_free (str);

	g_object_unref (client);
}

/* fixing typo in SpamAssassin name */
static void
em_update_sa_junk_setting_2_23 (void)
{
	GConfClient *client;
	GConfValue  *key;

	client = gconf_client_get_default ();

	key = gconf_client_get (client, "/apps/evolution/mail/junk/default_plugin", NULL);
	if (key) {
		const gchar *str = gconf_value_get_string (key);

		if (str && strcmp (str, "Spamassasin") == 0)
			gconf_client_set_string (client, "/apps/evolution/mail/junk/default_plugin", "SpamAssassin", NULL);

		gconf_value_free (key);
		g_object_unref (client);

		return;
	}

	g_object_unref (client);
}

static gboolean
update_progress_in_main_thread (double *progress)
{
		em_migrate_set_progress (*progress);
		return FALSE;
}

static void
migrate_folders(CamelStore *store, gboolean is_local, CamelFolderInfo *fi, const gchar *acc, CamelException *ex, gboolean *done, gint *nth_folder, gint total_folders)
{
	CamelFolder *folder;

	while (fi) {
		double progress;
		gchar *tmp;

		*nth_folder = *nth_folder + 1;

		tmp = g_strdup_printf ("%s/%s", acc, fi->full_name);
		em_migrate_set_folder_name (tmp);
		g_free (tmp);

		progress = (double) (*nth_folder) / total_folders;
		g_idle_add ((GSourceFunc) update_progress_in_main_thread, &progress);

		if (is_local)
				folder = camel_store_get_folder (store, fi->full_name, CAMEL_STORE_IS_MIGRATING, ex);
		else
				folder = camel_store_get_folder (store, fi->full_name, 0, ex);

		if (folder != NULL)
			camel_folder_summary_migrate_infos (folder->summary);

		migrate_folders(store, is_local, fi->child, acc, ex, done, nth_folder, total_folders);

		fi = fi->next;
	}

	if ( (*nth_folder) == (total_folders - 1))
		*done = TRUE;
}

/* This could be in CamelStore.ch */
static void
count_folders (CamelFolderInfo *fi, gint *count)
{
	while (fi) {
		*count = *count + 1;
		count_folders (fi->child , count);
		fi = fi->next;
	}
}

static CamelStore *
setup_local_store (MailComponent *mc)
{
	CamelURL *url;
	gchar *tmp;
	CamelStore *store;

	url = camel_url_new("mbox:", NULL);
	tmp = g_build_filename (mail_component_peek_base_directory(mc), "local", NULL);
	camel_url_set_path(url, tmp);
	g_free(tmp);
	tmp = camel_url_to_string(url, 0);
	store = (CamelStore *)camel_session_get_service(session, tmp, CAMEL_PROVIDER_STORE, NULL);
	g_free(tmp);

	return store;

}

struct migrate_folders_to_db_structure {
		gchar *account_name;
		CamelException ex;
		CamelStore *store;
		CamelFolderInfo *info;
		gboolean done;
		gboolean is_local_store;
};

static void migrate_folders_to_db_thread (struct migrate_folders_to_db_structure *migrate_dbs)
{
		gint num_of_folders = 0, nth_folder = 0;
		count_folders (migrate_dbs->info, &num_of_folders);
		migrate_folders (migrate_dbs->store, migrate_dbs->is_local_store, migrate_dbs->info,
						migrate_dbs->account_name, &(migrate_dbs->ex), &(migrate_dbs->done),
						&nth_folder, num_of_folders);
}

static void
migrate_to_db()
{
		EAccountList *accounts;
		EIterator *iter;
		gint i=0, len;
		MailComponent *component = mail_component_peek ();
		CamelStore *store = NULL;
		CamelFolderInfo *info;

		if (!(accounts = mail_config_get_accounts ()))
				return;

		iter = e_list_get_iterator ((EList *) accounts);
		len = e_list_length ((EList *) accounts);

		camel_session_set_online ((CamelSession *) session, FALSE);

	em_migrate_setup_progress_dialog (_("Migrating Folders"), _("The summary format of the Evolution mailbox "
								"folders has been moved to SQLite since Evolution 2.24.\n\nPlease be "
								"patient while Evolution migrates your folders..."));

		store = setup_local_store (component);
		info = camel_store_get_folder_info (store, NULL, CAMEL_STORE_FOLDER_INFO_RECURSIVE|CAMEL_STORE_FOLDER_INFO_FAST|CAMEL_STORE_FOLDER_INFO_SUBSCRIBED, NULL);

		if (info) {
				GThread *thread;
				struct migrate_folders_to_db_structure migrate_dbs;

				if (g_str_has_suffix (((CamelService *)store)->url->path, ".evolution/mail/local"))
						migrate_dbs.is_local_store = TRUE;
				else
						migrate_dbs.is_local_store = FALSE;
				camel_exception_init (&migrate_dbs.ex);
				migrate_dbs.account_name = _("On This Computer");
				migrate_dbs.info = info;
				migrate_dbs.store = store;
				migrate_dbs.done = FALSE;

				thread = g_thread_create ((GThreadFunc) migrate_folders_to_db_thread, &migrate_dbs, TRUE, NULL);
				while (!migrate_dbs.done)
						g_main_context_iteration (NULL, TRUE);
		}
		i++;
		while (e_iterator_is_valid (iter)) {
				EAccount *account = (EAccount *) e_iterator_get (iter);
				EAccountService *service;
				const gchar *name;

				service = account->source;
				name = account->name;
				if (account->enabled
								&& service->url != NULL
								&& service->url[0]
								&& strncmp(service->url, "mbox:", 5) != 0) {

						CamelException ex;

						camel_exception_init (&ex);
						mail_component_load_store_by_uri (component, service->url, name);

						store = (CamelStore *) camel_session_get_service (session, service->url, CAMEL_PROVIDER_STORE, &ex);
						info = camel_store_get_folder_info (store, NULL, CAMEL_STORE_FOLDER_INFO_RECURSIVE|CAMEL_STORE_FOLDER_INFO_FAST|CAMEL_STORE_FOLDER_INFO_SUBSCRIBED, &ex);
						if (info) {
								GThread *thread;
								struct migrate_folders_to_db_structure migrate_dbs;

								migrate_dbs.ex = ex;
								migrate_dbs.account_name = account->name;
								migrate_dbs.info = info;
								migrate_dbs.store = store;
								migrate_dbs.done = FALSE;

								thread = g_thread_create ((GThreadFunc) migrate_folders_to_db_thread, &migrate_dbs, TRUE, NULL);
								while (!migrate_dbs.done)
										g_main_context_iteration (NULL, TRUE);
						} else
								printf("%s:%s: failed to get folder infos \n", G_STRLOC, G_STRFUNC);
						camel_exception_clear(&ex);
				}
				i++;
				e_iterator_next (iter);
		}
		/* camel_session_set_online ((CamelSession *) session, TRUE); */
		g_object_unref (iter);
		em_migrate_close_progress_dialog ();
}

gint
em_migrate (const gchar *evolution_dir, gint major, gint minor, gint revision, CamelException *ex)
{
	struct stat st;
	gchar *path;

	/* make sure ~/.evolution/mail exists */
	path = g_build_filename (evolution_dir, "mail", NULL);
	if (g_stat (path, &st) == -1) {
		if (errno != ENOENT || g_mkdir_with_parents (path, 0777) == -1) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      _("Unable to create local mail folders at `%s': %s"),
					      path, g_strerror (errno));
			g_free (path);
			return -1;
		}
	}

	g_free (path);

	if (major == 0)
		return emm_setup_initial(evolution_dir);

	if (major == 1 && minor < 5) {
#ifndef G_OS_WIN32
		xmlDocPtr config_xmldb = NULL, filters, vfolders;

		path = g_build_filename (g_get_home_dir (), "evolution", NULL);
		if (minor <= 2 && !(config_xmldb = emm_load_xml (path, "config.xmldb"))) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      _("Unable to read settings from previous Evolution install, "
						"`evolution/config.xmldb' does not exist or is corrupt."));
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
#else
		g_error ("Upgrading from ancient versions not supported on Windows");
#endif
	}

	if (major < 2 || (major == 2 && minor < 12)) {
#ifndef G_OS_WIN32
		em_update_accounts_2_11 ();
#else
		g_error ("Upgrading from ancient versions not supported on Windows");
#endif
	}

	if (major < 2 || (major == 2 && minor < 22))
		em_update_message_notify_settings_2_21 ();

	if (major < 2 || (major == 2 && minor < 24)) {
		em_update_sa_junk_setting_2_23 ();
		migrate_to_db ();
	}

	return 0;
}
