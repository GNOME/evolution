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
#include <sys/socket.h>
#include <netinet/in.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libgnome/libgnome.h"

#include "camel-nntp-store.h"
#include "camel-nntp-folder.h"
#include "camel-stream-buffer.h"
#include "camel-stream-fs.h"
#include "camel-exception.h"
#include "camel-url.h"
#include "string-utils.h"

#define NNTP_PORT 119

static CamelServiceClass *service_class = NULL;

/* Returns the class for a CamelNNTPStore */
#define CNNTPS_CLASS(so) CAMEL_NNTP_STORE_CLASS (GTK_OBJECT(so)->klass)
#define CF_CLASS(so) CAMEL_FOLDER_CLASS (GTK_OBJECT(so)->klass)
#define CNNTPF_CLASS(so) CAMEL_NNTP_FOLDER_CLASS (GTK_OBJECT(so)->klass)

static CamelFolder *_get_folder (CamelStore *store, const gchar *folder_name, CamelException *ex);
static char *_get_folder_name (CamelStore *store, const char *folder_name,
			       CamelException *ex);

static gboolean nntp_connect (CamelService *service, CamelException *ex);
static gboolean nntp_disconnect (CamelService *service, CamelException *ex);

static void
camel_nntp_store_class_init (CamelNNTPStoreClass *camel_nntp_store_class)
{
	CamelStoreClass *camel_store_class = CAMEL_STORE_CLASS (camel_nntp_store_class);

	service_class = gtk_type_class (camel_service_get_type ());
	
	/* virtual method overload */
	camel_store_class->get_folder = _get_folder;
	camel_store_class->get_folder_name = _get_folder_name;
}



static void
camel_nntp_store_init (gpointer object, gpointer klass)
{
	CamelService *service = CAMEL_SERVICE (object);

	service->url_flags = CAMEL_SERVICE_URL_NEED_HOST;
}

GtkType
camel_nntp_store_get_type (void)
{
	static GtkType camel_nntp_store_type = 0;
	
	if (!camel_nntp_store_type)	{
		GtkTypeInfo camel_nntp_store_info =	
		{
			"CamelNNTPStore",
			sizeof (CamelNNTPStore),
			sizeof (CamelNNTPStoreClass),
			(GtkClassInitFunc) camel_nntp_store_class_init,
			(GtkObjectInitFunc) camel_nntp_store_init,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		camel_nntp_store_type = gtk_type_unique (CAMEL_STORE_TYPE, &camel_nntp_store_info);
	}
	
	return camel_nntp_store_type;
}

static CamelFolder *
_get_folder (CamelStore *store, const gchar *folder_name, CamelException *ex)
{
	CamelNNTPFolder *new_nntp_folder;
	CamelFolder *new_folder;

	/* XXX */
	folder_name = "netscape.public.mozilla.announce";

	/* check if folder has already been created */
	/* call the standard routine for that when  */
	/* it is done ... */

	new_nntp_folder =  gtk_type_new (CAMEL_NNTP_FOLDER_TYPE);
	new_folder = CAMEL_FOLDER (new_nntp_folder);
	
	/* XXX We shouldn't be passing NULL here, but it's equivalent to
	 * what was there before, and there's no
	 * CamelNNTPFolder::get_subfolder yet anyway...
	 */
	CF_CLASS (new_folder)->init (new_folder, store, NULL,
				     folder_name, '/', ex);
	
	return new_folder;
}


static char *
_get_folder_name (CamelStore *store, const char *folder_name,
		  CamelException *ex)
{
	return g_strdup (folder_name);
}

/**
 * camel_nntp_store_open: Connect to the server if we are currently
 * disconnected.
 * @store: the store
 * @ex: a CamelException
 *
 **/
void
camel_nntp_store_open (CamelNNTPStore *store, CamelException *ex)
{
	CamelService *service = CAMEL_SERVICE (store);

	if (!camel_service_is_connected (service))
		nntp_connect (service, ex);
}

/**
 * camel_nntp_store_close: Close the connection to the server
 * @store: the store
 * @ex: a CamelException
 *
 **/
void
camel_nntp_store_close (CamelNNTPStore *store, gboolean expunge,
			CamelException *ex)
{
	camel_nntp_command (store, NULL, "QUIT");

	nntp_disconnect (CAMEL_SERVICE (store), ex);
}

static gboolean
nntp_connect (CamelService *service, CamelException *ex)
{
	struct hostent *h;
	struct sockaddr_in sin;
	int fd, status, apoplen;
	char *buf, *apoptime, *pass;
	CamelNNTPStore *store = CAMEL_NNTP_STORE (service);

	if (!service_class->connect (service, ex))
		return FALSE;

	h = camel_service_gethost (service, ex);
	if (!h)
		return FALSE;

	sin.sin_family = h->h_addrtype;
	sin.sin_port = htons (service->url->port ? service->url->port : NNTP_PORT);
	memcpy (&sin.sin_addr, h->h_addr, sizeof (sin.sin_addr));

	fd = socket (h->h_addrtype, SOCK_STREAM, 0);
	if (fd == -1 ||
	    connect (fd, (struct sockaddr *)&sin, sizeof(sin)) == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Could not connect to %s (port %s): %s",
				      service->url->host, service->url->port,
				      strerror(errno));
		if (fd > -1)
			close (fd);
		return FALSE;
	}

	store->ostream = camel_stream_fs_new_with_fd (fd);
	store->istream = camel_stream_buffer_new (store->ostream,
						  CAMEL_STREAM_BUFFER_READ);

	/* Read the greeting */
	buf = camel_stream_buffer_read_line (CAMEL_STREAM_BUFFER (store->istream));
	if (!buf) {
		return -1;
	}

	g_free (buf);

	/* get a list of extensions that the server supports */

	return TRUE;
}

