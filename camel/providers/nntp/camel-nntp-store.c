/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-nntp-store.c : class for an nntp store */

/* 
 *
 * Copyright (C) 2000 Helix Code, Inc. <toshok@helixcode.com>
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <config.h>

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libgnome/libgnome.h"

#include "camel-nntp-resp-codes.h"
#include "camel-folder-summary.h"
#include "camel-nntp-store.h"
#include "camel-nntp-grouplist.h"
#include "camel-nntp-folder.h"
#include "camel-nntp-auth.h"
#include "camel-exception.h"
#include "camel-url.h"
#include "string-utils.h"

#include <gal/util/e-util.h>

#define NNTP_PORT 119

#define DUMP_EXTENSIONS

/* define if you want the subscribe ui to show folders in tree form */
/* #define INFO_AS_TREE */

static CamelRemoteStoreClass *remote_store_class = NULL;

static CamelServiceClass *service_class = NULL;

/* Returns the class for a CamelNNTPStore */
#define CNNTPS_CLASS(so) CAMEL_NNTP_STORE_CLASS (CAMEL_OBJECT_GET_CLASS(so))
#define CF_CLASS(so) CAMEL_FOLDER_CLASS (CAMEL_OBJECT_GET_CLASS(so))
#define CNNTPF_CLASS(so) CAMEL_NNTP_FOLDER_CLASS (CAMEL_OBJECT_GET_CLASS(so))

static gboolean ensure_news_dir_exists (CamelNNTPStore *store);

static void
camel_nntp_store_get_extensions (CamelNNTPStore *store, CamelException *ex)
{
	store->extensions = 0;

	if (camel_nntp_command (store, ex, NULL, "LIST EXTENSIONS") == NNTP_LIST_FOLLOWS) {
		gboolean done = FALSE;
		CamelException ex;

		camel_exception_init (&ex);

		while (!done) {
			char *line;

			if (camel_remote_store_recv_line (CAMEL_REMOTE_STORE (store), &line, &ex) < 0)
				break; /* XXX */

			if (*line == '.') {
				done = TRUE;
			}
			else {
#define CHECK_EXT(name,val) if (!strcasecmp (line, (name))) store->extensions |= (val)

				CHECK_EXT ("SEARCH",     CAMEL_NNTP_EXT_SEARCH);
				CHECK_EXT ("SETGET",     CAMEL_NNTP_EXT_SETGET);
				CHECK_EXT ("OVER",       CAMEL_NNTP_EXT_OVER);
				CHECK_EXT ("XPATTEXT",   CAMEL_NNTP_EXT_XPATTEXT);
				CHECK_EXT ("XACTIVE",    CAMEL_NNTP_EXT_XACTIVE);
				CHECK_EXT ("LISTMOTD",   CAMEL_NNTP_EXT_LISTMOTD);
				CHECK_EXT ("LISTSUBSCR", CAMEL_NNTP_EXT_LISTSUBSCR);
				CHECK_EXT ("LISTPNAMES", CAMEL_NNTP_EXT_LISTPNAMES);

#undef CHECK_EXT
			}

			g_free (line);
		}
	}

#ifdef DUMP_EXTENSIONS
	g_print ("NNTP Extensions:");
#define DUMP_EXT(name,val) if (store->extensions & (val)) g_print (" %s", name);
	DUMP_EXT ("SEARCH",     CAMEL_NNTP_EXT_SEARCH);
	DUMP_EXT ("SETGET",     CAMEL_NNTP_EXT_SETGET);
	DUMP_EXT ("OVER",       CAMEL_NNTP_EXT_OVER);
	DUMP_EXT ("XPATTEXT",   CAMEL_NNTP_EXT_XPATTEXT);
	DUMP_EXT ("XACTIVE",    CAMEL_NNTP_EXT_XACTIVE);
	DUMP_EXT ("LISTMOTD",   CAMEL_NNTP_EXT_LISTMOTD);
	DUMP_EXT ("LISTSUBSCR", CAMEL_NNTP_EXT_LISTSUBSCR);
	DUMP_EXT ("LISTPNAMES", CAMEL_NNTP_EXT_LISTPNAMES);
	g_print ("\n");
#undef DUMP_EXT
#endif
}

