/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-imap-command.c: IMAP command sending/parsing routines */

/*
 *  Authors:
 *    Dan Winship <danw@ximian.com>
 *    Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2000, 2001 Ximian, Inc.
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

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "camel-imap-command.h"
#include "camel-imap-utils.h"
#include "camel-imap-folder.h"
#include "camel-imap-store.h"
#include "camel-imap-store-summary.h"
#include "camel-imap-private.h"
#include <camel/camel-exception.h>
#include <camel/camel-private.h>
#include <camel/camel-utf8.h>
#include <camel/camel-session.h>
#include "camel-i18n.h"

extern int camel_verbose_debug;

static gboolean imap_command_start (CamelImapStore *store, CamelFolder *folder,
				    const char *cmd, CamelException *ex);
CamelImapResponse *imap_read_response (CamelImapStore *store,
				       CamelException *ex);
static char *imap_read_untagged (CamelImapStore *store, char *line,
				 CamelException *ex);
static char *imap_command_strdup_vprintf (CamelImapStore *store,
					  const char *fmt, va_list ap);
static char *imap_command_strdup_printf (CamelImapStore *store,
					 const char *fmt, ...);

/**
 * camel_imap_command:
 * @store: the IMAP store
 * @folder: The folder to perform the operation in (or %NULL if not
 * relevant).
 * @ex: a CamelException
 * @fmt: a sort of printf-style format string, followed by arguments
 *
 * This function calls camel_imap_command_start() to send the
 * command, then reads the complete response to it using
 * camel_imap_command_response() and returns a CamelImapResponse
 * structure.
 *
 * As a special case, if @fmt is %NULL, it will just select @folder
 * and return the response from doing so.
 *
 * See camel_imap_command_start() for details on @fmt.
 *
 * On success, the store's connect_lock will be locked. It will be freed
 * when you call camel_imap_response_free. (The lock is recursive, so
 * callers can grab and release it themselves if they need to run
 * multiple commands atomically.)
 *
 * Return value: %NULL if an error occurred (in which case @ex will
 * be set). Otherwise, a CamelImapResponse describing the server's
 * response, which the caller must free with camel_imap_response_free().
 **/
CamelImapResponse *
camel_imap_command (CamelImapStore *store, CamelFolder *folder,
		    CamelException *ex, const char *fmt, ...)
{
	va_list ap;
	char *cmd;
	
	CAMEL_SERVICE_LOCK (store, connect_lock);
	
	if (fmt) {
		va_start (ap, fmt);
		cmd = imap_command_strdup_vprintf (store, fmt, ap);
		va_end (ap);
	} else {
		camel_object_ref(folder);
		if (store->current_folder)
			camel_object_unref(store->current_folder);
		store->current_folder = folder;
		cmd = imap_command_strdup_printf (store, "SELECT %F", folder->full_name);
	}
	
	if (!imap_command_start (store, folder, cmd, ex)) {
		g_free (cmd);
		CAMEL_SERVICE_UNLOCK (store, connect_lock);
		return NULL;
	}
	g_free (cmd);
	
	return imap_read_response (store, ex);
}

/**
 * camel_imap_command_start:
 * @store: the IMAP store
 * @folder: The folder to perform the operation in (or %NULL if not
 * relevant).
 * @ex: a CamelException
 * @fmt: a sort of printf-style format string, followed by arguments
 *
 * This function makes sure that @folder (if non-%NULL) is the
 * currently-selected folder on @store and then sends the IMAP command
 * specified by @fmt and the following arguments.
 *
 * @fmt can include the following %-escapes ONLY:
 *	%s, %d, %%: as with printf
 *	%S: an IMAP "string" (quoted string or literal)
 *	%F: an IMAP folder name
 *
 * %S strings will be passed as literals if the server supports LITERAL+
 * and quoted strings otherwise. (%S does not support strings that
 * contain newlines.)
 *
 * %F will have the imap store's namespace prepended and then be processed
 * like %S.
 *
 * On success, the store's connect_lock will be locked. It will be
 * freed when %CAMEL_IMAP_RESPONSE_TAGGED or %CAMEL_IMAP_RESPONSE_ERROR
 * is returned from camel_imap_command_response(). (The lock is
 * recursive, so callers can grab and release it themselves if they
 * need to run multiple commands atomically.)
 *
 * Return value: %TRUE if the command was sent successfully, %FALSE if
 * an error occurred (in which case @ex will be set).
 **/
