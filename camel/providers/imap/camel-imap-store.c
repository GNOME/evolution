/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-imap-store.c : class for an imap store */

/*
 *  Authors: Jeffrey Stedfast <fejj@helixcode.com>
 *
 *  Copyright 2000 Helix Code, Inc. (www.helixcode.com)
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


#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <e-util/e-util.h>

#include "camel-imap-store.h"
#include "camel-imap-folder.h"
#include "camel-imap-utils.h"
#include "camel-folder.h"
#include "camel-exception.h"
#include "camel-session.h"
#include "camel-stream.h"
#include "camel-stream-buffer.h"
#include "camel-stream-fs.h"
#include "camel-url.h"
#include "string-utils.h"

#define d(x) x

/* Specified in RFC 2060 */
#define IMAP_PORT 143

static CamelRemoteStoreClass *remote_store_class = NULL;

static gboolean imap_create (CamelFolder *folder, CamelException *ex);
static gboolean imap_connect (CamelService *service, CamelException *ex);
static gboolean imap_disconnect (CamelService *service, CamelException *ex);
static GList *query_auth_types_generic (CamelService *service, CamelException *ex);
static GList *query_auth_types_connected (CamelService *service, CamelException *ex);
static CamelFolder *get_folder (CamelStore *store, const char *folder_name, gboolean create,
				CamelException *ex);
static void imap_keepalive (CamelRemoteStore *store);
/*static gboolean stream_is_alive (CamelStream *istream);*/
static int camel_imap_status (char *cmdid, char *respbuf);

static void
camel_imap_store_class_init (CamelImapStoreClass *camel_imap_store_class)
{
	/* virtual method overload */
	CamelServiceClass *camel_service_class =
		CAMEL_SERVICE_CLASS (camel_imap_store_class);
	CamelStoreClass *camel_store_class =
		CAMEL_STORE_CLASS (camel_imap_store_class);
	CamelRemoteStoreClass *camel_remote_store_class =
		CAMEL_REMOTE_STORE_CLASS (camel_imap_store_class);
	
	remote_store_class = CAMEL_REMOTE_STORE_CLASS(camel_type_get_global_classfuncs 
						      (camel_remote_store_get_type ()));
	
	/* virtual method overload */
	camel_service_class->query_auth_types_generic = query_auth_types_generic;
	camel_service_class->query_auth_types_connected = query_auth_types_connected;
	camel_service_class->connect = imap_connect;
	camel_service_class->disconnect = imap_disconnect;
	
	camel_store_class->get_folder = get_folder;
	
	camel_remote_store_class->keepalive = imap_keepalive;
}

static void
camel_imap_store_init (gpointer object, gpointer klass)
{
	CamelService *service = CAMEL_SERVICE (object);
	CamelImapStore *imap_store = CAMEL_IMAP_STORE (object);
	
	service->url_flags |= (CAMEL_SERVICE_URL_NEED_USER |
			       CAMEL_SERVICE_URL_NEED_HOST |
			       CAMEL_SERVICE_URL_ALLOW_PATH);
	
	imap_store->dir_sep = g_strdup ("/"); /*default*/
	imap_store->current_folder = NULL;
}

CamelType
camel_imap_store_get_type (void)
{
	static CamelType camel_imap_store_type = CAMEL_INVALID_TYPE;
	
	if (camel_imap_store_type == CAMEL_INVALID_TYPE)	{
		camel_imap_store_type =
			camel_type_register (CAMEL_REMOTE_STORE_TYPE, "CamelImapStore",
					     sizeof (CamelImapStore),
					     sizeof (CamelImapStoreClass),
					     (CamelObjectClassInitFunc) camel_imap_store_class_init,
					     NULL,
					     (CamelObjectInitFunc) camel_imap_store_init,
					     (CamelObjectFinalizeFunc) NULL);
	}
	
	return camel_imap_store_type;
}

static CamelServiceAuthType password_authtype = {
	"Password",
	
	"This option will connect to the IMAP server using a "
	"plaintext password.",
	
	"",
	TRUE
};

