/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-upgrade.c - upgrade previous config versions
 *
 * Copyright (C) 2003 Ximian, Inc.
 *
 * Authors: Michael Zucchi <notzed@ximian.com>
 *	    Jeffery Stedfast <fejj@ximian.com>
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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <stdio.h>
#include <errno.h>
#include <regex.h>
#include <string.h>
#include <ctype.h>

#include <glib.h>
#include <gconf/gconf.h>
#include <gconf/gconf-client.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include "e-config-upgrade.h"

#define d(x) 

/* output revision of configuration */
#define CONF_MAJOR (1)
#define CONF_MINOR (3)
#define CONF_REVISION (1)

/* major/minor/revision of existing config */
static unsigned int major = -1;
static unsigned int minor = -1;
static unsigned int revision = -1;

/* 1.0 details, if required */
static GHashTable *accounts_1_0 = NULL;
static GHashTable *accounts_name_1_0 = NULL;

/* where files are stored */
static const char *evolution_dir;

static char hexnib[256] = {
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,-1,-1,-1,-1,-1,-1,
	-1,10,11,12,13,14,15,16,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,10,11,12,13,14,15,16,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
};

static char *hex_decode(const char *val)
{
	char *o, *res;
	const unsigned char *p = (const unsigned char *)val;
	
	o = res = g_malloc(strlen(val)/2 + 1);
	for (p=val;(p[0] && p[1]);p+=2)
		*o++ = (hexnib[p[0]] << 4) | hexnib[p[1]];
	*o = 0;

	return res;
}

static char *url_decode(const char *val)
{
	char *o, *res, c;
	const unsigned char *p = (const unsigned char *)val;
	
	o = res = g_malloc(strlen(val) + 1);
	while (*p) {
		c = *p++;
		if (c == '%'
		    && hexnib[p[0]] != -1 && hexnib[p[1]] != -1) {
			*o++ = (hexnib[p[0]] << 4) | hexnib[p[1]];
			p+=2;
		} else
			*o++ = c;
	}
	*o = 0;

	return res;
}

/* so we dont need camel, just copy here */
static int
camel_file_util_decode_uint32 (FILE *in, guint32 *dest)
{
        guint32 value = 0;
	int v;

        /* until we get the last byte, keep decoding 7 bits at a time */
        while ( ((v = fgetc (in)) & 0x80) == 0 && v!=EOF) {
                value |= v;
                value <<= 7;
        }
	if (v == EOF) {
		*dest = value >> 7;
		return -1;
	}
	*dest = value | (v & 0x7f);

        return 0;
}

static int
camel_file_util_decode_string (FILE *in, char **str)
{
	guint32 len;
	register char *ret;

	if (camel_file_util_decode_uint32 (in, &len) == -1) {
		*str = NULL;
		return -1;
	}

	len--;
	if (len > 65536) {
		*str = NULL;
		return -1;
	}

	ret = g_malloc (len+1);
	if (len > 0 && fread (ret, len, 1, in) != 1) {
		g_free (ret);
		*str = NULL;
		return -1;
	}

	ret[len] = 0;
	*str = ret;
	return 0;
}

/* For 1.0.8 conversion */