gboolean
camel_imap_command_start (CamelImapStore *store, CamelFolder *folder,
			  CamelException *ex, const char *fmt, ...)
{
	va_list ap;
	char *cmd;
	gboolean ok;
	
	va_start (ap, fmt);
	cmd = imap_command_strdup_vprintf (store, fmt, ap);
	va_end (ap);
	
	CAMEL_SERVICE_LOCK (store, connect_lock);
	ok = imap_command_start (store, folder, cmd, ex);
	g_free (cmd);
	
	if (!ok)
		CAMEL_SERVICE_UNLOCK (store, connect_lock);
	return ok;
}

static gboolean
imap_command_start (CamelImapStore *store, CamelFolder *folder,
		    const char *cmd, CamelException *ex)
{
	ssize_t nwritten;
	
	/* Check for current folder */
	if (folder && folder != store->current_folder) {
		CamelImapResponse *response;
		CamelException internal_ex;
		
		response = camel_imap_command (store, folder, ex, NULL);
		if (!response)
			return FALSE;
		camel_exception_init (&internal_ex);
		camel_imap_folder_selected (folder, response, &internal_ex);
		camel_imap_response_free (store, response);
		if (camel_exception_is_set (&internal_ex)) {
			camel_exception_xfer (ex, &internal_ex);
			return FALSE;
		}
	}
	
	/* Send the command */
	if (camel_verbose_debug) {
		const char *mask;
		
		if (!strncmp ("LOGIN \"", cmd, 7))
			mask = "LOGIN \"xxx\" xxx";
		else if (!strncmp ("LOGIN {", cmd, 7))
			mask = "LOGIN {N+}\r\nxxx {N+}\r\nxxx";
		else if (!strncmp ("LOGIN ", cmd, 6))
			mask = "LOGIN xxx xxx";
		else
			mask = cmd;
		
		fprintf (stderr, "sending : %c%.5d %s\r\n", store->tag_prefix, store->command, mask);
	}
	
	nwritten = camel_stream_printf (store->ostream, "%c%.5d %s\r\n",
					store->tag_prefix, store->command++, cmd);
	
	if (nwritten == -1) {
		if (errno == EINTR)
			camel_exception_set (ex, CAMEL_EXCEPTION_USER_CANCEL,
					     _("Operation cancelled"));
		else
			camel_exception_set (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
					     g_strerror (errno));
		
		camel_service_disconnect (CAMEL_SERVICE (store), FALSE, NULL);
		return FALSE;
	}
	
	return TRUE;
}

/**
 * camel_imap_command_continuation:
 * @store: the IMAP store
 * @cmd: buffer containing the response/request data
 * @cmdlen: command length
 * @ex: a CamelException
 *
 * This method is for sending continuing responses to the IMAP server
 * after camel_imap_command() or camel_imap_command_response() returns
 * a continuation response.
 * 
 * This function assumes you have an exclusive lock on the imap stream.
 *
 * Return value: as for camel_imap_command(). On failure, the store's
 * connect_lock will be released.
 **/
CamelImapResponse *
camel_imap_command_continuation (CamelImapStore *store, const char *cmd,
				 size_t cmdlen, CamelException *ex)
{
	if (!camel_imap_store_connected (store, ex))
		return NULL;
	
	if (camel_stream_write (store->ostream, cmd, cmdlen) == -1 ||
	    camel_stream_write (store->ostream, "\r\n", 2) == -1) {
		if (errno == EINTR)
			camel_exception_set (ex, CAMEL_EXCEPTION_USER_CANCEL,
					     _("Operation cancelled"));
		else
			camel_exception_set (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
					     g_strerror (errno));
		camel_service_disconnect (CAMEL_SERVICE (store), FALSE, NULL);
		CAMEL_SERVICE_UNLOCK (store, connect_lock);
		return NULL;
	}
	
	return imap_read_response (store, ex);
}