static GList *
query_auth_types_connected (CamelService *service, CamelException *ex)
{
#if 0	
	GList *ret = NULL;
	gboolean passwd = TRUE;
	
	if (service->url) {
		passwd = try_connect (service, ex);
		if (camel_exception_get_id (ex) != CAMEL_EXCEPTION_NONE)
			return NULL;
	}
	
	if (passwd)
		ret = g_list_append (ret, &password_authtype);
	
	if (!ret) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Could not connect to IMAP server on %s.",
				      service->url->host ? service->url->host : 
				      "(unknown host)");
	}				      
	
	return ret;
#else
	g_warning ("imap::query_auth_types_connected: not implemented. Defaulting.");
	/* FIXME: use the classfunc instead of the local? */
	return query_auth_types_generic (service, ex);
#endif
}

static GList *
query_auth_types_generic (CamelService *service, CamelException *ex)
{
	GList *prev;
	
	prev = CAMEL_SERVICE_CLASS (remote_store_class)->query_auth_types_generic (service, ex);
	return g_list_prepend (prev, &password_authtype);
}

static gboolean
imap_connect (CamelService *service, CamelException *ex)
{
	CamelImapStore *store = CAMEL_IMAP_STORE (service);
	CamelSession *session = camel_service_get_session (CAMEL_SERVICE (store));
	gchar *buf, *result, *errbuf = NULL;
	gboolean authenticated = FALSE;
	gint status;
	
	if (CAMEL_SERVICE_CLASS (remote_store_class)->connect (service, ex) == FALSE)
		return FALSE;

	store->command = 0;
	g_free (store->dir_sep);
	store->dir_sep = g_strdup ("/");  /* default dir sep */
	
	/* Read the greeting, if any. */
	if (camel_remote_store_recv_line (CAMEL_REMOTE_STORE (service), &buf, ex) < 0) {
		return FALSE;
	}
	g_free (buf);
	
	/* authenticate the user */
	while (!authenticated) {
		if (errbuf) {
			/* We need to un-cache the password before prompting again */
			camel_session_query_authenticator (session,
							   CAMEL_AUTHENTICATOR_TELL, NULL,
							   TRUE, service, "password", ex);
			g_free (service->url->passwd);
			service->url->passwd = NULL;
		}
		
		if (!service->url->authmech && !service->url->passwd) {
			gchar *prompt;
			
			prompt = g_strdup_printf ("%sPlease enter the IMAP password for %s@%s",
						  errbuf ? errbuf : "", service->url->user, service->url->host);
			service->url->passwd =
				camel_session_query_authenticator (session,
								   CAMEL_AUTHENTICATOR_ASK, prompt,
								   TRUE, service, "password", ex);
			g_free (prompt);
			g_free (errbuf);
			errbuf = NULL;
			
			if (!service->url->passwd) {
				camel_exception_set (ex, CAMEL_EXCEPTION_USER_CANCEL, 
						     "You didn\'t enter a password.");
				return FALSE;
			}
		}
		
		status = camel_imap_command (store, NULL, ex, "LOGIN \"%s\" \"%s\"",
					     service->url->user,
					     service->url->passwd);
		
		if (status != CAMEL_IMAP_OK) {
			errbuf = g_strdup_printf ("Unable to authenticate to IMAP server.\n"
						  "%s\n\n",
						  camel_exception_get_description (ex));
		} else {
			g_message ("IMAP Service sucessfully authenticated user %s", service->url->user);
			authenticated = TRUE;
		}
	}
	
	/* Now lets find out the IMAP capabilities */
	status = camel_imap_command_extended (store, NULL, &result, ex, "CAPABILITY");
	
	if (status != CAMEL_IMAP_OK) {
		/* Non-fatal error... (ex is set) */
	}
	
	/* parse for capabilities here. */
	if (e_strstrcase (result, "IMAP4REV1"))
		store->server_level = IMAP_LEVEL_IMAP4REV1;
	else if (e_strstrcase (result, "IMAP4"))
		store->server_level = IMAP_LEVEL_IMAP4;
	else
		store->server_level = IMAP_LEVEL_UNKNOWN;
	
	if ((store->server_level >= IMAP_LEVEL_IMAP4REV1) || (e_strstrcase (result, "STATUS")))
		store->has_status_capability = TRUE;
	else
		store->has_status_capability = FALSE;
	
	g_free (result);
	
	/* We now need to find out which directory separator this daemon uses */
	status = camel_imap_command_extended (store, NULL, &result, ex, "LIST \"\" \"\"");
	
	if (status != CAMEL_IMAP_OK) {
		/* Again, this is non-fatal */
	} else {
		char *flags, *sep, *folder;
		
		if (imap_parse_list_response (result, "", &flags, &sep, &folder)) {
			if (*sep) {
				g_free (store->dir_sep);
				store->dir_sep = g_strdup (sep);
			}
		}
		
		g_free (flags);
		g_free (sep);
		g_free (folder);
	}
	
	g_free (result);
	
	return TRUE;
}