static gboolean
nntp_disconnect (CamelService *service, CamelException *ex)
{
	CamelNNTPStore *store = CAMEL_NNTP_STORE (service);

	if (!service->connected)
		return TRUE;

	if (!service_class->disconnect (service, ex))
		return FALSE;

	gtk_object_unref (GTK_OBJECT (store->ostream));
	gtk_object_unref (GTK_OBJECT (store->istream));
	store->ostream = NULL;
	store->istream = NULL;
	return TRUE;
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
 * Return value: one of CAMEL_NNTP_OK (command executed successfully),
 * CAMEL_NNTP_ERR (command encounted an error), or CAMEL_NNTP_FAIL
 * (a protocol-level error occurred, and Camel is uncertain of the
 * result of the command.)
 **/
int
camel_nntp_command (CamelNNTPStore *store, char **ret, char *fmt, ...)
{
	char *cmdbuf, *respbuf;
	va_list ap;
	int status;
	int resp_code;
	CamelException *ex;

	va_start (ap, fmt);
	cmdbuf = g_strdup_vprintf (fmt, ap);
	va_end (ap);

	ex = camel_exception_new();

	/* make sure we're connected */
	if (store->ostream == NULL)
		nntp_connect (CAMEL_SERVICE (store), ex);

	if (camel_exception_get_id (ex)) {
		camel_exception_free (ex);
		return CAMEL_NNTP_FAIL;
	}

	/* Send the command */
	camel_stream_write (store->ostream, cmdbuf, strlen (cmdbuf));
	g_free (cmdbuf);
	camel_stream_write (store->ostream, "\r\n", 2);

	/* Read the response */
	respbuf = camel_stream_buffer_read_line (CAMEL_STREAM_BUFFER (store->istream));
	resp_code = atoi (respbuf);

	if (resp_code < 400)
		status = CAMEL_NNTP_OK;
	else if (resp_code < 500)
		status = CAMEL_NNTP_ERR;
	else
		status = CAMEL_NNTP_FAIL;

	if (ret) {
		*ret = strchr (respbuf, ' ');
		if (*ret)
			*ret = g_strdup (*ret + 1);
	}
	g_free (respbuf);

	return status;
}

/**
 * camel_nntp_command_get_additional_data: get "additional data" from
 * a NNTP command.
 * @store: the NNTP store
 *
 * This command gets the additional data returned by "multi-line" POP
 * commands, such as LIST, RETR, TOP, and UIDL. This command _must_
 * be called after a successful (CAMEL_NNTP_OK) call to
 * camel_nntp_command for a command that has a multi-line response.
 * The returned data is un-byte-stuffed, and has lines termined by
 * newlines rather than CR/LF pairs.
 *
 * Return value: the data, which the caller must free.
 **/
char *
camel_nntp_command_get_additional_data (CamelNNTPStore *store)
{
	CamelStreamBuffer *stream = CAMEL_STREAM_BUFFER (store->istream);
	GPtrArray *data;
	char *buf;
	int i, status = CAMEL_NNTP_OK;

	data = g_ptr_array_new ();
	while (1) {
		buf = camel_stream_buffer_read_line (stream);
		if (!buf) {
			status = CAMEL_NNTP_FAIL;
			break;
		}

		if (!strcmp (buf, "."))
			break;
		if (*buf == '.')
			memmove (buf, buf + 1, strlen (buf));
		g_ptr_array_add (data, buf);
	}

	if (status == CAMEL_NNTP_OK) {
		/* Append an empty string to the end of the array
		 * so when we g_strjoinv it, we get a "\n" after
		 * the last real line.
		 */
		g_ptr_array_add (data, "");
		g_ptr_array_add (data, NULL);
		buf = g_strjoinv ("\n", (char **)data->pdata);
	} else
		buf = NULL;

	for (i = 0; i < data->len - 2; i++)
		g_free (data->pdata[i]);
	g_ptr_array_free (data, TRUE);

	return buf;
}

void
camel_nntp_store_subscribe_group (CamelStore *store,
				  const gchar *group_name)
{
	gchar *root_dir = camel_nntp_store_get_toplevel_dir(CAMEL_NNTP_STORE(store));
	char *ret = NULL;
	CamelException *ex = camel_exception_new();

	camel_nntp_store_open(CAMEL_NNTP_STORE(store), ex);

	if (camel_exception_get_id (ex)) {
		g_free (root_dir);
		camel_exception_free (ex);
		return;
	}

	if (CAMEL_NNTP_OK  == camel_nntp_command ( CAMEL_NNTP_STORE (store),
						   &ret, "GROUP %s", group_name)) {
		gchar *summary_file;
		summary_file = g_strdup_printf ("%s/%s-ev-summary", root_dir, group_name);
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
	char *news_dir;
	char *top_dir;

	g_assert(url != NULL);

	news_dir = gnome_util_prepend_user_home ("evolution/news");

	top_dir = g_strdup_printf( "%s/%s",
				   news_dir,
				   url->host );

	g_free (news_dir);

	return top_dir;
}