/**
 * camel_imap_command_response:
 * @store: the IMAP store
 * @response: a pointer to pass back the response data in
 * @ex: a CamelException
 *
 * This reads a single tagged, untagged, or continuation response from
 * @store into *@response. The caller must free the string when it is
 * done with it.
 *
 * Return value: One of %CAMEL_IMAP_RESPONSE_CONTINUATION,
 * %CAMEL_IMAP_RESPONSE_UNTAGGED, %CAMEL_IMAP_RESPONSE_TAGGED, or
 * %CAMEL_IMAP_RESPONSE_ERROR. If either of the last two, @store's
 * command lock will be unlocked.
 **/
CamelImapResponseType
camel_imap_command_response (CamelImapStore *store, char **response,
			     CamelException *ex)
{
	CamelImapResponseType type;
	char *respbuf;
	
	if (camel_imap_store_readline (store, &respbuf, ex) < 0) {
		CAMEL_SERVICE_UNLOCK (store, connect_lock);
		return CAMEL_IMAP_RESPONSE_ERROR;
	}
	
	switch (*respbuf) {
	case '*':
		if (!strncasecmp (respbuf, "* BYE", 5)) {
			/* Connection was lost, no more data to fetch */
			camel_service_disconnect (CAMEL_SERVICE (store), FALSE, NULL);
			camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
					      _("Server unexpectedly disconnected: %s"),
					      _("Unknown error")); /* g_strerror (104));  FIXME after 1.0 is released */
			store->connected = FALSE;
			g_free (respbuf);
			respbuf = NULL;
			type = CAMEL_IMAP_RESPONSE_ERROR;
			break;
		}
		
		/* Read the rest of the response. */
		type = CAMEL_IMAP_RESPONSE_UNTAGGED;
		respbuf = imap_read_untagged (store, respbuf, ex);
		if (!respbuf)
			type = CAMEL_IMAP_RESPONSE_ERROR;
		else if (!strncasecmp (respbuf, "* OK [ALERT]", 12)) {
			char *msg;

			/* for imap ALERT codes, account user@host */
			msg = g_strdup_printf(_("Alert from IMAP server %s@%s:\n%s"),
					      ((CamelService *)store)->url->user, ((CamelService *)store)->url->host, respbuf+12);
			camel_session_alert_user(((CamelService *)store)->session, CAMEL_SESSION_ALERT_WARNING, msg, FALSE);
			g_free(msg);
		}
		
		break;
	case '+':
		type = CAMEL_IMAP_RESPONSE_CONTINUATION;
		break;
	default:
		type = CAMEL_IMAP_RESPONSE_TAGGED;
		break;
	}
	*response = respbuf;
	
	if (type == CAMEL_IMAP_RESPONSE_ERROR ||
	    type == CAMEL_IMAP_RESPONSE_TAGGED)
		CAMEL_SERVICE_UNLOCK (store, connect_lock);
	
	return type;
}

CamelImapResponse *
imap_read_response (CamelImapStore *store, CamelException *ex)
{
	CamelImapResponse *response;
	CamelImapResponseType type;
	char *respbuf, *p;
	
	/* Get another lock so that when we reach the tagged
	 * response and camel_imap_command_response unlocks,
	 * we're still locked. This lock is owned by response
	 * and gets unlocked when response is freed.
	 */
	CAMEL_SERVICE_LOCK (store, connect_lock);
	
	response = g_new0 (CamelImapResponse, 1);
	if (store->current_folder && camel_disco_store_status (CAMEL_DISCO_STORE (store)) != CAMEL_DISCO_STORE_RESYNCING) {
		response->folder = store->current_folder;
		camel_object_ref (CAMEL_OBJECT (response->folder));
	}
	
	response->untagged = g_ptr_array_new ();
	while ((type = camel_imap_command_response (store, &respbuf, ex))
	       == CAMEL_IMAP_RESPONSE_UNTAGGED)
		g_ptr_array_add (response->untagged, respbuf);
	
	if (type == CAMEL_IMAP_RESPONSE_ERROR) {
		camel_imap_response_free_without_processing (store, response);
		return NULL;
	}
	
	response->status = respbuf;
	
	/* Check for OK or continuation response. */
	if (*respbuf == '+')
		return response;
	p = strchr (respbuf, ' ');
	if (p && !strncasecmp (p, " OK", 3))
		return response;
	
	/* We should never get BAD, or anything else but +, OK, or NO
	 * for that matter.
	 */
	if (!p || strncasecmp (p, " NO", 3) != 0) {
		g_warning ("Unexpected response from IMAP server: %s",
			   respbuf);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      _("Unexpected response from IMAP "
					"server: %s"), respbuf);
		camel_imap_response_free_without_processing (store, response);
		return NULL;
	}
	
	p += 3;
	if (!*p++)
		p = NULL;
	camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
			      _("IMAP command failed: %s"),
			      p ? p : _("Unknown error"));
	camel_imap_response_free_without_processing (store, response);
	return NULL;
}