static gboolean
imap_disconnect (CamelService *service, CamelException *ex)
{
	CamelImapStore *store = CAMEL_IMAP_STORE (service);
	char *result;
	int status;
	
	/* send the logout command */
	status = camel_imap_command_extended (store, NULL, &result, ex, "LOGOUT");
	if (status != CAMEL_IMAP_OK) {
		/* Oh fuck it, we're disconnecting anyway... */
	}
	g_free (result);
	
	g_free (store->dir_sep);
	store->dir_sep = NULL;
	
	store->current_folder = NULL;
	
	return CAMEL_SERVICE_CLASS (remote_store_class)->disconnect (service, ex);
}

const gchar *
camel_imap_store_get_toplevel_dir (CamelImapStore *store)
{
	CamelURL *url = CAMEL_SERVICE (store)->url;
	
	g_assert (url != NULL);
	return url->path;
}

static gboolean
imap_folder_exists (CamelFolder *folder, CamelException *ex)
{
	CamelStore *store = CAMEL_STORE (folder->parent_store);
	CamelURL *url = CAMEL_SERVICE (store)->url;
	gchar *result, *folder_path, *dir_sep;
	gint status;
	
	dir_sep = CAMEL_IMAP_STORE (folder->parent_store)->dir_sep;
	
	g_return_val_if_fail (dir_sep, FALSE);
	
	if (url && url->path && *(url->path + 1) && strcmp (folder->full_name, "INBOX"))
		folder_path = g_strdup_printf ("%s%s%s", url->path + 1, dir_sep, folder->full_name);
	else
		folder_path = g_strdup (folder->full_name);
	
	status = camel_imap_command_extended (CAMEL_IMAP_STORE (folder->parent_store), NULL,
					      &result, ex, "EXAMINE %s", folder_path);
	
	if (status != CAMEL_IMAP_OK) {
		g_free (result);
		g_free (folder_path);
		return FALSE;
	}
	g_free (folder_path);
	g_free (result);
	
	return TRUE;
}

static gboolean
imap_create (CamelFolder *folder, CamelException *ex)
{
	CamelStore *store = CAMEL_STORE (folder->parent_store);
	CamelURL *url = CAMEL_SERVICE (store)->url;
	gchar *result, *folder_path, *dir_sep;
	gint status;
	
	g_return_val_if_fail (folder != NULL, FALSE);
	
	if (!(folder->full_name || folder->name)) {
		camel_exception_set (ex, CAMEL_EXCEPTION_FOLDER_INVALID,
				     "invalid folder path. Use set_name ?");
		return FALSE;
	}
	
	if (!strcmp (folder->full_name, "INBOX"))
		return TRUE;
	
	if (imap_folder_exists (folder, ex))
		return TRUE;
	
        /* create the directory for the subfolder */
	dir_sep = CAMEL_IMAP_STORE (folder->parent_store)->dir_sep;
	
	g_return_val_if_fail (dir_sep, FALSE);
	
	if (url && url->path && *(url->path + 1) && strcmp (folder->full_name, "INBOX"))
		folder_path = g_strdup_printf ("%s%s%s", url->path + 1, dir_sep, folder->full_name);
	else
		folder_path = g_strdup (folder->full_name);
	
	status = camel_imap_command_extended (CAMEL_IMAP_STORE (folder->parent_store), NULL,
					      &result, ex, "CREATE %s", folder_path);
	g_free (folder_path);
	
	if (status != CAMEL_IMAP_OK)
		return FALSE;
	
	return TRUE;
}