/* as much info as we have on a given account */
struct _account_info {
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

struct _imap_folder_info {
	char *folder;
	/* encoded?  decoded?  canonicalised? */
	char dir_sep;
};

static char *parse_lsub(const char *lsub, char *dir_sep)
{
	static int comp;
	static regex_t pat;
	regmatch_t match[3];
	char *m = "^\\* LSUB \\([^)]*\\) \"?([^\" ]+)\"? \"?(.*)\"?$";

	if (!comp) {
		if (regcomp(&pat, m, REG_EXTENDED|REG_ICASE) == -1) {
			g_warning("reg comp '%s' failed: %s", m, g_strerror(errno));
			return NULL;
		}
		comp = 1;
	}

	if (regexec(&pat, lsub, 3, match, 0) == 0) {
		if (match[1].rm_so != -1 && match[2].rm_so != -1) {
			if (dir_sep)
				*dir_sep = (match[1].rm_eo - match[1].rm_so == 1) ? lsub[match[1].rm_so] : 0;
			return g_strndup(lsub + match[2].rm_so, match[2].rm_eo - match[2].rm_so);
		}
	}

	return NULL;
}

static int read_imap_storeinfo(struct _account_info *si)
{
	FILE *storeinfo;
	guint32 tmp;
	char *buf, *folder, dir_sep, *path, *name, *p;
	struct _imap_folder_info *fi;

	si->u.imap.folders = g_hash_table_new(g_str_hash, g_str_equal);

	/* get details from uri first */
	name = strstr(si->uri, ";override_namespace");
	if (name) {
		name = strstr(si->uri, ";namespace=");
		if (name) {
			char *end;

			name += strlen(";namespace=");
			if (*name == '\"') {
				name++;
				end = strchr(name, '\"');
			} else {
				end = strchr(name, ';');
			}

			if (end) {
				/* try get the dir_sep from the namespace */
				si->u.imap.namespace = g_strndup(name, end-name);

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
	path = g_build_filename(evolution_dir, "mail/imap", si->base_uri+7, "storeinfo", NULL);
	storeinfo = fopen(path, "r");
	g_free(path);
	if (storeinfo == NULL) {
		g_warning("could not find imap store info '%s'", path);
		return -1;
	}

	/* ignore version */
	camel_file_util_decode_uint32(storeinfo, &tmp);
	camel_file_util_decode_uint32(storeinfo, &si->u.imap.capabilities);
	g_free(si->u.imap.namespace);
	camel_file_util_decode_string (storeinfo, &si->u.imap.namespace);
	camel_file_util_decode_uint32 (storeinfo, &tmp);
	si->u.imap.dir_sep = tmp;
	/* strip trailing dir_sep or / */
	if (si->u.imap.namespace
	    && (si->u.imap.namespace[strlen(si->u.imap.namespace)-1] == si->u.imap.dir_sep
		|| si->u.imap.namespace[strlen(si->u.imap.namespace)-1] == '/')) {
		si->u.imap.namespace[strlen(si->u.imap.namespace)-1] = 0;
	}

	d(printf("namespace '%s' dir_sep '%c'\n", si->u.imap.namespace, si->u.imap.dir_sep?si->u.imap.dir_sep:'?'));

	while (camel_file_util_decode_string (storeinfo, &buf) == 0) {
		folder = parse_lsub(buf, &dir_sep);
		if (folder) {
			fi = g_malloc0(sizeof(*fi));
			fi->folder = folder;
			fi->dir_sep = dir_sep;
#if d(!)0
			printf(" add folder '%s' ", folder);
			if (dir_sep)
				printf("'%c'\n", dir_sep);
			else
				printf("NIL\n");
#endif
			g_hash_table_insert(si->u.imap.folders, fi->folder, fi);
		} else {
			g_warning("Could not parse LIST result '%s'\n", buf);
		}
	}
	fclose(storeinfo);

	return 0;
}

static char *get_base_uri(const char *val)
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

static char *upgrade_uri(const char *uri)
{
	char *out = NULL;

	/* upgrades camel uri's */

	if (major <=1 && minor < 2) {
		if (strncmp(uri, "imap:", 5) == 0) {
			char *base_uri, dir_sep, *folder, *p;
			struct _account_info *ai;

			/* add namespace, canonicalise dir_sep to / */
			base_uri = get_base_uri(uri);
			ai = g_hash_table_lookup(accounts_1_0, base_uri);

			if (ai == NULL) {
				g_free(base_uri);
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
					g_free(base_uri);
					return NULL;
				}
			}

			folder = g_strdup(uri + strlen(base_uri)+1);

			/* Add the namespace before the mailbox name, unless the mailbox is INBOX */
			if (ai->u.imap.namespace && strcmp(folder, "INBOX") != 0)
				out = g_strdup_printf("%s/%s/%s", base_uri, ai->u.imap.namespace, folder);
			else
				out = g_strdup_printf("%s/%s", base_uri, folder);

			p = out;
			while (*p) {
				if (*p == dir_sep)
					*p = '/';
				p++;
			}

			g_free(folder);
			g_free(base_uri);
		} else if (strncmp(uri, "exchange:", 9) == 0) {
			char *base_uri, *folder, *p;

			/*  exchange://user@host/exchange/ * -> exchange://user@host/personal/ * */
			/*  Any url encoding (%xx) in the folder name is also removed */
			base_uri = get_base_uri(uri);
			uri += strlen(base_uri) + 1;
			if (strncmp(uri, "exchange/", 9) == 0) {
				folder = url_decode(uri+9);
				p = strchr(folder, '/');
				out = g_strdup_printf("%s/personal%s", base_uri, p?p:"/");
				g_free(folder);
			}
		} else if (strncmp(uri, "exchanget:", 10) == 0) {
			/* these should be converted in the accounts table when it is loaded */
			g_warning("exchanget: uri not converted: '%s'", uri);
		}
	}

	return out;
}

static char *upgrade_evolution_uri(const char *uri)
{
	char *out = NULL;

	if (!strcmp (uri, "evolution:/local/Inbox")) {
		return g_strdup("default:mail");
	} else if (!strcmp (uri, "evolution:/local/Calendar")) {
		return g_strdup("default:calendar");
	} else if (!strcmp (uri, "evolution:/local/Contacts")) {
		return g_strdup("default:contacts");
	} else if (!strcmp (uri, "evolution:/local/Tasks")) {
		return g_strdup("default:tasks");
	} if (!strncmp(uri, "evolution:/", 11)) {
		char *account, *tmp, *folder, *p;
		struct _account_info *ai;

		d(printf("convert url '%s'\n", uri));
		tmp = strchr(uri+11, '/');
		if (tmp == NULL)
			return NULL;

		folder = *tmp?tmp+1:tmp;
		account = g_strndup(uri+11, tmp-uri-11);
		d(printf("  looking for account '%s'\n", account));
		ai = g_hash_table_lookup(accounts_name_1_0, account);
		if (ai && !strncmp(ai->base_uri, "imap:", 5)) {
			/* Add namespace to evolution url's of imap accounts, if the account uses a namespace */
			d(printf("found account ... '%s', folder = '%s'\n", ai->name, folder));
			
			if (ai->u.imap.namespace && strcmp(folder, "INBOX") != 0)
				out = g_strdup_printf("evolution:/%s/%s/%s", account, ai->u.imap.namespace, folder);
			else
				out = g_strdup_printf("evolution:/%s/%s", account, folder);
			
			if (ai->u.imap.dir_sep) {
				p = out + strlen("evolution://") + strlen(account);
				while (*p) {
					if (*p == ai->u.imap.dir_sep)
						*p = '/';
					p++;
				}
			}
		} else if (ai && !strncmp(ai->base_uri, "exchange:", 9)) {
			/* add personal to exchange url's */
			folder = url_decode(folder);
			out = g_strdup_printf("evolution:/%s/personal/%s", account, folder);
			g_free(folder);
		}
		g_free(account);
	}

	return out;
}

static char *upgrade_type(const char *type)
{
	char *res = NULL;

	/*
	  <item type="ldap-contacts ...>
	  to
	  <item type="contacts/ldap ...>
	*/

	if (strcmp(type, "ldap-contacts") == 0)
		res = g_strdup("contacts/ldap");

	return res;
}

enum {
	CONVERT_CONTENT,
	CONVERT_ARG,
};

struct _convert {
	int type;
	char *tag;
	char *arg;
	char *(*convert)(const char *val);
} convert_table [] = {
	/* fix evolution uri's in shortcuts file */
	{ CONVERT_CONTENT, "item", NULL, upgrade_evolution_uri },
	/* ldap item/type's converted from ldap-contacts to contacts/ldap */
	{ CONVERT_ARG,     "item", "type", upgrade_type },
	/* fix folder uri's in filters/vfolders */
	{ CONVERT_ARG,     "folder", "uri", upgrade_uri },
};

#define CONVERT_SIZEOF (sizeof(convert_table)/sizeof(convert_table[0]))

static int upgrade_xml_1_0_rec(xmlNodePtr node)
{
	int i;
	struct _convert *ct;
	int scan = TRUE;
	int work = FALSE;
	char *txt, *newtxt;

	for (i=0;i<CONVERT_SIZEOF;i++) {
		ct = &convert_table[i];
		if (!strcmp(node->name, ct->tag)) {
			switch(ct->type) {
			case CONVERT_CONTENT:
				txt = xmlNodeGetContent(node);
				newtxt = ct->convert(txt);
				d(printf("Upgrade content '%s': '%s' -> '%s'\n", ct->tag, txt, newtxt?newtxt:"unchanged"));
				xmlFree(txt);
				if (newtxt) {
					xmlNodeSetContent(node, newtxt);
					g_free(newtxt);
					work = TRUE;
				}
				scan = FALSE;
				break;
			case CONVERT_ARG:
				txt = xmlGetProp(node, ct->arg);
				if (txt) {
					newtxt = ct->convert(txt);
					d(printf("Upgrade prop '%s' '%s': '%s' -> '%s'\n", ct->tag, ct->arg, txt, newtxt?newtxt:"unchanged"));
					xmlFree(txt);
					if (newtxt) {
						xmlSetProp(node, ct->arg, newtxt);
						g_free(newtxt);
						work = TRUE;
					}
				}
				break;
			}
		}
	}

	if (scan) {
		node = node->children;
		while (node) {
			work |= upgrade_xml_1_0_rec(node);
			node = node->next;
		}
	}

	return work;
}

/* ********************************************************************** */
/* XML 1 content encoding */

static int
is_xml1encoded(const char *txt)
{
	const unsigned char *p;
	int isxml1 = FALSE;
	int is8bit = FALSE;

	p = (const unsigned char *)txt;
	while (*p) {
		if (p[0] == '\\' && p[1] == 'U' && p[2] == '+'
		    && isxdigit(p[3]) && isxdigit(p[4]) && isxdigit(p[5]) && isxdigit(p[6])
		    && p[7] == '\\') {
			isxml1 = TRUE;
			p+=7;
		} else if (p[0] >= 0x80)
			is8bit = TRUE;
		p++;
	}

	/* check for invalid utf8 that needs cleaning */
	if (is8bit && (!isxml1))
		isxml1 = !g_utf8_validate(txt, -1, NULL);

	return isxml1;
}

static char *
decode_xml1(const char *txt)
{
	GString *out = g_string_new("");
	const unsigned char *p;
	char *res;

	/* convert:
                   \U+XXXX\ -> utf8
	   8 bit characters -> utf8 (iso-8859-1) */

	p = (const unsigned char *)txt;
	while (*p) {
		if (p[0] > 0x80
		    || (p[0] == '\\' && p[1] == 'U' && p[2] == '+'
			&& isxdigit(p[3]) && isxdigit(p[4]) && isxdigit(p[5]) && isxdigit(p[6])
			&& p[7] == '\\')) {
			char utf8[8];
			gunichar u;

			if (p[0] == '\\') {
				memcpy(utf8, p+3, 4);
				utf8[4] = 0;
				u = strtoul(utf8, NULL, 16);
				p+=7;
			} else
				u = p[0];
			utf8[g_unichar_to_utf8(u, utf8)] = 0;
			g_string_append(out, utf8);
		} else {
			g_string_append_c(out, *p);
		}
		p++;
	}

	res = out->str;
	g_string_free(out, FALSE);

	return res;
}

static int
upgrade_xml_1_2_rec(xmlNodePtr node)
{
	const char *value_tags[] = { "string", "address", "regex", "file", "command", NULL };
	const char *rule_tags[] = { "title", NULL };
	const struct {
		char *name;
		const char **tags;
	} tags[] = {
		{ "value", value_tags },
		{ "rule", rule_tags },
		{ 0 },
	};
	int changed = 0;
	xmlNodePtr work;
	int i,j;
	char *txt, *tmp;

	/* upgrades the content of a node, if the node has a specific parent/node name */

	for (i=0;tags[i].name;i++) {
		if (!strcmp(node->name, tags[i].name)) {
			work = node->children;
			while (work) {
				for (j=0;tags[i].tags[j];j++) {
					if (!strcmp(work->name, tags[i].tags[j])) {
						txt = xmlNodeGetContent(work);
						if (is_xml1encoded(txt)) {
							tmp = decode_xml1(txt);
							d(printf("upgrading xml node %s/%s '%s' -> '%s'\n", tags[i].name, tags[i].tags[j], txt, tmp));
							xmlNodeSetContent(work, tmp);
							changed = 1;
							g_free(tmp);
						}
						xmlFree(txt);
					}
				}
				work = work->next;
			}
			break;
		}
	}

	node = node->children;
	while (node) {
		changed |= upgrade_xml_1_2_rec(node);
		node = node->next;
	}

	return changed;
}

/* ********************************************************************** */

static int upgrade_xml_file(const char *filename, int (*upgrade_rec)(xmlNodePtr root))
{
	xmlDocPtr doc;
	char *savename;
	struct stat st;
	int res;

	/* FIXME: do something nicer with the errors */

	savename = alloca(strlen(filename)+64);
	sprintf(savename, "%s.save-%u.%u.%u", filename, major, minor, revision);
	if (stat(savename, &st) == 0) {
		fprintf(stderr, "xml file `%s' already upgraded\n", filename);
		return 0;
	}

	/* no file, no error */
	if (stat(filename, &st) == -1)
		return 0;

	doc = xmlParseFile (filename);
	if (!doc || !doc->xmlRootNode) {
		fprintf (stderr, "Failed to load %s\n", filename);
		return -1;
	}

	if (!upgrade_rec(doc->xmlRootNode)) {
		xmlFreeDoc(doc);
		printf("file %s contains nothing to upgrade\n", filename);
		return 0;
	}
	
	d(printf("backing up %s to %s\n", filename, savename));

	if (rename(filename, savename) == -1) {
		xmlFreeDoc(doc);
		fprintf(stderr, "could not rename '%s' to '%s': %s\n", filename, savename, strerror(errno));
		return -1;
	}

	res = xmlSaveFormatFile(filename, doc, 1);

	xmlFreeDoc(doc);

	return res;
}

/* ********************************************************************** */
/*  Tables for converting flat bonobo conf -> gconf xml blob		  */
/* ********************************************************************** */

/* for remapping bonobo-conf account data into the new xml blob format */
/* These are used in build_xml, and order must match the lookup_table */
enum _map_t {
	MAP_END = 0,		/* end of line*/
	MAP_BOOL,		/* bool -> prop of name 'to' value true or false */
	MAP_LONG,		/* long -> prop of name 'to' value a long */
	MAP_STRING,		/* string -> prop of name 'to' */
	MAP_ENUM,		/* long/bool -> prop of name 'to', with the value indexed into the child map table's from field */
	MAP_CHILD,		/* a new child of name 'to' */
	MAP_MASK = 0x3f,
	MAP_URI_UPGRADE = 0x40,	/* if from 1.0.x, upgrade any uri's present */
	MAP_CONTENT = 0x80,	/* if set, create a new node of name 'to' instead of a property */
};

struct _map_table {
	char *from;
	char *to;
	int type;
	struct _map_table *child;
};

/* Mail/Accounts/ * */
struct _map_table cc_map[] = {
	{ "account_always_cc_%i", "always", MAP_BOOL },
	{ "account_always_cc_addrs_%i", "recipients", MAP_STRING|MAP_CONTENT },
	{ NULL },
};

struct _map_table bcc_map[] = {
	{ "account_always_cc_%i", "always", MAP_BOOL },
	{ "account_always_bcc_addrs_%i", "recipients", MAP_STRING|MAP_CONTENT },
	{ NULL },
};

struct _map_table pgp_map[] = {
	{ "account_pgp_encrypt_to_self_%i", "encrypt-to-self", MAP_BOOL },
	{ "account_pgp_always_trust_%i", "always-trust", MAP_BOOL },
	{ "account_pgp_always_sign_%i", "always-sign", MAP_BOOL },
	{ "account_pgp_no_imip_sign_%i", "no-imip-sign", MAP_BOOL },
	{ "account_pgp_key_%i", "key-id", MAP_STRING|MAP_CONTENT },
	{ NULL },
};

struct _map_table smime_map[] = {
	{ "account_smime_encrypt_to_self_%i", "encrypt-to-self", MAP_BOOL },
	{ "account_smime_always_sign_%i", "always-sign", MAP_BOOL },
	{ "account_smime_key_%i", "key-id", MAP_STRING|MAP_CONTENT },
	{ NULL },
};

struct _map_table identity_sig_map[] = {
	{ "identity_autogenerated_signature_%i", "auto", MAP_BOOL },
	{ "identity_def_signature_%i", "default", MAP_LONG },
	{ NULL },
};

struct _map_table identity_map[] = {
	{ "identity_name_%i", "name", MAP_STRING|MAP_CONTENT },
	{ "identity_address_%i", "addr-spec", MAP_STRING|MAP_CONTENT },
	{ "identity_reply_to_%i", "reply-to", MAP_STRING|MAP_CONTENT },
	{ "identity_organization_%i", "organization", MAP_STRING|MAP_CONTENT },
	{ NULL, "signature", MAP_CHILD, identity_sig_map },
	{ NULL },
};

struct _map_table source_map[] = {
	{ "source_save_passwd_%i", "save-passwd", MAP_BOOL },
	{ "source_keep_on_server_%i", "keep-on-server", MAP_BOOL },
	{ "source_auto_check_%i", "auto-check", MAP_BOOL },
	{ "source_auto_check_time_%i", "auto-check-timeout", MAP_LONG },
	{ "source_url_%i", "url", MAP_STRING|MAP_CONTENT },
	{ NULL },
};

struct _map_table transport_map[] = {
	{ "transport_save_passwd_%i", "save-passwd", MAP_BOOL },
	{ "transport_url_%i", "url", MAP_STRING|MAP_CONTENT|MAP_URI_UPGRADE },
	{ NULL },
};

struct _map_table account_map[] = {
	{ "account_name_%i", "name", MAP_STRING },
	{ "source_enabled_%i", "enabled", MAP_BOOL },
	{ NULL, "identity", MAP_CHILD, identity_map },
	{ NULL, "source", MAP_CHILD, source_map },
	{ NULL, "transport", MAP_CHILD, transport_map },
	{ "account_drafts_folder_uri_%i", "drafts-folder", MAP_STRING|MAP_CONTENT|MAP_URI_UPGRADE },
	{ "account_sent_folder_uri_%i", "sent-folder", MAP_STRING|MAP_CONTENT|MAP_URI_UPGRADE },
	{ NULL, "auto-cc", MAP_CHILD, cc_map },
	{ NULL, "auto-bcc", MAP_CHILD, bcc_map },
	{ NULL, "pgp", MAP_CHILD, pgp_map },
	{ NULL, "smime", MAP_CHILD, smime_map },
	{ NULL },
};

/* /Mail/Signatures/ * */
struct _map_table signature_format_map[] = {
	{ "text/plain", },
	{ "text/html", },
	{ NULL }
};

struct _map_table signature_map[] = {
	{ "name_%i", "name", MAP_STRING },
	{ "html_%i", "format", MAP_ENUM, signature_format_map },
	{ "filename_%i", "filename", MAP_STRING|MAP_CONTENT },
	{ "script_%i", "script", MAP_STRING|MAP_CONTENT },
	{ NULL },
};


static char *get_name(const char *in, int index)
{
	char c, *res;
	GString *out = g_string_new("");

	while ( (c = *in++) ) {
		if (c == '%') {
			c = *in++;
			switch(c) {
			case '%':
				g_string_append_c(out, '%');
				break;
			case 'i':
				g_string_append_printf(out, "%d", index);
				break;
			}
		} else {
			g_string_append_c(out, c);
		}
	}

	res = out->str;
	g_string_free(out, FALSE);

	return res;
}

static xmlNodePtr lookup_bconf_entry(xmlNodePtr source, const char *name)
{
	xmlNodePtr node = source->children;
	int found;
	char *val;

	while (node) {
		if (!strcmp(node->name, "entry")) {
			val = xmlGetProp(node, "name");
			found = val && strcmp(val, name) == 0;
			xmlFree(val);
			if (found)
				break;
		}
		node = node->next;
	}

	return node;
}

static char *lookup_bconf_value(xmlNodePtr source, const char *name)
{
	xmlNodePtr node = lookup_bconf_entry(source, name);

	if (node)
		return xmlGetProp(node, "value");
	else
		return NULL;
}

static xmlNodePtr lookup_bconf_path(xmlDocPtr doc, const char *path)
{
	xmlNodePtr root;
	char *val;
	int found;

	root = doc->children;
	if (strcmp(root->name, "bonobo-config") != 0) {
		g_warning("not bonobo-config xml file\n");
		return NULL;
	}

	root = root->children;
	while (root) {
		if (!strcmp(root->name, "section")) {
			val = xmlGetProp(root, "path");
			found = val && strcmp(val, path) == 0;
			xmlFree(val);
			if (found)
				break;
		}
		root = root->next;
	}

	return root;
}


static char *lookup_bool(xmlNodePtr source, const char *name, struct _map_table *map)
{
	char *val, *res;

	val = lookup_bconf_value(source, name);
	if (val) {
		res = g_strdup(val[0] == '1'?"true":"false");
		xmlFree(val);
	} else
		res = NULL;
	
	return res;
}

static char *lookup_long(xmlNodePtr source, const char *name, struct _map_table *map)
{
	char *val, *res;

	val = lookup_bconf_value(source, name);
	if (val) {
		res = g_strdup(val);
		xmlFree(val);
	} else
		res = NULL;
	
	return res;
}

static char *lookup_string(xmlNodePtr source, const char *name, struct _map_table *map)
{
	char *val, *res;

	val = lookup_bconf_value(source, name);
	if (val) {
		res = hex_decode(val);
		xmlFree(val);
	} else
		res = NULL;

	return res;
}

static char *lookup_enum(xmlNodePtr source, const char *name, struct _map_table *map)
{
	char *val;
	int index = 0, i;

	val = lookup_bconf_value(source, name);
	if (val) {
		index = atoi(val);
		xmlFree(val);
	}

	for (i=0;map->child[i].from;i++)
		if (i == index)
			return g_strdup(map->child[i].from);

	return NULL;
}

typedef char * (*lookup_func) (xmlNodePtr, const char *, struct _map_table *);

static void
build_xml(xmlNodePtr root, struct _map_table *map, int index, xmlNodePtr source)
{
	char *name, *value;
	xmlNodePtr node;
	lookup_func lookup_table[] = { lookup_bool, lookup_long, lookup_string, lookup_enum };

	while (map->type != MAP_END) {
		if ((map->type & MAP_MASK) == MAP_CHILD) {
			node = xmlNewChild(root, NULL, map->to, NULL);
			build_xml(node, map->child, index, source);
		} else {
			name = get_name(map->from, index);
			value = lookup_table[(map->type&MAP_MASK)-1](source, name, map);

			d(printf("key '%s=%s' -> ", name, value));

			if (map->type & MAP_URI_UPGRADE) {
				char *tmp = value;

				value = upgrade_uri(tmp);
				if (value)
					g_free(tmp);
				else
					value = tmp;
			}

			d(printf("'%s=%s'\n", map->to, value));

			if (map->type & MAP_CONTENT) {
				if (value && value[0])
					xmlNewTextChild(root, NULL, map->to, value);
			} else {
				xmlSetProp(root, map->to, value);
			}
			g_free(value);
			g_free(name);
		}
		map++;
	}
}

static int convert_xml_blob(GConfClient *gconf, xmlDocPtr doc, struct _map_table *map, const char *path, const char *outpath, const char *name, const char *idparam)
{
	xmlNodePtr source;
	int count = 0, i;
	GSList *list, *l;
	char *val;

	source = lookup_bconf_path(doc, path);
	if (source) {
		list = NULL;
		val = lookup_bconf_value(source, "num");
		if (val) {
			count = atoi(val);
			xmlFree(val);
		}

		d(printf("Found %d blobs at %s\n", count, path));

		for (i = 0; i<count;i++) {
			xmlDocPtr docout;
			xmlChar *xmlbuf;
			int n;
			xmlNodePtr root;

			docout = xmlNewDoc ("1.0");
			root = xmlNewDocNode (docout, NULL, name, NULL);
			xmlDocSetRootElement (docout, root);

			/* This could be set with a MAP_UID type ... */
			if (idparam) {
				char buf[16];

				sprintf(buf, "%d", i);
				xmlSetProp(root, idparam, buf);
			}

			build_xml(root, map, i, source);
			
			xmlDocDumpMemory (docout, &xmlbuf, &n);
			xmlFreeDoc (docout);

			list = g_slist_append(list, xmlbuf);
		}

		gconf_client_set_list(gconf, outpath, GCONF_VALUE_STRING, list, NULL);
		while (list) {
			l = list->next;
			xmlFree(list->data);
			g_slist_free_1(list);
			list = l;
		}
	} else {
		g_warning("could not find '%s' in old config database, skipping", path);
	}

	return 0;
}

/* ********************************************************************** */
/*  Tables for bonobo conf -> gconf conversion				  */
/* ********************************************************************** */

/* order important here, used to index a few tables below */
enum {
	BMAP_BOOL,
	BMAP_BOOLNOT,
	BMAP_INT,
	BMAP_STRING,
	BMAP_SIMPLESTRING,	/* a non-encoded string */
	BMAP_COLOUR,
	BMAP_FLOAT,		/* bloody floats, who uses floats ... idiots */
	BMAP_STRLIST,		/* strings separated to !<-->! -> gconf list */
	BMAP_ANYLIST,		/* corba sequence corba string -> gconf list */
	BMAP_MASK = 0x7f,
	BMAP_LIST = 0x80	/* from includes a %i field for the index of the key, to be converted to a list of type BMAP_* */
};

struct _gconf_map {
	char *from;
	char *to;
	int type;
};

/* ********************************************************************** */

static struct _gconf_map mail_accounts_map[] = {
	/* /Mail/Accounts - most entries are processed via the xml blob routine */
	/* This also works because the initial uid mapping is 1:1 with the list order */
	{ "default_account", "mail/default_account", BMAP_SIMPLESTRING },
	{ 0 },
};

static struct _gconf_map mail_display_map[] = {
	/* /Mail/Display */
	{ "thread_list", "mail/display/thread_list", BMAP_BOOL },
	{ "thread_subject", "mail/display/thread_subject", BMAP_BOOL },
	{ "hide_deleted", "mail/display/show_deleted", BMAP_BOOLNOT },
	{ "preview_pane", "mail/display/show_preview", BMAP_BOOL },
	{ "paned_size", "mail/display/paned_size", BMAP_INT },
	{ "seen_timeout", "mail/display/mark_seen_timeout", BMAP_INT },
	{ "do_seen_timeout", "mail/display/mark_seen", BMAP_BOOL },
	{ "http_images", "mail/display/load_http_images", BMAP_INT },
	{ "citation_highlight", "mail/display/mark_citations", BMAP_BOOL },
	{ "citation_color", "mail/display/citation_colour", BMAP_COLOUR },
	{ "x_mailer_display_style", "mail/display/xmailer_mask", BMAP_INT },
	{ 0 },
};

static struct _gconf_map mail_format_map[] = {
	/* /Mail/Format */
	{ "message_display_style", "mail/display/message_style", BMAP_INT },
	{ "send_html", "mail/composer/send_html", BMAP_BOOL },
	{ "default_reply_style", "mail/format/reply_style", BMAP_INT },
	{ "default_forward_style", "mail/format/forward_style", BMAP_INT },
	{ "default_charset", "mail/composer/charset", BMAP_STRING },
	{ "confirm_unwanted_html", "mail/prompts/unwanted_html", BMAP_BOOL },
	{ 0 },
};

static struct _gconf_map mail_trash_map[] = {
	/* /Mail/Trash */
	{ "empty_on_exit", "mail/trash/empty_on_exit", BMAP_BOOL },
	{ 0 },
};

static struct _gconf_map mail_prompts_map[] = {
	/* /Mail/Prompts */
	{ "confirm_expunge", "mail/prompts/expunge", BMAP_BOOL },
	{ "empty_subject", "mail/prompts/empty_subject", BMAP_BOOL },
	{ "only_bcc", "mail/prompts/only_bcc", BMAP_BOOL },
	{ 0 }
};

static struct _gconf_map mail_filters_map[] = {
	/* /Mail/Filters */
	{ "log", "mail/filters/log", BMAP_BOOL },
	{ "log_path", "mail/filters/logfile", BMAP_STRING },
	{ 0 }
};

static struct _gconf_map mail_notify_map[] = {
	/* /Mail/Notify */
	{ "new_mail_notification", "mail/notify/type", BMAP_INT },
	{ "new_mail_notification_sound_file", "mail/notify/sound", BMAP_STRING },
	{ 0 }
};

static struct _gconf_map mail_filesel_map[] = {
	/* /Mail/Filesel */
	{ "last_filesel_dir", "mail/save_dir", BMAP_STRING },
	{ 0 }
};

static struct _gconf_map mail_composer_map[] = {
	/* /Mail/Composer */
	{ "ViewFrom", "mail/composer/view/From", BMAP_BOOL },
	{ "ViewReplyTo", "mail/composer/view/ReplyTo", BMAP_BOOL },
	{ "ViewCC", "mail/composer/view/Cc", BMAP_BOOL },
	{ "ViewBCC", "mail/composer/view/Bcc", BMAP_BOOL },
	{ "ViewSubject", "mail/composer/view/Subject", BMAP_BOOL },
	{ 0 },
};

/* ********************************************************************** */

static struct _gconf_map importer_elm_map[] = {
	/* /Importer/Elm */
	{ "mail", "importer/elm/mail", BMAP_BOOL },
	{ "mail-imported", "importer/elm/mail-imported", BMAP_BOOL },
	{ 0 },
};

static struct _gconf_map importer_pine_map[] = {
	/* /Importer/Pine */
	{ "mail", "importer/elm/mail", BMAP_BOOL },
	{ "address", "importer/elm/address", BMAP_BOOL },
	{ 0 },
};

static struct _gconf_map importer_netscape_map[] = {
	/* /Importer/Netscape */
	{ "mail", "importer/netscape/mail", BMAP_BOOL },
	{ "settings", "importer/netscape/settings", BMAP_BOOL },
	{ "filters", "importer/netscape/filters", BMAP_BOOL },
	{ 0 },
};

/* ********************************************************************** */

static struct _gconf_map myev_mail_map[] = {
	/* /My-Evolution/Mail */
	{ "show_full_path", "summary/mail/show_full_paths", BMAP_BOOL },
	{ 0 },
};

static struct _gconf_map myev_rdf_map[] = {
	/* /My-Evolution/RDF */
	{ "rdf_urls", "summary/rdf/uris", BMAP_STRLIST },
	{ "rdf_refresh_time", "summary/rdf/refresh_time", BMAP_INT },
	{ "limit", "summary/rdf/max_items", BMAP_INT },
	{ 0 },
};

static struct _gconf_map myev_weather_map[] = {
	/* /My-Evolution/Weather */
	{ "stations", "summary/weather/stations", BMAP_STRLIST },
	{ "units", "summary/weather/use_metric", BMAP_BOOL }, /* this is use_metric bool in 1.3? */
	{ "weather_refresh_time", "summary/weather/refresh_time", BMAP_INT },
	{ 0 },
};

static struct _gconf_map myev_schedule_map[] = {
	/* /My-Evolution/Shedule */
	{ "show_tasks", "summary/tasks/show_all", BMAP_BOOL }, /* this is show_all bool in 1.3? */
	{ 0 },
};

/* ********************************************************************** */

/* This grabs the defaults from the first view ... (?) */
static struct _gconf_map shell_views_map[] = {
	/* /Shell/Views/0 */
	{ "Width", "shell/view_defaults/width", BMAP_INT },
	{ "Height", "shell/view_defaults/height", BMAP_INT },
	{ "CurrentShortcutsGroupNum", "shell/view_defaults/selected_shortcut_group", BMAP_INT },
	{ "FolderBarShown", "shell/view_defaults/show_folder_bar", BMAP_BOOL },
	{ "ShortcutBarShown", "shell/view_defaults/show_shortcut_bar", BMAP_BOOL },
	{ "HPanedPosition", "shell/view_defaults/shortcut_bar/width", BMAP_INT },
	{ "ViewPanedPosition", "shell/view_defaults/folder_bar/width", BMAP_INT },
	{ "DisplayedURI", "shell/view_defaults/folder_path", BMAP_STRING },
	{ 0 },
};

static struct _gconf_map offlinefolders_map[] = {
	/* /OfflineFolders */
	{ "paths", "shell/offline/folder_paths", BMAP_ANYLIST },
	{ 0 },
};

static struct _gconf_map defaultfolders_map[] = {
	/* /DefaultFolders */
	{ "mail_path", "shell/default_folders/mail_path", BMAP_STRING },
	{ "mail_uri", "shell/default_folders/mail_uri", BMAP_STRING },
	{ "contacts_path", "shell/default_folders/contacts_path", BMAP_STRING },
	{ "contacts_uri", "shell/default_folders/contacts_uri", BMAP_STRING },
	{ "calendar_path", "shell/default_folders/calendar_path", BMAP_STRING },
	{ "calendar_uri", "shell/default_folders/calendar_uri", BMAP_STRING },
	{ "tasks_path", "shell/default_folders/tasks_path", BMAP_STRING },
	{ "tasks_uri", "shell/default_folders/tasks_uri", BMAP_STRING },
	{ 0 },
};

static struct _gconf_map shell_map[] = {
	/* /Shell */
	{ "StartOffline", "shell/start_offline", BMAP_BOOL },
	{ 0 },
};

/* ********************************************************************** */

static struct _gconf_map addressbook_map[] = {
	/* /Addressbook */
	{ "select_names_uri", "addressbook/select_names/last_used_uri", BMAP_STRING },
	{ 0 },
};

static struct _gconf_map addressbook_completion_map[] = {
	/* /Addressbook/Completion */
	{ "uris", "addressbook/completion/uris", BMAP_STRING },
	{ 0 },
};

/* ********************************************************************** */

static struct _gconf_map calendar_display_map[] = {
	/* /Calendar/Display */
	{ "Timezone", "calendar/display/timezone", BMAP_STRING },
	{ "Use24HourFormat", "calendar/display/use_24hour_format", BMAP_BOOL },
	{ "WeekStartDay", "calendar/display/week_start_day", BMAP_INT },
	{ "DayStartHour", "calendar/display/day_start_hour", BMAP_INT },
	{ "DayStartMinute", "calendar/display/day_start_minute", BMAP_INT },
	{ "DayEndHour", "calendar/display/day_end_hour", BMAP_INT },
	{ "DayEndMinute", "calendar/display/day_end_minute", BMAP_INT },
	{ "TimeDivisions", "calendar/display/time_divisions", BMAP_INT },
	{ "View", "calendar/display/default_view", BMAP_INT },
	{ "HPanePosition", "calendar/display/hpane_position", BMAP_FLOAT },
	{ "VPanePosition", "calendar/display/vpane_position", BMAP_FLOAT },
	{ "MonthHPanePosition", "calendar/display/month_hpane_position", BMAP_FLOAT },
	{ "MonthVPanePosition", "calendar/display/month_vpane_position", BMAP_FLOAT },
	{ "CompressWeekend", "calendar/display/compress_weekend", BMAP_BOOL },
	{ "ShowEventEndTime", "calendar/display/show_event_end", BMAP_BOOL },
	{ "WorkingDays", "calendar/display/working_days", BMAP_INT },
	{ 0 },
};

static struct _gconf_map calendar_tasks_map[] = {
	/* /Calendar/Tasks */
	{ "HideCompletedTasks", "calendar/tasks/hide_completed", BMAP_BOOL },
	{ "HideCompletedTasksUnits", "calendar/tasks/hide_completed_units", BMAP_STRING },
	{ "HideCompletedTasksValue", "calendar/tasks/hide_completed_value", BMAP_INT },
	{ 0 },
};

static struct _gconf_map calendar_tasks_colours_map[] = {
	/* /Calendar/Tasks/Colors */
	{ "TasksDueToday", "calendar/tasks/colors/due_today", BMAP_STRING },
	{ "TasksOverDue", "calendar/tasks/colors/overdue", BMAP_STRING },
	{ "TasksDueToday", "calendar/tasks/colors/due_today", BMAP_STRING },
	{ 0 },
};

static struct _gconf_map calendar_other_map[] = {
	/* /Calendar/Other */
	{ "ConfirmDelete", "calendar/prompts/confirm_delete", BMAP_BOOL },
	{ "ConfirmExpunge", "calendar/prompts/confirm_expunge", BMAP_BOOL },
	{ "UseDefaultReminder", "calendar/other/use_default_reminder", BMAP_BOOL },
	{ "DefaultReminderInterval", "calendar/other/default_reminder_interval", BMAP_INT },
	{ "DefaultReminderUnits", "calendar/other/default_reminder_units", BMAP_STRING },
	{ 0 },
};

static struct _gconf_map calendar_datenavigator_map[] = {
	/* /Calendar/DateNavigator */
	{ "ShowWeekNumbers", "calendar/date_navigator/show_week_numbers", BMAP_BOOL },
	{ 0 },
};

static struct _gconf_map calendar_alarmnotify_map[] = {
	/* /Calendar/AlarmNotify */
	{ "LastNotificationTime", "calendar/notify/last_notification_time", BMAP_INT },
	{ "CalendarToLoad%i", "calendar/notify/calendars", BMAP_STRING|BMAP_LIST },
	{ "BlessedProgram%i", "calendar/notify/programs", BMAP_STRING|BMAP_LIST },
	{ 0 },
};

/* ********************************************************************** */

static struct {
	char *root;
	struct _gconf_map *map;
} gconf_remap_list[] = {
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

	{ "My-Evolution/Mail", myev_mail_map },
	{ "My-Evolution/RDF", myev_rdf_map },
	{ "My-Evolution/Weather", myev_weather_map },
	{ "My-Evolution/Schedule", myev_schedule_map },

	{ "/Shell", shell_map },
	{ "/Shell/Views/0", shell_views_map },
	{ "/OfflineFolders", offlinefolders_map },
	{ "/DefaultFolders", defaultfolders_map },

	{ "/Addressbook", addressbook_map },
	{ "/Addressbook/Completion", addressbook_completion_map },

	{ "/Calendar/Display", calendar_display_map },
	{ "/Calendar/Tasks", calendar_tasks_map },
	{ "/Calendar/Tasks/Colors", calendar_tasks_colours_map },
	{ "/Calendar/Other/Map", calendar_other_map },
	{ "/Calendar/DateNavigator", calendar_datenavigator_map },
	{ "/Calendar/AlarmNotify", calendar_alarmnotify_map },

	{ 0 },
};

#define N_(x) x

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
static int import_bonobo_config(xmlDocPtr config_doc, GConfClient *gconf)
{
	xmlNodePtr source;
	int i, j, k;
	struct _gconf_map *map;
	char *path, *val, *tmp;
	GSList *list, *l;
	char buf[32];

	/* process all flat config */
	for (i=0;gconf_remap_list[i].root;i++) {
		d(printf("Path: %s\n", gconf_remap_list[i].root));
		source = lookup_bconf_path(config_doc, gconf_remap_list[i].root);
		if (source == NULL)
			continue;
		map = gconf_remap_list[i].map;
		for (j=0;map[j].from;j++) {
			if (map[j].type & BMAP_LIST) {
				/* collapse a multi-entry indexed field into a list */
				list = NULL;
				k = 0;
				do {
					path = get_name(map[j].from, k);
					val = lookup_bconf_value(source, path);
					d(printf("finding path '%s' = '%s'\n", path, val));
					g_free(path);
					if (val) {
						switch(map[j].type & BMAP_MASK) {
						case BMAP_BOOL:
						case BMAP_INT:
							list = g_slist_append(list, GINT_TO_POINTER(atoi(val)));
							break;
						case BMAP_STRING:
							d(printf(" -> '%s'\n", hex_decode(val)));
							list = g_slist_append(list, hex_decode(val));
							break;
						}
						xmlFree(val);
						k++;
					}
				} while (val);

				if (list) {
					const int gconf_type[] = { GCONF_VALUE_BOOL, GCONF_VALUE_BOOL, GCONF_VALUE_INT, GCONF_VALUE_STRING, GCONF_VALUE_STRING };

					path = g_strdup_printf("/apps/evolution/%s", map[j].to);
					gconf_client_set_list(gconf, path, gconf_type[map[j].type & BMAP_MASK], list, NULL);
					g_free(path);
					if ((map[j].type & BMAP_MASK) == BMAP_STRING)
						g_slist_foreach(list, (GFunc)g_free, NULL);
					g_slist_free(list);
				}

				continue;
			} else if (map[j].type == BMAP_ANYLIST)
				val = NULL;
			else {
				val = lookup_bconf_value(source, map[j].from);
				if (val == NULL)
					continue;
			}
			d(printf(" %s = '%s' -> %s [%d]\n",
				 map[j].from,
				 val == NULL ? "(null)" : val,
				 map[j].to,
				 map[j].type));
			path = g_strdup_printf("/apps/evolution/%s", map[j].to);
			switch(map[j].type) {
			case BMAP_BOOL:
				gconf_client_set_bool(gconf, path, atoi(val), NULL);
				break;
			case BMAP_BOOLNOT:
				gconf_client_set_bool(gconf, path, !atoi(val), NULL);
				break;
			case BMAP_INT:
				gconf_client_set_int(gconf, path, atoi(val), NULL);
				break;
			case BMAP_STRING:
				tmp = hex_decode(val);
				gconf_client_set_string(gconf, path, tmp, NULL);
				g_free(tmp);
				break;
			case BMAP_SIMPLESTRING:
				gconf_client_set_string(gconf, path, val, NULL);
				break;
			case BMAP_FLOAT:
				gconf_client_set_float(gconf, path, strtod(val, NULL), NULL);
				break;
			case BMAP_STRLIST:{
				char *v = hex_decode(val);
				char **t = g_strsplit (v, " !<-->! ", 8196);

				list = NULL;
				for (k=0;t[k];k++) {
					list = g_slist_append(list, t[k]);
					d(printf("  [%d] = '%s'\n", k, t[k]));
				}
				gconf_client_set_list(gconf, path, GCONF_VALUE_STRING, list, NULL);
				g_slist_free(list);
				g_strfreev(t);
				g_free(v);
				break;}
			case BMAP_ANYLIST:{
				xmlNodePtr node = source->children;
				list = NULL;

				/* find the entry node */
				while (node) {
					if (!strcmp(node->name, "entry")) {
						int found;

						tmp = xmlGetProp(node, "name");
						if (tmp) {
							found = strcmp(tmp, map[j].from) == 0;
							xmlFree(tmp);
							if (found)
								break;
						}
					}
					node = node->next;
				}

				/* find the the any block */
				if (node) {
					node = node->children;
					while (node) {
						if (strcmp(node->name, "any") == 0)
							break;
						node = node->next;
					}
				}

				/* skip to the value inside it */
				if (node) {
					node = node->children;
					while (node) {
						if (strcmp(node->name, "value") == 0)
							break;
						node = node->next;
					}
				}

				if (node) {
					node = node->children;
					while (node) {
						if (strcmp(node->name, "value") == 0)
							list = g_slist_append(list, xmlNodeGetContent(node));
						node = node->next;
					}
				}

				/* & store */
				if (list) {
					gconf_client_set_list(gconf, path, GCONF_VALUE_STRING, list, NULL);
					while (list) {
						l = list->next;
						xmlFree(list->data);
						g_slist_free_1(list);
						list = l;
					}
				}
				
				break;}
			case BMAP_COLOUR:
				sprintf(buf, "#%06x", atoi(val) & 0xffffff);
				gconf_client_set_string(gconf, path, buf, NULL);
				break;
			}
			/* FIXME: handle errors */
			g_free(path);
			xmlFree(val);
		}
	}

	/* Labels:
	  label string + label colour as integer
	   -> label string:# colour as hex */
	source = lookup_bconf_path(config_doc, "/Mail/Labels");
	if (source) {
		list = NULL;
		for (i=0;i<5;i++) {
			char labx[16], colx[16];
			char *lab, *col;

			sprintf(labx, "label_%d", i);
			sprintf(colx, "color_%d", i);
			lab = lookup_string(source, labx, NULL);
			col = lookup_bconf_value(source, colx);
			if (col) {
				sprintf(colx, "#%06x", atoi(col) & 0xffffff);
				xmlFree(col);
			} else
				strcpy(colx, label_default[i].colour);
			val = g_strdup_printf("%s:%s", lab?lab:label_default[i].label, colx);
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
	convert_xml_blob(gconf, config_doc, account_map, "/Mail/Accounts", "/apps/evolution/mail/accounts", "account", "uid");
	/* Same for signatures */
	convert_xml_blob(gconf, config_doc, signature_map, "/Mail/Signatures", "/apps/evolution/mail/signatures", "signature", NULL);

	/* My-Evolution folder lists */
	source = lookup_bconf_path(config_doc, "My-Evolution/Mail");
	if (source) {
		char **t;

		list = NULL;
		l = NULL;

		val = lookup_string(source, "display_folders-1.2", NULL);
		/* FIXME: what needs upgrading here?  Anything? */
		if (val == NULL)
			val = lookup_string(source, "display_folders", NULL);

		if (val) {
			t = g_strsplit (val, " !<-->! ", 8196);
			for (i=0;t[i] && t[i+1];i+=2) {
				list = g_slist_append(list, t[i]);
				l = g_slist_append(l, t[i+1]);
				d(printf(" [%d] = euri: '%s', puri: '%s'\n", i, t[i], t[i+1]));
			}
			if (list) {
				gconf_client_set_list(gconf, "/apps/evolution/summary/mail/folder_evolution_uris", GCONF_VALUE_STRING, list, NULL);
				g_slist_free(list);
			}
			if (l) {
				gconf_client_set_list(gconf, "/apps/evolution/summary/mail/folder_physical_uris", GCONF_VALUE_STRING, l, NULL);
				g_slist_free(l);
			}
			g_strfreev(t);
		}
	}

	return 0;
}


static int load_accounts_1_0(xmlDocPtr doc)
{
	xmlNodePtr source;
	char *val, *tmp;
	int count = 0, i;
	char key[32];

	source = lookup_bconf_path(doc, "/Mail/Accounts");
	if (source == NULL)
		return 0;

	val = lookup_bconf_value(source, "num");
	if (val) {
		count = atoi(val);
		xmlFree(val);
	}

	/* load account upgrade info for each account */
	for (i=0;i<count;i++) {
		struct _account_info *ai;
		char *rawuri;

		sprintf(key, "source_url_%d", i);
		rawuri = lookup_bconf_value(source, key);
		if (rawuri == NULL)
			continue;
		ai = g_malloc0(sizeof(*ai));
		ai->uri = hex_decode(rawuri);
		ai->base_uri = get_base_uri(ai->uri);
		sprintf(key, "account_name_%d", i);
		ai->name = lookup_string(source, key, NULL);

		d(printf("load account '%s'\n", ai->uri));

		if (!strncmp(ai->uri, "imap:", 5)) {
			read_imap_storeinfo(ai);
		} else if (!strncmp(ai->uri, "exchange:", 9)) {
			xmlNodePtr node;

			d(printf(" upgrade exchange account\n"));
			/* small hack, poke the source_url into the transport_url for exchanget: transports
			   - this will be picked up later in the conversion */
			sprintf(key, "transport_url_%d", i);
			node = lookup_bconf_entry(source, key);
			if (node
			    && (val = xmlGetProp(node, "value"))) {
				tmp = hex_decode(val);
				xmlFree(val);
				if (strncmp(tmp, "exchanget:", 10) == 0)
					xmlSetProp(node, "value", rawuri);
				g_free(tmp);
			} else {
				d(printf(" couldn't find transport uri?\n"));
			}
		}
		xmlFree(rawuri);

		g_hash_table_insert(accounts_1_0, ai->base_uri, ai);
		if (ai->name)
			g_hash_table_insert(accounts_name_1_0, ai->name, ai);
	}

	return 0;
}

/**
 * e_config_upgrade:
 * @edir: 
 * 
 * Upgrade evolution configuration from prior versions of evolution to
 * the current one.
 *
 * No work is performed if the configuration version is up to date.
 *
 * The tracked version is upgraded to the latest even if no
 * configuration upgrades are required for that version.
 *
 * Further information about how this is intended to work:
 *
 * There are 3 basic steps, numbered in the comments below.
 * 1. Determine the current config verison
 * 2. Upgrade to the current source version
 * 3. Save the version number, as defined by CONF_MAJOR, CONF_MINOR,
 *    CONF_REVISION.  These are all treated as integers, so 10 is
 *    greater than 9.
 *
 * 1 and 3 should not need changing.  After an upgrade to 1.3.x and
 * until the config system changes again (!), step one becomes
 * trivial.  Any changes to part 2 should be added to the end of the
 * section, or as required.  This allows for very fine-grained version
 * upgrades, including pre-release and patch-level changes to fix
 * config problems which may have lasted for a single version or
 * patch, in which case CONF_REVISION can be bumped.
 * 
 * At any time, the CONF_VERSION/MAJOR/REVISION can be increased to
 * match the source release, even if no new configuration changes will
 * be required from the previous version.  This should be done at each
 * release in case bugs in that configuration version are required to
 * be fixed at any time in the future.
 *
 * Return value: -1 on an error.
 **/
int
e_config_upgrade(const char *edir)
{
	xmlNodePtr source;
	xmlDocPtr config_doc = NULL;
	int i;
	char *val, *tmp;
	GConfClient *gconf;
	int res = -1;
	struct stat st;

	evolution_dir = edir;

	/* 1. determine existing version */
	gconf = gconf_client_get_default();
	val = gconf_client_get_string(gconf, "/apps/evolution/version", NULL);
	if (val) {
		sscanf(val, "%u.%u.%u", &major, &minor, &revision);
		g_free(val);
	} else {
		char *filename = g_build_filename(evolution_dir, "config.xmldb", NULL);

		if (lstat(filename, &st) == 0
		    && S_ISREG(st.st_mode))
			config_doc = xmlParseFile (filename);
		g_free(filename);

		tmp = NULL;
		if ( config_doc
		     && (source = lookup_bconf_path(config_doc, "/Shell"))
		     && (tmp = lookup_bconf_value(source, "upgrade_from_1_0_to_1_2_performed"))
		     && tmp[0] == '1' ) {
			major = 1;
			minor = 2;
			revision = 0;
		} else {
			major = 1;
			minor = 0;
			revision = 0;
		}
		if (tmp)
			xmlFree(tmp);
	}

	/* 2. Now perform any upgrade duties */

	d(printf("current config version is '%u.%u.%u'\n", major, minor, revision));

	/* For 1.0.x we need to load the accounts first, as data it initialises is used elsewhere */
	if (major <=1 && minor < 2) {
		char *xml_files[] = { "vfolders.xml", "filters.xml", "shortcuts.xml" };

		/* load in 1.0 info */
		if (config_doc) {
			accounts_1_0 = g_hash_table_new(g_str_hash, g_str_equal);
			accounts_name_1_0 = g_hash_table_new(g_str_hash, g_str_equal);
			load_accounts_1_0(config_doc);
		}

		/* upgrade xml uri fields */
		for (i=0;i<sizeof(xml_files)/sizeof(xml_files[0]);i++) {
			char *path;

			path = g_build_filename(evolution_dir, xml_files[i], NULL);
			if (upgrade_xml_file(path, upgrade_xml_1_0_rec) == -1)
				g_warning("Could not upgrade xml file %s", xml_files[i]);

			g_free(path);
		}
	}

	if (config_doc && major <=1 && minor < 3) {
		/* move bonobo config to gconf */
		if (import_bonobo_config(config_doc, gconf) == -1) {
			g_warning("Could not move config from bonobo-conf to gconf");
			goto error;
		}
	}

	if (major <=1 && minor <=3 && revision < 1) {
		/* check for xml 1 encoding from version 1.2 upgrade or from a previous previous 1.3.0 upgrade */
		char *xml_files[] = { "vfolders.xml", "filters.xml" };

		d(printf("Checking for xml1 format xml files\n"));

		for (i=0;i<sizeof(xml_files)/sizeof(xml_files[0]);i++) {
			char *path;

			path = g_build_filename(evolution_dir, xml_files[i], NULL);
			if (upgrade_xml_file(path, upgrade_xml_1_2_rec) == -1)
				g_warning("Could not upgrade xml file %s", xml_files[i]);

			g_free(path);
		}
	}

	/* 3. we're done, update our version info if its changed */
	if (major < CONF_MAJOR
	    || minor < CONF_MINOR
	    || revision < CONF_REVISION) {
		val = g_strdup_printf("%u.%u.%u", CONF_MAJOR, CONF_MINOR, CONF_REVISION);
		gconf_client_set_string(gconf, "/apps/evolution/version", val, NULL);
		/* TODO: should this be translatable? */
		g_message("Evolution configuration upgraded to version: %s", val);
		g_free(val);
		gconf_client_suggest_sync(gconf, NULL);
	}

	res = 0;

error:
	if (config_doc)
		xmlFreeDoc(config_doc);

	return res;
}