/* Given a line that is the start of an untagged response, read and
 * return the complete response, which may include an arbitrary number
 * of literals.
 */
static char *
imap_read_untagged (CamelImapStore *store, char *line, CamelException *ex)
{
	int fulllen, ldigits, nread, i;
	unsigned int length;
	GPtrArray *data;
	GString *str;
	char *end, *p, *s, *d;
	
	p = strrchr (line, '{');
	if (!p)
		return line;
	
	data = g_ptr_array_new ();
	fulllen = 0;
	
	while (1) {
		str = g_string_new (line);
		g_free (line);
		fulllen += str->len;
		g_ptr_array_add (data, str);
		
		p = strrchr (str->str, '{');
		if (!p)
			break;
		
		length = strtoul (p + 1, &end, 10);
		if (*end != '}' || *(end + 1) || end == p + 1 || length >= UINT_MAX - 2)
			break;
		ldigits = end - (p + 1);
		
		/* Read the literal */
		str = g_string_sized_new (length + 2);
		str->str[0] = '\n';
		nread = camel_stream_read (store->istream, str->str + 1, length);
		if (nread == -1) {
			if (errno == EINTR)
				camel_exception_set (ex, CAMEL_EXCEPTION_USER_CANCEL,
						     _("Operation cancelled"));
			else
				camel_exception_set (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
						     g_strerror (errno));
			camel_service_disconnect (CAMEL_SERVICE (store), FALSE, NULL);
			g_string_free (str, TRUE);
			goto lose;
		}
		if (nread < length) {
			camel_exception_set (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
					     _("Server response ended too soon."));
			camel_service_disconnect (CAMEL_SERVICE (store), FALSE, NULL);
			g_string_free (str, TRUE);
			goto lose;
		}
		str->str[length + 1] = '\0';
		
		/* Fix up the literal, turning CRLFs into LF. Also, if
		 * we find any embedded NULs, strip them. This is
		 * dubious, but:
		 *   - The IMAP grammar says you can't have NULs here
		 *     anyway, so this will not affect our behavior
		 *     against any completely correct server.
		 *   - WU-imapd 12.264 (at least) will cheerily pass
		 *     NULs along if they are embedded in the message
		 */
		
		s = d = str->str + 1;
		end = str->str + 1 + length;
		while (s < end) {
			while (s < end && *s == '\0') {
				s++;
				length--;
			}
			if (*s == '\r' && *(s + 1) == '\n') {
				s++;
				length--;
			}
			*d++ = *s++;
		}
		*d = '\0';
		str->len = length + 1;
		
		/* p points to the "{" in the line that starts the
		 * literal. The length of the CR-less response must be
		 * less than or equal to the length of the response
		 * with CRs, therefore overwriting the old value with
		 * the new value cannot cause an overrun. However, we
		 * don't want it to be shorter either, because then the
		 * GString's length would be off...
		 */
		sprintf (p, "{%0*d}", ldigits, length);
		
		fulllen += str->len;
		g_ptr_array_add (data, str);
		
		/* Read the next line. */
		if (camel_imap_store_readline (store, &line, ex) < 0)
			goto lose;
	}
	
	/* Now reassemble the data. */
	p = line = g_malloc (fulllen + 1);
	for (i = 0; i < data->len; i++) {
		str = data->pdata[i];
		memcpy (p, str->str, str->len);
		p += str->len;
		g_string_free (str, TRUE);
	}
	*p = '\0';
	g_ptr_array_free (data, TRUE);
	return line;
	
 lose:
	for (i = 0; i < data->len; i++)
		g_string_free (data->pdata[i], TRUE);
	g_ptr_array_free (data, TRUE);
	return NULL;
}