static gboolean
folder_is_selectable (CamelStore *store, const char *folder_path, CamelException *ex)
{
	char *result, *flags, *sep, *folder;
	int status;
	
	if (!strcmp (folder_path, "INBOX"))
		return TRUE;
	
	status = camel_imap_command_extended (CAMEL_IMAP_STORE (store), NULL,
					      &result, ex, "LIST \"\" %s", folder_path);
	if (status != CAMEL_IMAP_OK)
		return FALSE;
	
	if (imap_parse_list_response (result, "", &flags, &sep, &folder)) {
		gboolean retval;
		
		retval = !e_strstrcase (flags, "NoSelect");
		g_free (flags);
		g_free (sep);
		g_free (folder);
		
		return retval;
	}
	g_free (flags);
	g_free (sep);
	g_free (folder);
	
	return FALSE;
}

static CamelFolder *
get_folder (CamelStore *store, const char *folder_name, gboolean create, CamelException *ex)
{
	CamelURL *url = CAMEL_SERVICE (store)->url;
	CamelFolder *new_folder;
	char *folder_path, *dir_sep;
	
	g_return_val_if_fail (store != NULL, NULL);
	g_return_val_if_fail (folder_name != NULL, NULL);
	
	dir_sep = CAMEL_IMAP_STORE (store)->dir_sep;
	
	/* if we're trying to get the top-level dir, we really want the namespace */
	if (!dir_sep || !strcmp (folder_name, dir_sep))
		folder_path = g_strdup (url->path + 1);
	else
		folder_path = g_strdup (folder_name);
	
	new_folder = camel_imap_folder_new (store, folder_path, ex);
	
	/* this is the top-level dir, we already know it exists - it has to! */
	if (!strcmp (folder_name, dir_sep))
		return new_folder;
	
	if (create && !imap_create (new_folder, ex)) {
		if (!folder_is_selectable (store, folder_path, ex)) {
			camel_exception_clear (ex);
			new_folder->can_hold_messages = FALSE;
			return new_folder;
		} else {
			g_free (folder_path);
			camel_object_unref (CAMEL_OBJECT (new_folder));		
			return NULL;
		}
	}
	
	return new_folder;
}

static void
imap_keepalive (CamelRemoteStore *store)
{
	CamelImapStore *imap_store = CAMEL_IMAP_STORE (store);
	char *result;
	int status;
	CamelException ex;
	
	camel_exception_init (&ex);
	status = camel_imap_command_extended (imap_store, imap_store->current_folder, 
					      &result, &ex, "NOOP");
	camel_exception_clear (&ex);
	g_free (result);
}

#if 0
static gboolean
stream_is_alive (CamelStream *istream)
{
	CamelStreamFs *fs_stream;
	char buf;
	
	g_return_val_if_fail (istream != NULL, FALSE);
	
	fs_stream = CAMEL_STREAM_FS (CAMEL_STREAM_BUFFER (istream)->stream);
	g_return_val_if_fail (fs_stream->fd != -1, FALSE);
	
	if (read (fs_stream->fd, (void *) &buf, 0) == 0)
		return TRUE;
	
	return FALSE;
}
#endif

static int
camel_imap_status (char *cmdid, char *respbuf)
{
	char *retcode;
	
	if (respbuf) {
		if (!strncmp (respbuf, cmdid, strlen (cmdid))) {
			retcode = imap_next_word (respbuf);
			
			if (!strncmp (retcode, "OK", 2))
				return CAMEL_IMAP_OK;
			else if (!strncmp (retcode, "NO", 2))
				return CAMEL_IMAP_NO;
			else if (!strncmp (retcode, "BAD", 3))
				return CAMEL_IMAP_BAD;
		}
	}
	
	return CAMEL_IMAP_FAIL;
}