static void
camel_nntp_store_get_overview_fmt (CamelNNTPStore *store, CamelException *ex)
{
	int status;
	int i;
	gboolean done = FALSE;
	
	status = camel_nntp_command (store, ex, NULL,
				     "LIST OVERVIEW.FMT");

	if (status != NNTP_LIST_FOLLOWS) {
		if (store->extensions & CAMEL_NNTP_EXT_OVER) {
			/* if we can't get the overview format, we should
			   disable OVER support */
			g_warning ("server reported support of OVER but LIST OVERVIEW.FMT failed."
				   "  disabling OVER.\n");
			store->extensions &= ~CAMEL_NNTP_EXT_OVER;
			return;
		}
	}
	else {
		if (!(store->extensions & CAMEL_NNTP_EXT_OVER)) {
			g_warning ("server didn't report support of OVER but LIST OVERVIEW.FMT worked."
				   "  enabling OVER.\n");
			store->extensions |= CAMEL_NNTP_EXT_OVER;
		}
	}

	/* start at 1 because the article number is always first */
	store->num_overview_fields = 1;
	
	for (i = 0; i < CAMEL_NNTP_OVER_LAST; i ++) {
		store->overview_field [i].index = -1;
	}

	while (!done) {
		char *line;

		if (camel_remote_store_recv_line (CAMEL_REMOTE_STORE (store), &line, ex) < 0)
			break; /* XXX */

		if (*line == '.') {
			done = TRUE;
		}
		else {
			CamelNNTPOverField *over_field = NULL;
			char *colon = NULL;;

			if (!strncasecmp (line, "From:", 5)) {
				over_field = &store->overview_field [ CAMEL_NNTP_OVER_FROM ];
				over_field->index = store->num_overview_fields;
				colon = line + 5;
			}
			else if (!strncasecmp (line, "Subject:", 7)) {
				over_field = &store->overview_field [ CAMEL_NNTP_OVER_SUBJECT ];
				over_field->index = store->num_overview_fields;
				colon = line + 7;
			}
			else if (!strncasecmp (line, "Date:", 5)) {
				over_field = &store->overview_field [ CAMEL_NNTP_OVER_DATE ];
				over_field->index = store->num_overview_fields;
				colon = line + 5;
			}
			else if (!strncasecmp (line, "Message-ID:", 11)) {
				over_field = &store->overview_field [ CAMEL_NNTP_OVER_MESSAGE_ID ];
				over_field->index = store->num_overview_fields;
				colon = line + 11;
			}
			else if (!strncasecmp (line, "References:", 11)) {
				over_field = &store->overview_field [ CAMEL_NNTP_OVER_REFERENCES ];
				over_field->index = store->num_overview_fields;
				colon = line + 11;
			}
			else if (!strncasecmp (line, "Bytes:", 6)) {
				over_field = &store->overview_field [ CAMEL_NNTP_OVER_BYTES ];
				over_field->index = store->num_overview_fields;
				colon = line + 11;
			}
		
			if (colon && !strncmp (colon + 1, "full", 4))
				over_field->full = TRUE;

			store->num_overview_fields ++;
		}

		g_free (line);
	}

	for (i = 0; i < CAMEL_NNTP_OVER_LAST; i ++) {
		if (store->overview_field [i].index == -1) {
			g_warning ("server's OVERVIEW.FMT doesn't support minimum set we require,"
				   " disabling OVER support.\n");
			store->extensions &= ~CAMEL_NNTP_EXT_OVER;
		}
	}
}