/**
 * camel_imap_response_free:
 * @store: the CamelImapStore the response is from
 * @response: a CamelImapResponse
 *
 * Frees all of the data in @response and processes any untagged
 * EXPUNGE and EXISTS responses in it. Releases @store's connect_lock.
 **/
void
camel_imap_response_free (CamelImapStore *store, CamelImapResponse *response)
{
	int i, number, exists = 0;
	GArray *expunged = NULL;
	char *resp, *p;
	
	if (!response)
		return;
	
	for (i = 0; i < response->untagged->len; i++) {
		resp = response->untagged->pdata[i];
		
		if (response->folder) {
			/* Check if it's something we need to handle. */
			number = strtoul (resp + 2, &p, 10);
			if (!g_ascii_strcasecmp (p, " EXISTS")) {
				exists = number;
			} else if (!strcasecmp (p, " EXPUNGE")) {
				if (!expunged) {
					expunged = g_array_new (FALSE, FALSE,
								sizeof (int));
				}
				g_array_append_val (expunged, number);
			}
		}
		g_free (resp);
	}
	
	g_ptr_array_free (response->untagged, TRUE);
	g_free (response->status);
	
	if (response->folder) {
		if (exists > 0 || expunged) {
			/* Update the summary */
			camel_imap_folder_changed (response->folder,
						   exists, expunged, NULL);
			if (expunged)
				g_array_free (expunged, TRUE);
		}
		
		camel_object_unref (CAMEL_OBJECT (response->folder));
	}
	
	g_free (response);
	CAMEL_SERVICE_UNLOCK (store, connect_lock);
}

/**
 * camel_imap_response_free_without_processing:
 * @store: the CamelImapStore the response is from.
 * @response: a CamelImapResponse:
 *
 * Frees all of the data in @response without processing any untagged
 * responses. Releases @store's command lock.
 **/
void
camel_imap_response_free_without_processing (CamelImapStore *store,
					     CamelImapResponse *response)
{
	if (!response)
		return;
	
	if (response->folder) {
		camel_object_unref (CAMEL_OBJECT (response->folder));
		response->folder = NULL;
	}
	camel_imap_response_free (store, response);
}

/**
 * camel_imap_response_extract:
 * @store: the store the response came from
 * @response: the response data returned from camel_imap_command
 * @type: the response type to extract
 * @ex: a CamelException
 *
 * This checks that @response contains a single untagged response of
 * type @type and returns just that response data. If @response
 * doesn't contain the right information, the function will set @ex
 * and return %NULL. Either way, @response will be freed and the
 * store's connect_lock released.
 *
 * Return value: the desired response string, which the caller must free.
 **/
char *
camel_imap_response_extract (CamelImapStore *store,
			     CamelImapResponse *response,
			     const char *type,
			     CamelException *ex)
{
	int len = strlen (type), i;
	char *resp;
	
	len = strlen (type);
	
	for (i = 0; i < response->untagged->len; i++) {
		resp = response->untagged->pdata[i];
		/* Skip "* ", and initial sequence number, if present */
		strtoul (resp + 2, &resp, 10);
		if (*resp == ' ')
			resp = (char *) imap_next_word (resp);
		
		if (!strncasecmp (resp, type, len))
			break;
	}
	
	if (i < response->untagged->len) {
		resp = response->untagged->pdata[i];
		g_ptr_array_remove_index (response->untagged, i);
	} else {
		resp = NULL;
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      _("IMAP server response did not contain "
					"%s information"), type);
	}
	
	camel_imap_response_free (store, response);
	return resp;
}