static gint
check_current_folder (CamelImapStore *store, CamelFolder *folder, char *fmt, CamelException *ex)
{
	CamelURL *url = CAMEL_SERVICE (store)->url;
	char *result, *folder_path, *dir_sep;
	int status;
	
	/* return OK if we meet one of the following criteria:
	 * 1. the command doesn't care about which folder we're in (folder == NULL)
	 * 2. if we're already in the right folder (store->current_folder == folder)
	 * 3. we're going to create a new folder */
	if (!folder || store->current_folder == folder || !strncmp (fmt, "CREATE", 5))
		return CAMEL_IMAP_OK;
	
	dir_sep = store->dir_sep;
	
	if (url && url->path && *(url->path + 1) && strcmp (folder->full_name, "INBOX"))
		folder_path = g_strdup_printf ("%s%s%s", url->path + 1, dir_sep, folder->full_name);
	else
		folder_path = g_strdup (folder->full_name);
	
	status = camel_imap_command_extended (store, NULL, &result, ex, "SELECT %s", folder_path);
	g_free (folder_path);
	
	if (!result || status != CAMEL_IMAP_OK) {
		store->current_folder = NULL;
		return status;
	}
	g_free (result);
	
	/* remember our currently selected folder */
	store->current_folder = folder;
	
	return CAMEL_IMAP_OK;
}

static gboolean
send_command (CamelImapStore *store, char **cmdid, char *fmt, va_list ap, CamelException *ex)
{
	gchar *cmdbuf;
	
	*cmdid = g_strdup_printf ("A%.5d", store->command++);
	
	cmdbuf = g_strdup_vprintf (fmt, ap);
	
	if (camel_remote_store_send_string (CAMEL_REMOTE_STORE (store), ex, "%s %s\r\n", *cmdid, cmdbuf) < 0) {
		g_free (cmdbuf);
		g_free (*cmdid);
		*cmdid = NULL;
		return FALSE;
	}
	
	g_free (cmdbuf);
	return TRUE;
}