static gboolean
nntp_store_connect (CamelService *service, CamelException *ex)
{
	char *buf;
	int resp_code;
	CamelNNTPStore *store = CAMEL_NNTP_STORE (service);

	if (!ensure_news_dir_exists(store)) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not open directory for news server: %s"),
				      strerror (errno));
		return FALSE;
	}

	if (CAMEL_SERVICE_CLASS (remote_store_class)->connect (service, ex) == FALSE)
		return FALSE;

	/* Read the greeting */
	if (camel_remote_store_recv_line (CAMEL_REMOTE_STORE (service), &buf, ex) < 0) {
		return FALSE;
	}

	/* check if posting is allowed. */
	resp_code = atoi (buf);
	if (resp_code == NNTP_GREETING_POSTING_OK) {
		g_print ("posting allowed\n");
		store->posting_allowed = TRUE;
	}
	else if (resp_code == NNTP_GREETING_NO_POSTING) {
		g_print ("no posting allowed\n");
		store->posting_allowed = FALSE;
	}
	else {
		g_warning ("unexpected server greeting code %d, no posting allowed\n", resp_code);
		store->posting_allowed = FALSE;
	}

	g_free (buf);

	/* get a list of extensions that the server supports */
	camel_nntp_store_get_extensions (store, ex);

	/* try to get the overview.fmt */
	camel_nntp_store_get_overview_fmt (store, ex);

	return TRUE;
}

static gboolean
nntp_store_disconnect (CamelService *service, gboolean clean, CamelException *ex)
{
	CamelNNTPStore *store = CAMEL_NNTP_STORE (service);

	if (clean)
		camel_nntp_command (store, ex, NULL, "QUIT");

	if (store->newsrc)
		camel_nntp_newsrc_write (store->newsrc);

	if (!service_class->disconnect (service, clean, ex))
		return FALSE;

	return TRUE;
}

static char *
nntp_store_get_name (CamelService *service, gboolean brief)
{
	if (brief)
		return g_strdup_printf ("%s", service->url->host);
	else
		return g_strdup_printf (_("USENET News via %s"), service->url->host);
	
}

static CamelServiceAuthType password_authtype = {
	N_("Password"),
	
	N_("This option will authenticate with the NNTP server using a "
	   "plaintext password."),
	
	"",
	TRUE
};

static GList *
nntp_store_query_auth_types (CamelService *service, CamelException *ex)
{
	GList *prev;
	
	g_warning ("nntp::query_auth_types: not implemented. Defaulting.");
	prev = CAMEL_SERVICE_CLASS (remote_store_class)->query_auth_types (service, ex);
	return g_list_prepend (prev, &password_authtype);
}

static CamelFolder *
nntp_store_get_folder (CamelStore *store, const gchar *folder_name,
		       guint32 flags, CamelException *ex)
{
	CamelNNTPStore *nntp_store = CAMEL_NNTP_STORE (store);

	printf ("get_folder called on folder_name=%s\n", folder_name);

	/* if we haven't already read our .newsrc, read it now */
	if (!nntp_store->newsrc)
		nntp_store->newsrc = 
		camel_nntp_newsrc_read_for_server (CAMEL_SERVICE(store)->url->host);

	if (!nntp_store->newsrc) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      _("Unable to open or create .newsrc file for %s: %s"),
				      CAMEL_SERVICE(store)->url->host,
				      strerror(errno));
		return NULL;
	}

	return camel_nntp_folder_new (store, folder_name, ex);
}

