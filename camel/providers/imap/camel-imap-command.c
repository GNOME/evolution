/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-imap-command.c: IMAP command sending/parsing routines */

/*
 *  Authors:
 *    Dan Winship <danw@helixcode.com>
 *    Jeffrey Stedfast <fejj@helixcode.com>
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
#include <string.h>

#include "camel-imap-command.h"
#include "camel-imap-utils.h"
#include "camel-imap-folder.h"
#include <camel/camel-exception.h>

static char *imap_read_untagged (CamelImapStore *store, char *line,
				 CamelException *ex);
static CamelImapResponse *imap_read_response (CamelImapStore *store,
					      CamelException *ex);

/**
 * camel_imap_command: Send a command to a IMAP server and get a response
 * @store: the IMAP store
 * @folder: The folder to perform the operation in (or %NULL if not
 * relevant).
 * @ex: a CamelException
 * @fmt: a printf-style format string, followed by arguments
 *
 * This function makes sure that @folder (if non-%NULL) is the
 * currently-selected folder on @store and then sends the IMAP command
 * specified by @fmt and the following arguments. It then reads the
 * server's response(s) and parses the final result.
 *
 * As a special case, if @fmt is %NULL, it will just select @folder
 * and return the response from doing so.
 * 
 * Return value: %NULL if an error occurred (in which case @ex will
 * be set). Otherwise, a CamelImapResponse describing the server's
 * response, which the caller must free with camel_imap_response_free().
 **/
CamelImapResponse *
camel_imap_command (CamelImapStore *store, CamelFolder *folder,
		    CamelException *ex, const char *fmt, ...)
{
	gchar *cmdbuf;
	va_list ap;

	/* Check for current folder */
	if (folder && (!fmt || folder != store->current_folder)) {
		CamelImapResponse *response;

		store->current_folder = NULL;
		response = camel_imap_command (store, NULL, ex,
					       "SELECT \"%s\"",
					       folder->full_name);
		if (!response)
			return NULL;
		store->current_folder = folder;

		if (!fmt)
			return response;

		camel_imap_response_free (response);
	}

	/* Send the command */
	va_start (ap, fmt);
	cmdbuf = g_strdup_vprintf (fmt, ap);
	va_end (ap);

	camel_remote_store_send_string (CAMEL_REMOTE_STORE (store), ex,
					"A%.5d %s\r\n", store->command++,
					cmdbuf);
	g_free (cmdbuf);
	if (camel_exception_is_set (ex))
		return NULL;

	/* Read the response. */
	return imap_read_response (store, ex);
}

/**
 * camel_imap_command_continuation: Send more command data to the IMAP server
 * @store: the IMAP store
 * @ex: a CamelException
 * @cmdbuf: buffer containing the response/request data
 *
 * This method is for sending continuing responses to the IMAP server
 * after camel_imap_command returns a CAMEL_IMAP_PLUS response.
 * 
 * Return value: as for camel_imap_command()
 **/
CamelImapResponse *
camel_imap_command_continuation (CamelImapStore *store, CamelException *ex,
				 const char *cmdbuf)
{
	if (camel_remote_store_send_string (CAMEL_REMOTE_STORE (store), ex,
					    "%s\r\n", cmdbuf) < 0)
		return NULL;

	return imap_read_response (store, ex);
}

/* Read the response to an IMAP command. */
static CamelImapResponse *
imap_read_response (CamelImapStore *store, CamelException *ex)
{
	CamelImapResponse *response;
	int number, exists = 0;
	GArray *expunged = NULL;
	char *respbuf, *retcode, *word, *p;

	/* Read first line */
	if (camel_remote_store_recv_line (CAMEL_REMOTE_STORE (store),
					  &respbuf, ex) < 0)
		return NULL;

	response = g_new0 (CamelImapResponse, 1);
	response->untagged = g_ptr_array_new ();

	/* Check for untagged data */
	while (!strncmp (respbuf, "* ", 2)) {
		/* Read the rest of the response if it is multi-line. */
		respbuf = imap_read_untagged (store, respbuf, ex);
		if (camel_exception_is_set (ex))
			break;

		/* If it starts with a number, we might deal with
		 * it ourselves.
		 */
		word = imap_next_word (respbuf);
		number = strtoul (word, &p, 10);
		if (p != word && store->current_folder) {
			word = imap_next_word (p);
			if (!g_strcasecmp (word, "EXISTS")) {
				exists = number;
				g_free (respbuf);
				goto next;
			} else if (!g_strcasecmp (word, "EXPUNGE")) {
				if (!expunged) {
					expunged = g_array_new (FALSE, FALSE,
								sizeof (int));
				}
				g_array_append_val (expunged, number);
				g_free (respbuf);
				goto next;
			}
		} else {
			if (!g_strncasecmp (word, "BYE", 3)) {
				/* connection was lost, no more data to fetch */
				store->connected = FALSE;
				g_free (respbuf);
				respbuf = NULL;
				break;
			}
		}

		g_ptr_array_add (response->untagged, respbuf);
	next:
		if (camel_remote_store_recv_line (
			CAMEL_REMOTE_STORE (store), &respbuf, ex) < 0)
			break;
	}

	/* Update the summary */
	if (store->current_folder && (exists > 0 || expunged)) {
		camel_imap_folder_changed (store->current_folder, exists,
					   expunged, NULL);
	}
	if (expunged)
		g_array_free (expunged, TRUE);

	if (!respbuf || camel_exception_is_set (ex)) {
		camel_imap_response_free (response);
		return NULL;
	}

	response->status = respbuf;

	/* Check for OK or continuation response. */
	if (!strncmp (respbuf, "+ ", 2))
		return response;
	retcode = imap_next_word (respbuf);
	if (!strncmp (retcode, "OK", 2))
		return response;

	/* We should never get BAD, or anything else but +, OK, or NO
	 * for that matter.
	 */
	if (strncmp (retcode, "NO", 2) != 0) {
		g_warning ("Unexpected response from IMAP server: %s",
			   respbuf);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      _("Unexpected response from IMAP "
					"server: %s"), respbuf);
		camel_imap_response_free (response);
		return NULL;
	}

	retcode = imap_next_word (retcode);
	camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
			      _("IMAP command failed: %s"),
			      retcode ? retcode : _("Unknown error"));
	camel_imap_response_free (response);
	return NULL;
}