static gint
slurp_response (CamelImapStore *store, CamelFolder *folder, char *cmdid, char **ret,
		gboolean stop_on_plus, CamelException *ex)
{
	gint status = CAMEL_IMAP_OK;
	GPtrArray *data, *expunged;
	gchar *respbuf;
	guint32 len = 0;
	gint recent = 0;
	gint i;
	
	data = g_ptr_array_new ();
	expunged = g_ptr_array_new ();
	
	while (1) {
		if (camel_remote_store_recv_line (CAMEL_REMOTE_STORE (store), &respbuf, ex) < 0) {
			for (i = 0; i < data->len; i++)
				g_free (data->pdata[i]);
			g_ptr_array_free (data, TRUE);
			g_ptr_array_free (expunged, TRUE);
			
			return CAMEL_IMAP_FAIL;
		}
		
		g_ptr_array_add (data, respbuf);
		len += strlen (respbuf) + 1;
		
		/* IMAP's last response starts with our command id or, sometimes, a plus */	
		if (stop_on_plus && *respbuf == '+') {
			status = CAMEL_IMAP_PLUS;
			break;
		}
		
		if (!strncmp (respbuf, cmdid, strlen (cmdid))) {
			status = camel_imap_status (cmdid, respbuf);
			break;
		}
		
		/* If recent or expunge flags were somehow set and this
		   response doesn't begin with a '*' then
		   recent/expunged must have been misdetected */
		if ((recent || expunged->len > 0) && *respbuf != '*') {
			d(fprintf (stderr, "hmmm, someone tried to pull a fast one on us.\n"));
			
			recent = 0;
			
			for (i = 0; i < expunged->len; i++) {
				g_free (expunged->pdata[i]);
				g_ptr_array_remove_index (expunged, i);
			}
		}
		
		/* Check for a RECENT in the untagged response */
		if (*respbuf == '*') {
			if (strstr (respbuf, "RECENT")) {
				char *rcnt;
				
				d(fprintf (stderr, "*** We may have found a 'RECENT' flag: %s\n", respbuf));
				/* Make sure it's in the form: "* %d RECENT" */
				rcnt = imap_next_word (respbuf);
				if (*rcnt >= '0' && *rcnt <= '9' && !strncmp ("RECENT", imap_next_word (rcnt), 6))
					recent = atoi (rcnt);
			} else if (strstr (respbuf, "EXPUNGE")) {
				char *id_str;
				int id;
				
				d(fprintf (stderr, "*** We may have found an 'EXPUNGE' flag: %s\n", respbuf));
				/* Make sure it's in the form: "* %d EXPUNGE" */
				id_str = imap_next_word (respbuf);
				if (*id_str >= '0' && *id_str <= '9' && !strncmp ("EXPUNGE", imap_next_word (id_str), 7)) {
					id = atoi (id_str);
					g_ptr_array_add (expunged, g_strdup_printf ("%d", id));
				}
			}
		}
	}
	
	/* Apply the 'recent' changes */
	if (folder && recent > 0)
		camel_imap_folder_changed (folder, recent, expunged, ex);
	
	if (status == CAMEL_IMAP_OK || status == CAMEL_IMAP_PLUS) {
		gchar *p;
		
		/* Command succeeded! Put the output into one big
		 * string of love. */
		
		*ret = g_new (char, len + 1);
		p = *ret;
		
		for (i = 0; i < data->len; i++) {
			char *datap;
			
			datap = (char *) data->pdata[i];
			if (*datap == '.')
				datap++;
			len = strlen (datap);
			memcpy (p, datap, len);
			p += len;
			*p++ = '\n';
		}
		
		*p = '\0';
	} else {
		/* Bummer. Try to grab what the server said. */
		if (respbuf) {
			char *word;
			
			word = imap_next_word (respbuf); /* should now point to status */
			
			word = imap_next_word (word);    /* points to fail message, if there is one */
			
			camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
					      "IMAP command failed: %s", word);
		}
		
		*ret = NULL;
	}
	
	/* Can this be put into the 'if succeeded' bit?
	 * Or can a failed command generate untagged responses? */
	
	for (i = 0; i < data->len; i++)
		g_free (data->pdata[i]);
	g_ptr_array_free (data, TRUE);
	
	for (i = 0; i < expunged->len; i++)
		g_free (expunged->pdata[i]);
	g_ptr_array_free (expunged, TRUE);
	
	return status;
}


/**
 * camel_imap_command: Send a command to a IMAP server.
 * @store: the IMAP store
 * @folder: The folder to perform the operation in
 * @ret: a pointer to return the full server response in
 * @ex: a CamelException.
 * @fmt: a printf-style format string, followed by arguments
 * 
 * This camel method sends the command specified by @fmt and the following
 * arguments to the connected IMAP store specified by @store. It then
 * reads the server's response and parses out the status code. If
 * the caller passed a non-NULL pointer for @ret, camel_imap_command
 * will set it to point to a buffer containing the rest of the
 * response from the IMAP server. (If @ret was passed but there was
 * no extended response, @ret will be set to NULL.) The caller function is
 * responsible for freeing @ret.
 * 
 * Return value: one of CAMEL_IMAP_OK (command executed successfully),
 * CAMEL_IMAP_NO (operational error message), CAMEL_IMAP_BAD (error
 * message from the server), or CAMEL_IMAP_FAIL (a protocol-level error
 * occurred, and Camel is uncertain of the result of the command.)
 **/