#ifdef INFO_AS_TREE
static void
build_folder_info (CamelNNTPStore *nntp_store, CamelFolderInfo **root,
		   CamelFolderInfo *parent, CamelNNTPGroupListEntry *entry,
		   char *prefix, char *suffix,
		   GHashTable *name_to_info)
{
	CamelURL *url = CAMEL_SERVICE (nntp_store)->url;
	char *dot;
	if ((dot = strchr (suffix, '.'))) {
		/* it's an internal node, figure out the next node in
                   the chain */
		CamelFolderInfo *node;
		char *node_name, *node_full_name;

		node_name = g_malloc0 (dot - suffix + 1);
		strncpy (node_name, suffix, dot - suffix);
		node_full_name = g_strdup_printf ("%s.%s", prefix, node_name);

		node = g_hash_table_lookup (name_to_info, node_full_name);
		if (!node) {
			/* we need to add one */
			node = g_new0 (CamelFolderInfo, 1);
			node->name = g_strdup (node_name);
			node->full_name = g_strdup (node_full_name);
			node->url = NULL;
			node->message_count = -1;
			node->unread_message_count = -1;

			if (parent) {
				if (parent->child) {
					node->sibling = parent->child;
					parent->child = node;
				}
				else {
					parent->child = node;
				}
			}
			else {
				if (*root) {
					*root = node;
				}
				else {
					node->sibling = *root;
					*root = node;
				}
			}

			g_hash_table_insert (name_to_info, node_full_name, node);
		}

		build_folder_info (nntp_store, root, node, entry, node_full_name, dot + 1, name_to_info);
	}
	else {
		/* it's a leaf node, make the CamelFolderInfo and
                   append it to @parent's list of children. */
		CamelFolderInfo *new_group;

		new_group = g_new0 (CamelFolderInfo, 1);
		new_group->name = g_strdup (entry->group_name);
		new_group->full_name = g_strdup (entry->group_name);
		new_group->url = g_strdup_printf ("nntp://%s%s%s/%s",
						  url->user ? url->user : "",
						  url->user ? "@" : "",
						  url->host, (char *)entry->group_name);

		new_group->message_count = entry->high - entry->low;
		new_group->unread_message_count = (new_group->message_count - 
						   camel_nntp_newsrc_get_num_articles_read (nntp_store->newsrc, entry->group_name));

		if (parent) {
			if (parent->child) {
				new_group->sibling = parent->child;
				parent->child = new_group;
			}
			else {
				parent->child = new_group;
			}
		}
		else {
			if (*root) {
				*root = new_group;
			}
			else {
				new_group->sibling = *root;
				*root = new_group;
			}
		}
	}
}
#endif

static CamelFolderInfo *
build_folder_info_from_grouplist (CamelNNTPStore *nntp_store, const char *top)
{
	GList *g;
	CamelFolderInfo *groups = NULL;
#ifdef INFO_AS_TREE
	GHashTable *hash = g_hash_table_new (g_str_hash, g_str_equal);
#else
	CamelFolderInfo *last = NULL, *fi;
	CamelURL *url = CAMEL_SERVICE (nntp_store)->url;
#endif

	for (g = nntp_store->group_list->group_list; g; g = g_list_next (g)) {
		CamelNNTPGroupListEntry *entry = g->data;

		if (!top || !strncmp (top, entry->group_name, strlen (top))) {
#ifdef INFO_AS_TREE
			build_folder_info (nntp_store, &groups, NULL, entry,
					   "", entry->group_name, hash);
#else

			fi = g_new0 (CamelFolderInfo, 1);
			fi->name = g_strdup (entry->group_name);
			fi->full_name = g_strdup (entry->group_name);
			fi->url = g_strdup_printf ("nntp://%s%s%s/%s",
						   url->user ? url->user : "",
						   url->user ? "@" : "",
						   url->host, (char *)entry->group_name);

			fi->message_count = entry->high - entry->low;
			fi->unread_message_count = (fi->message_count - 
						    camel_nntp_newsrc_get_num_articles_read (
							     nntp_store->newsrc, entry->group_name));

			if (last)
				last->sibling = fi;
			else
				groups = fi;
			last = fi;
#endif
		}
	}

	return groups;
}