/* Given a line that is the start of an untagged response, read and
 * return the complete response, which may include an arbitrary number
 * of literals.
 */
static char *
imap_read_untagged (CamelImapStore *store, char *line, CamelException *ex)
{
	int fulllen, length, ldigits, nread, i;
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
		if (*end != '}' || *(end + 1) || end == p + 1)
			break;
		ldigits = end - (p + 1);

		/* Read the literal */
		str = g_string_sized_new (length + 2);
		str->str[0] = '\n';
		nread = camel_stream_read (CAMEL_REMOTE_STORE (store)->istream,
					   str->str + 1, length);
		if (nread < length) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
					      _("Server response ended too soon."));
			camel_service_disconnect (CAMEL_SERVICE (store),
						  FALSE, NULL);
			goto lose;
		}
		str->str[length] = '\0';

		/* Fix up the literal, turning CRLFs into LF. Also, if
		 * we find any embedded NULs, strip them. This is
		 * dubious, but:
		 *   - The IMAP grammar says you can't have NULs here
		 *     anyway, so this will not affect our behavior
		 *     against any completely correct server.
		 *   - WU-imapd 12.264 (at least) will cheerily pass
		 *     NULs along if they are embedded in the message
		 *   - The only cause of embedded NULs we've seen is an
		 *     Evolution base64-encoder bug that sometimes
		 *     inserts a NUL into the last line when it
		 *     shouldn't.
		 */

		s = d = str->str + 1;
		end = str->str + 1 + length;
		while (s < end) {
			while (*s == '\0' && s < end) {
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
		sprintf (p, "{%0*d}", ldigits, str->len);

		fulllen += str->len;
		g_ptr_array_add (data, str);

		/* Read the next line. */
		if (camel_remote_store_recv_line (CAMEL_REMOTE_STORE (store),
						  &line, ex) < 0)
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
 * response: a CamelImapResponse:
 *
 * Frees all of the data in @response.
 **/
void
camel_imap_response_free (CamelImapResponse *response)
{
	int i;

	if (!response)
		return;
	for (i = 0; i < response->untagged->len; i++)
		g_free (response->untagged->pdata[i]);
	g_ptr_array_free (response->untagged, TRUE);
	g_free (response->status);
	g_free (response);
}

/**
 * camel_imap_response_extract:
 * @response: the response data returned from camel_imap_command
 * @type: the response type to extract
 * @ex: a CamelException
 *
 * This checks that @response contains a single untagged response of
 * type @type and returns just that response data. If @response
 * doesn't contain the right information, the function will set @ex and
 * return %NULL. Either way, @response will be freed.
 *
 * Return value: the desired response string, which the caller must free.
 **/
char *
camel_imap_response_extract (CamelImapResponse *response, const char *type,
			     CamelException *ex)
{
	int len = strlen (type), i;
	char *resp;

	for (i = 0; i < response->untagged->len; i++) {
		resp = response->untagged->pdata[i];
		/* Skip "* ", and initial sequence number, if present */
		strtoul (resp + 2, &resp, 10);
		if (*resp == ' ')
			resp = imap_next_word (resp);

		if (!g_strncasecmp (resp, type, len))
			break;
		
		g_free (response->untagged->pdata[i]);
	}

	if (i < response->untagged->len) {
		resp = response->untagged->pdata[i];
		for (i++; i < response->untagged->len; i++)
			g_free (response->untagged->pdata[i]);
	} else {
		resp = NULL;
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      _("IMAP server response did not contain "
					"%s information"), type);
	}

	g_ptr_array_free (response->untagged, TRUE);
	g_free (response->status);
	g_free (response);
	return resp;
}

/**
 * camel_imap_response_extract_continuation:
 * @response: the response data returned from camel_imap_command
 * @ex: a CamelException
 *
 * This checks that @response contains a continuation response, and
 * returns just that data. If @response doesn't contain a continuation
 * response, the function will set @ex and return %NULL. Either way,
 * @response will be freed.
 *
 * Return value: the desired response string, which the caller must free.
 **/
char *
camel_imap_response_extract_continuation (CamelImapResponse *response,
					  CamelException *ex)
{
	char *status;

	if (response->status && !strncmp (response->status, "+ ", 2)) {
		status = response->status;
		response->status = NULL;
		camel_imap_response_free (response);
		return status;
	}

	camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
			      _("Unexpected OK response from IMAP server: %s"),
			      response->status);
	camel_imap_response_free (response);
	return NULL;
}
