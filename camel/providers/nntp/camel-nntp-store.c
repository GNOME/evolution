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

#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libgnome/libgnome.h"

#include "camel-nntp-resp-codes.h"
#include "camel-folder-summary.h"
#include "camel-nntp-store.h"
#include "camel-nntp-folder.h"
#include "camel-nntp-auth.h"
#include "camel-exception.h"
#include "camel-url.h"
#include "string-utils.h"

#define NNTP_PORT 119

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
		/* if we can't get the overview format, we should
                   disable OVER support */
		g_warning ("server reported support of OVER but LIST OVERVIEW.FMT failed."
			   "  disabling OVER\n");
		store->extensions &= ~CAMEL_NNTP_EXT_OVER;
		return;
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
				      "Could not open directory for news server: %s",
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

	/* if the server supports the OVER extension, get the overview.fmt */
	if (store->extensions & CAMEL_NNTP_EXT_OVER)
		camel_nntp_store_get_overview_fmt (store, ex);

	return TRUE;
}

static gboolean
nntp_store_disconnect (CamelService *service, CamelException *ex)
{
	CamelNNTPStore *store = CAMEL_NNTP_STORE (service);

	camel_nntp_command (store, ex, NULL, "QUIT");

	if (store->newsrc)
		camel_nntp_newsrc_write (store->newsrc);

	if (!service_class->disconnect (service, ex))
		return FALSE;

	return TRUE;
}

static char *
nntp_store_get_name (CamelService *service, gboolean brief)
{
	/* Same info for long and brief... */
	return g_strdup_printf ("USENET news via %s", service->url->host);
}

static CamelFolder *
nntp_store_get_folder (CamelStore *store, const gchar *folder_name,
		       gboolean get_folder, CamelException *ex)
{
	CamelNNTPStore *nntp_store = CAMEL_NNTP_STORE (store);

	printf ("get_folder called on folder_name=%s\n", folder_name);

	/* if we haven't already read our .newsrc, read it now */
	if (!nntp_store->newsrc)
		nntp_store->newsrc = 
		camel_nntp_newsrc_read_for_server (CAMEL_SERVICE(store)->url->host);

	if (!nntp_store->newsrc) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Unable to open or create .newsrc file for %s: %s",
				      CAMEL_SERVICE(store)->url->host,
				      strerror(errno));
		return NULL;
	}

	return camel_nntp_folder_new (store, folder_name, ex);
}

static void
finalize (CamelObject *object)
{
	/* Done for us now */
	/*CamelException ex;
	 *
	 *camel_exception_init (&ex);
	 *nntp_store_disconnect (CAMEL_SERVICE (object), &ex);
	 *camel_exception_clear (&ex);
	 */
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
	camel_service_class->get_name = nntp_store_get_name;

	camel_store_class->get_folder = nntp_store_get_folder;
}



static void
camel_nntp_store_init (gpointer object, gpointer klass)
{
	CamelService *service = CAMEL_SERVICE (object);

	service->url_flags = CAMEL_SERVICE_URL_NEED_HOST;
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
	struct stat sb;
	int rv;

	rv = stat (dir, &sb);
	if (-1 == rv && errno != ENOENT) {
		g_free (dir);
		return FALSE;
	}

	if (S_ISDIR (sb.st_mode)) {
		g_free (dir);
		return TRUE;
	}
	else {
		rv = mkdir (dir, 0777);
		g_free (dir);

		if (-1 == rv)
			return FALSE;
		else
			return TRUE;
	}
}