static CamelFolderInfo *
nntp_store_get_folder_info (CamelStore *store, const char *top,
			    gboolean fast, gboolean recursive,
			    gboolean subscribed_only,
			    CamelException *ex)
{
	CamelURL *url = CAMEL_SERVICE (store)->url;
	CamelNNTPStore *nntp_store = (CamelNNTPStore *)store;
	GPtrArray *names;
	CamelFolderInfo *groups = NULL, *last = NULL, *fi;
	int i;

	/* if we haven't already read our .newsrc, read it now */
	if (!nntp_store->newsrc)
		nntp_store->newsrc = 
		camel_nntp_newsrc_read_for_server (CAMEL_SERVICE(store)->url->host);

	if (!nntp_store->newsrc) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      _("Unable to open or create .newsrc file for %s: %s"),
				      CAMEL_SERVICE(store)->url->host,
				      strerror(errno));
		return NULL;
	}

	if (!subscribed_only) {
		if (!nntp_store->group_list)
			nntp_store->group_list = camel_nntp_grouplist_fetch (nntp_store, ex);
		if (camel_exception_is_set (ex)) {
			return NULL;
		}
		else {
			fi = build_folder_info_from_grouplist (nntp_store, top);
			return fi;
		}
	}

	if (top == NULL) {
		/* return the list of groups */
		names = camel_nntp_newsrc_get_subscribed_group_names (nntp_store->newsrc);
		for (i = 0; i < names->len; i++) {
			fi = g_new0 (CamelFolderInfo, 1);
			fi->name = g_strdup (names->pdata[i]);
			fi->full_name = g_strdup (names->pdata[i]);
			fi->url = g_strdup_printf ("nntp://%s%s%s/%s",
						   url->user ? url->user : "",
						   url->user ? "@" : "",
						   url->host, (char *)names->pdata[i]);
			/* FIXME */
			fi->message_count = fi->unread_message_count = -1;

			if (last)
				last->sibling = fi;
			else
				groups = fi;
			last = fi;
		}
		camel_nntp_newsrc_free_group_names (nntp_store->newsrc, names);

		return groups;
	}
	else {
		/* getting a specific group */

		fi = g_new0 (CamelFolderInfo, 1);
		fi->name = g_strdup (top);
		fi->full_name = g_strdup (top);
		fi->url = g_strdup_printf ("nntp://%s/%s", url->host, top);
		/* FIXME */
		fi->message_count = fi->unread_message_count = -1;

		return fi;
	}
}

static char *
nntp_store_get_root_folder_name (CamelStore *store, CamelException *ex)
{
	return g_strdup ("");
}

static gboolean
nntp_store_folder_subscribed (CamelStore *store, const char *folder_name)
{
	CamelNNTPStore *nntp_store = CAMEL_NNTP_STORE (store);

	return camel_nntp_newsrc_group_is_subscribed (nntp_store->newsrc, folder_name);
}

static void
nntp_store_subscribe_folder (CamelStore *store, const char *folder_name,
			     CamelException *ex)
{
	CamelNNTPStore *nntp_store = CAMEL_NNTP_STORE (store);

	camel_nntp_newsrc_subscribe_group (nntp_store->newsrc, folder_name);
}

static void
nntp_store_unsubscribe_folder (CamelStore *store, const char *folder_name,
			       CamelException *ex)
{
	CamelNNTPStore *nntp_store = CAMEL_NNTP_STORE (store);

	camel_nntp_newsrc_unsubscribe_group (nntp_store->newsrc, folder_name);
}

static void
finalize (CamelObject *object)
{
	CamelNNTPStore *nntp_store = CAMEL_NNTP_STORE (object);
	if (nntp_store->newsrc)
		camel_nntp_newsrc_write (nntp_store->newsrc);
}

static void
camel_nntp_store_class_init (CamelNNTPStoreClass *camel_nntp_store_class)
{
	CamelStoreClass *camel_store_class = CAMEL_STORE_CLASS (camel_nntp_store_class);
	CamelServiceClass *camel_service_class = CAMEL_SERVICE_CLASS (camel_nntp_store_class);

	remote_store_class = CAMEL_REMOTE_STORE_CLASS(camel_type_get_global_classfuncs 
						      (camel_remote_store_get_type ()));

	service_class = CAMEL_SERVICE_CLASS (camel_type_get_global_classfuncs (camel_service_get_type ()));
	
	/* virtual method overload */
	camel_service_class->connect = nntp_store_connect;
	camel_service_class->disconnect = nntp_store_disconnect;
	camel_service_class->query_auth_types = nntp_store_query_auth_types;
	camel_service_class->get_name = nntp_store_get_name;

	camel_store_class->get_folder = nntp_store_get_folder;
	camel_store_class->get_root_folder_name = nntp_store_get_root_folder_name;
	camel_store_class->get_folder_info = nntp_store_get_folder_info;
	camel_store_class->free_folder_info = camel_store_free_folder_info_full;

	camel_store_class->folder_subscribed = nntp_store_folder_subscribed;
	camel_store_class->subscribe_folder = nntp_store_subscribe_folder;
	camel_store_class->unsubscribe_folder = nntp_store_unsubscribe_folder;
}