gint
camel_imap_command (CamelImapStore *store, CamelFolder *folder, CamelException *ex, char *fmt, ...)
{
	char *cmdid, *respbuf, *word;
	gint status = CAMEL_IMAP_OK;
	va_list ap;
	
	/* check for current folder */
	status = check_current_folder (store, folder, fmt, ex);
	if (status != CAMEL_IMAP_OK)
		return status;
	
	/* send the command */
	va_start (ap, fmt);
        if (!send_command (store, &cmdid, fmt, ap, ex)) {
		va_end (ap);
		g_free (cmdid);
		return CAMEL_IMAP_FAIL;
	}
	va_end (ap);
	
	/* read single line response */
	if (camel_remote_store_recv_line (CAMEL_REMOTE_STORE (store), &respbuf, ex) < 0) {
		g_free (cmdid);
		return CAMEL_IMAP_FAIL;
	}
	
	status = camel_imap_status (cmdid, respbuf);
	g_free (cmdid);
	
	if (status == CAMEL_IMAP_OK)
		return status;
	
	if (respbuf) {
		/* get error response and set exception accordingly */
		word = imap_next_word (respbuf); /* points to status */
		word = imap_next_word (word);    /* points to fail message, if there is one */
		
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "IMAP command failed: %s", word);
	} else {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "IMAP command failed: %s", "Unknown");
	}
	
	return status;
}

/**
 * camel_imap_command_extended: Send a command to a IMAP server and get
 * a multi-line response.
 * @store: the IMAP store
 * @folder: The folder to perform the operation in
 * @ret: a pointer to return the full server response in
 * @fmt: a printf-style format string, followed by arguments
 *
 * This camel method sends the IMAP command specified by @fmt and the
 * following arguments to the IMAP store specified by @store. If the
 * store is in a disconnected state, camel_imap_command_extended will first
 * re-connect the store before sending the specified IMAP command. It then
 * reads the server's response and parses out the status code. If the caller
 * passed a non-NULL pointer for @ret, camel_imap_command_extended will set
 * it to point to a buffer containing the rest of the response from the IMAP
 * server. (If @ret was passed but there was no extended response, @ret will
 * be set to NULL.) The caller function is responsible for freeing @ret.
 * 
 * This camel method gets the additional data returned by "multi-line" IMAP
 * commands, such as SELECT, LIST, FETCH, and various other commands.
 * The returned data is un-byte-stuffed, and has lines termined by
 * newlines rather than CR/LF pairs.
 * 
 * Return value: one of CAMEL_IMAP_OK (command executed successfully),
 * CAMEL_IMAP_NO (operational error message), CAMEL_IMAP_BAD (error
 * message from the server), or CAMEL_IMAP_FAIL (a protocol-level error
 * occurred, and Camel is uncertain of the result of the command.)
 **/

gint
camel_imap_command_extended (CamelImapStore *store, CamelFolder *folder, char **ret, CamelException *ex, char *fmt, ...)
{
	gint status = CAMEL_IMAP_OK;
	gchar *cmdid;
	va_list ap;
	
	status = check_current_folder (store, folder, fmt, ex);
	if (status != CAMEL_IMAP_OK)
		return status;
	
	/* send the command */
	va_start (ap, fmt);
        if (!send_command (store, &cmdid, fmt, ap, ex)) {
		va_end (ap);
		return CAMEL_IMAP_FAIL;
	}
	va_end (ap);
	
	return slurp_response (store, folder, cmdid, ret, FALSE, ex);
}

/**
 * camel_imap_command_preliminary: Send a preliminary command to the
 * IMAP server.
 * @store: the IMAP store
 * @cmdid: a pointer to return the command identifier (for use in
 * camel_imap_command_continuation)
 * @fmt: a printf-style format string, followed by arguments
 * 
 * This camel method sends a preliminary IMAP command specified by
 * @fmt and the following arguments to the IMAP store specified by
 * @store. This function is meant for use with multi-transactional
 * IMAP communications like Kerberos authentication and APPEND.
 * 
 * If the caller passed a non-NULL pointer for @ret,
 * camel_imap_command_preliminary will set it to point to a buffer
 * containing the rest of the response from the IMAP server. The
 * caller function is responsible for freeing @ret.
 * 
 * Return value: one of CAMEL_IMAP_PLUS, CAMEL_IMAP_NO, CAMEL_IMAP_BAD
 * or CAMEL_IMAP_FAIL
 * 
 * Note: on success (CAMEL_IMAP_PLUS), you will need to follow up with
 * a camel_imap_command_continuation call.
 **/