/**
 * camel_imap_response_extract_continuation:
 * @store: the store the response came from
 * @response: the response data returned from camel_imap_command
 * @ex: a CamelException
 *
 * This checks that @response contains a continuation response, and
 * returns just that data. If @response doesn't contain a continuation
 * response, the function will set @ex, release @store's connect_lock,
 * and return %NULL. Either way, @response will be freed.
 *
 * Return value: the desired response string, which the caller must free.
 **/
char *
camel_imap_response_extract_continuation (CamelImapStore *store,
					  CamelImapResponse *response,
					  CamelException *ex)
{
	char *status;
	
	if (response->status && *response->status == '+') {
		status = response->status;
		response->status = NULL;
		camel_imap_response_free (store, response);
		return status;
	}
	
	camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
			      _("Unexpected OK response from IMAP server: %s"),
			      response->status);
	camel_imap_response_free (store, response);
	return NULL;
}

static char *
imap_command_strdup_vprintf (CamelImapStore *store, const char *fmt,
			     va_list ap)
{
	GPtrArray *args;
	const char *p, *start;
	char *out, *outptr, *string;
	int num, len, i, arglen;

	args = g_ptr_array_new ();
	
	/* Determine the length of the data */
	len = strlen (fmt);
	p = start = fmt;
	while (*p) {
		p = strchr (start, '%');
		if (!p)
			break;
		
		switch (*++p) {
		case 'd':
			num = va_arg (ap, int);
			g_ptr_array_add (args, GINT_TO_POINTER (num));
			start = p + 1;
			len += 10;
			break;
		case 's':
			string = va_arg (ap, char *);
			g_ptr_array_add (args, string);
			start = p + 1;
			len += strlen (string);
			break;
		case 'S':
		case 'F':
			string = va_arg (ap, char *);
			if (*p == 'F') {
				/* NB: this is freed during output */
				char *s = camel_imap_store_summary_full_from_path(store->summary, string);
				string = s?s:camel_utf8_utf7(string);
			}
			arglen = strlen (string);
			g_ptr_array_add (args, string);
			if (imap_is_atom (string)) {
				len += arglen;
			} else {
				if (store->capabilities & IMAP_CAPABILITY_LITERALPLUS)
					len += arglen + 15;
				else
					len += arglen * 2;
			}
			start = p + 1;
			break;
		case '%':
			start = p;
			break;
		default:
			g_warning ("camel-imap-command is not printf. I don't "
				   "know what '%%%c' means.", *p);
			start = *p ? p + 1 : p;
			break;
		}
	}
	
	/* Now write out the string */
	outptr = out = g_malloc (len + 1);
	p = start = fmt;
	i = 0;
	while (*p) {
		p = strchr (start, '%');
		if (!p) {
			strcpy (outptr, start);
			break;
		} else {
			strncpy (outptr, start, p - start);
			outptr += p - start;
		}
		
		switch (*++p) {
		case 'd':
			num = GPOINTER_TO_INT (args->pdata[i++]);
			outptr += sprintf (outptr, "%d", num);
			break;
			
		case 's':
			string = args->pdata[i++];
			outptr += sprintf (outptr, "%s", string);
			break;
		case 'S':
		case 'F':
			string = args->pdata[i++];
			if (imap_is_atom (string)) {
				outptr += sprintf (outptr, "%s", string);
			} else {
				if (store->capabilities & IMAP_CAPABILITY_LITERALPLUS) {
					outptr += sprintf (outptr, "{%d+}\r\n%s", strlen (string), string);
				} else {
					char *quoted = imap_quote_string (string);
					
					outptr += sprintf (outptr, "%s", quoted);
					g_free (quoted);
				}
			}
			
			if (*p == 'F')
				g_free (string);
			break;
		default:
			*outptr++ = '%';
			*outptr++ = *p;
		}
		
		start = *p ? p + 1 : p;
	}
	
	return out;
}

static char *
imap_command_strdup_printf (CamelImapStore *store, const char *fmt, ...)
{
	va_list ap;
	char *result;
	
	va_start (ap, fmt);
	result = imap_command_strdup_vprintf (store, fmt, ap);
	va_end (ap);
	
	return result;
}