static void
camel_nntp_store_init (gpointer object, gpointer klass)
{
	CamelRemoteStore *remote_store = CAMEL_REMOTE_STORE (object);
	CamelStore *store = CAMEL_STORE (object);

	remote_store->default_port = NNTP_PORT;

	store->flags = CAMEL_STORE_SUBSCRIPTIONS;
}

CamelType
camel_nntp_store_get_type (void)
{
	static CamelType camel_nntp_store_type = CAMEL_INVALID_TYPE;
	
	if (camel_nntp_store_type == CAMEL_INVALID_TYPE)	{
		camel_nntp_store_type = camel_type_register (CAMEL_REMOTE_STORE_TYPE, "CamelNNTPStore",
							     sizeof (CamelNNTPStore),
							     sizeof (CamelNNTPStoreClass),
							     (CamelObjectClassInitFunc) camel_nntp_store_class_init,
							     NULL,
							     (CamelObjectInitFunc) camel_nntp_store_init,
							     (CamelObjectFinalizeFunc) finalize);
	}
	
	return camel_nntp_store_type;
}


/**
 * camel_nntp_command: Send a command to a NNTP server.
 * @store: the NNTP store
 * @ret: a pointer to return the full server response in
 * @fmt: a printf-style format string, followed by arguments
 *
 * This command sends the command specified by @fmt and the following
 * arguments to the connected NNTP store specified by @store. It then
 * reads the server's response and parses out the status code. If
 * the caller passed a non-NULL pointer for @ret, camel_nntp_command
 * will set it to point to an buffer containing the rest of the
 * response from the NNTP server. (If @ret was passed but there was
 * no extended response, @ret will be set to NULL.) The caller must
 * free this buffer when it is done with it.
 *
 * Return value: the response code of the nntp command.
 **/
static int
camel_nntp_command_send_recv (CamelNNTPStore *store, CamelException *ex, char **ret, char *cmd)
{
	char *respbuf;
	int resp_code;
	gboolean again;

	do {
		again = FALSE;

		/* Send the command */
		if (camel_remote_store_send_string (CAMEL_REMOTE_STORE (store), ex, cmd) < 0) {
			return NNTP_PROTOCOL_ERROR;
		}

		/* Read the response */
		if (camel_remote_store_recv_line (CAMEL_REMOTE_STORE (store), &respbuf, ex) < 0) {
			if (ret)
				*ret = g_strdup (g_strerror (errno));
			return NNTP_PROTOCOL_ERROR;
		}

		resp_code = atoi (respbuf);

		/* this is kind of a gross hack, but since an auth challenge
		   can pop up at any time, and we want to shield this from our
		   callers, we handle authentication here. */
		if (resp_code == NNTP_AUTH_REQUIRED) {
			resp_code = camel_nntp_auth_authenticate (store, ex);
			if (resp_code != NNTP_AUTH_ACCEPTED) {
				return resp_code;
			}

			/* need to resend our command here */
			again = TRUE;
		}
	} while (again);

	if (ret) {
		*ret = strchr (respbuf, ' ');
		if (*ret)
			*ret = g_strdup (*ret + 1);
	}
	g_free (respbuf);

	return resp_code;
}

int
camel_nntp_command (CamelNNTPStore *store, CamelException *ex, char **ret, char *fmt, ...)
{
	char *cmdbuf;
	va_list ap;
	int resp_code;
	char *real_fmt;

	real_fmt = g_strdup_printf ("%s\r\n", fmt);

	va_start (ap, fmt);
	cmdbuf = g_strdup_vprintf (real_fmt, ap);
	va_end (ap);

	g_free (real_fmt);

	resp_code = camel_nntp_command_send_recv (store, ex, ret, cmdbuf);

	g_free (cmdbuf);

	return resp_code;
}