gint
camel_imap_command_preliminary (CamelImapStore *store, char **cmdid, CamelException *ex, char *fmt, ...)
{
	char *respbuf, *word;
	gint status = CAMEL_IMAP_OK;
	va_list ap;
	
	/* send the command */
	va_start (ap, fmt);
        if (!send_command (store, cmdid, fmt, ap, ex)) {
		va_end (ap);
		return CAMEL_IMAP_FAIL;
	}
	va_end (ap);
	
	/* read single line response */
	if (camel_remote_store_recv_line (CAMEL_REMOTE_STORE (store), &respbuf, ex) < 0)
		return CAMEL_IMAP_FAIL;
	
	/* Check for '+' which indicates server is ready for command continuation */
	if (*respbuf == '+') {
		g_free (cmdid);
		return CAMEL_IMAP_PLUS;
	}
	
	status = camel_imap_status (*cmdid, respbuf);
	g_free (cmdid);
	
	if (respbuf) {
		/* get error response and set exception accordingly */
		word = imap_next_word (respbuf); /* points to status */
		word = imap_next_word (word);    /* points to fail message, if there is one */
		
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "IMAP command failed: %s", word);
	} else {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "IMAP command failed: %s", "Unknown");
	}
	
	return status;
}

/**
 * camel_imap_command_continuation: Handle another transaction with the IMAP
 * server and possibly get a multi-line response.
 * @store: the IMAP store
 * @cmdid: The command identifier returned from camel_imap_command_preliminary
 * @ret: a pointer to return the full server response in
 * @cmdbuf: buffer containing the response/request data
 *
 * This method is for sending continuing responses to the IMAP server. Meant
 * to be used as a followup to camel_imap_command_preliminary.
 * camel_imap_command_continuation will set @ret to point to a buffer
 * containing the rest of the response from the IMAP server. The
 * caller function is responsible for freeing @ret.
 * 
 * Return value: one of CAMEL_IMAP_PLUS (command requires additional data),
 * CAMEL_IMAP_OK (command executed successfully),
 * CAMEL_IMAP_NO (operational error message),
 * CAMEL_IMAP_BAD (error message from the server), or
 * CAMEL_IMAP_FAIL (a protocol-level error occurred, and Camel is uncertain
 * of the result of the command.)
 **/
gint
camel_imap_command_continuation (CamelImapStore *store, char **ret, char *cmdid, char *cmdbuf, CamelException *ex)
{
	if (camel_remote_store_send_string (CAMEL_REMOTE_STORE (store), ex, "%s\r\n", cmdbuf) < 0) {
		if (ret)
			*ret = NULL;
		return CAMEL_IMAP_FAIL;
	}
	
	return slurp_response (store, NULL, cmdid, ret, TRUE, ex);
}

/**
 * camel_imap_command_continuation_with_stream: Handle another transaction with the IMAP
 * server and possibly get a multi-line response.
 * @store: the IMAP store
 * @cmdid: The command identifier returned from camel_imap_command_preliminary
 * @ret: a pointer to return the full server response in
 * @cstream: a CamelStream containing a continuation response.
 * 
 * This method is for sending continuing responses to the IMAP server. Meant
 * to be used as a followup to camel_imap_command_preliminary.
 * camel_imap_command_continuation will set @ret to point to a buffer
 * containing the rest of the response from the IMAP server. The
 * caller function is responsible for freeing @ret.
 * 
 * Return value: one of CAMEL_IMAP_PLUS (command requires additional data),
 * CAMEL_IMAP_OK (command executed successfully),
 * CAMEL_IMAP_NO (operational error message),
 * CAMEL_IMAP_BAD (error message from the server), or
 * CAMEL_IMAP_FAIL (a protocol-level error occurred, and Camel is uncertain
 * of the result of the command.)
 **/
gint
camel_imap_command_continuation_with_stream (CamelImapStore *store, char **ret, char *cmdid,
					     CamelStream *cstream, CamelException *ex)
{
	if (camel_remote_store_send_stream (CAMEL_REMOTE_STORE (store), cstream, ex) < 0) {
		if (ret)
			*ret = NULL;
		return CAMEL_IMAP_FAIL;
	}
	
	return slurp_response (store, NULL, cmdid, ret, TRUE, ex);
}