void
camel_nntp_store_subscribe_group (CamelStore *store,
				  const gchar *group_name)
{
	gchar *root_dir = camel_nntp_store_get_toplevel_dir(CAMEL_NNTP_STORE(store));
	char *ret = NULL;
	CamelException *ex = camel_exception_new();

	if (camel_exception_get_id (ex)) {
		g_free (root_dir);
		camel_exception_free (ex);
		return;
	}

	if (camel_nntp_command ( CAMEL_NNTP_STORE (store),
				 ex, &ret, "GROUP %s", group_name) == NNTP_GROUP_SELECTED) {
		/* we create an empty summary file here, so that when
                   the group is opened we'll know we need to build it. */
		gchar *summary_file;
		int fd;
		summary_file = g_strdup_printf ("%s/%s-ev-summary", root_dir, group_name);
		
		fd = open (summary_file, O_CREAT | O_RDWR, 0666);
		close (fd);

		g_free (summary_file);
	}
	if (ret) g_free (ret);

	g_free (root_dir);
	camel_exception_free (ex);
}

void
camel_nntp_store_unsubscribe_group (CamelStore *store,
				    const gchar *group_name)
{
	gchar *root_dir = camel_nntp_store_get_toplevel_dir(CAMEL_NNTP_STORE(store));
	gchar *summary_file;

	summary_file = g_strdup_printf ("%s/%s-ev-summary", root_dir, group_name);
	if (g_file_exists (summary_file))
		unlink (summary_file);
	g_free (summary_file);

	g_free (root_dir);
}

GList *
camel_nntp_store_list_subscribed_groups(CamelStore *store)
{
	GList *group_name_list = NULL;
	struct stat stat_buf;
	gint stat_error = 0;
	gchar *entry_name;
	gchar *full_entry_name;
	gchar *real_group_name;
	struct dirent *dir_entry;
	DIR *dir_handle;
	gchar *root_dir = camel_nntp_store_get_toplevel_dir(CAMEL_NNTP_STORE(store));

	dir_handle = opendir (root_dir);
	g_return_val_if_fail (dir_handle, NULL);

	/* read the first entry in the directory */
	dir_entry = readdir (dir_handle);
	while ((stat_error != -1) && (dir_entry != NULL)) {

		/* get the name of the next entry in the dir */
		entry_name = dir_entry->d_name;
		full_entry_name = g_strdup_printf ("%s/%s", root_dir, entry_name);
		stat_error = stat (full_entry_name, &stat_buf);
		g_free (full_entry_name);

		/* is it a normal file ending in -ev-summary ? */
		if ((stat_error != -1) && S_ISREG (stat_buf.st_mode)) {
			gboolean summary_suffix_found;

			real_group_name = string_prefix (entry_name, "-ev-summary",
							 &summary_suffix_found);

			if (summary_suffix_found)
				/* add the folder name to the list */
				group_name_list = g_list_append (group_name_list, 
								 real_group_name);
		}
		/* read next entry */
		dir_entry = readdir (dir_handle);
	}

	closedir (dir_handle);

	return group_name_list;
}

gchar *
camel_nntp_store_get_toplevel_dir (CamelNNTPStore *store)
{
	CamelURL *url = CAMEL_SERVICE (store)->url;
	char *top_dir;

	g_assert(url != NULL);

	top_dir = g_strdup_printf( "%s/evolution/news/%s",
				   g_get_home_dir (),
				   url->host );

	return top_dir;
}

static gboolean
ensure_news_dir_exists (CamelNNTPStore *store)
{
	gchar *dir = camel_nntp_store_get_toplevel_dir (store);

	if (access (dir, F_OK) == 0) {
		g_free (dir);
		return TRUE;
	}

	if (e_mkdir_hier (dir, S_IRWXU) == -1) {
		g_free (dir);
		return FALSE;
	}

	return TRUE;
}
