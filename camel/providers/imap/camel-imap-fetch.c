/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Dan Winship <danw@helixcode.com>
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

#include "config.h"

#include <ctype.h>
#include <string.h>
#include "camel-imap-command.h"
#include "camel-imap-folder.h"
#include "camel-imap-summary.h"
#include "camel-imap-utils.h"
#include "camel-imap-private.h"

#include "camel-internet-address.h"
#include "camel-mime-message.h"
#include "camel-mime-utils.h"
#include "camel-stream-mem.h"

static const char *imap_protocol_get_summary_specifier (CamelImapStore *store);

static CamelMessageInfo *parse_headers (char **headers_p);
static CamelMessageContentInfo *parse_body (CamelFolderSummary *summary,
					    char **body_p,
					    const char *part_specifier);

static void skip_astring (char **str_p);

/**
 * imap_add_to_summary:
 * @folder: the (IMAP) folder
 * @first: the sequence number of the first message to add
 * @last: the sequence number of the last message to add
 * @changes: a CamelFolderChangeInfo structure to update
 * @ex: a CamelException
 *
 * This fetches information about the messages in the indicated range
 * and updates the folder's summary information. As a side effect, it may
 * also cache partial messages in the folder message cache.
 **/
void
imap_add_to_summary (CamelFolder *folder, int first, int last,
		     CamelFolderChangeInfo *changes, CamelException *ex)
{
	CamelImapStore *store = CAMEL_IMAP_STORE (folder->parent_store);
	CamelImapResponse *response;
	GPtrArray *headers, *messages;
	const char *summary_specifier;
	char *p, *uid;
	int i, seq;
	CamelMessageInfo *mi;
	CamelMessageContentInfo *content;
	guint32 flags, size;

	summary_specifier = imap_protocol_get_summary_specifier (store);
	CAMEL_IMAP_STORE_LOCK(store, command_lock);
	if (first == last) {
		response = camel_imap_command (store, folder, ex,
					       "FETCH %d (%s)", first,
					       summary_specifier);
	} else {
		response = camel_imap_command (store, folder, ex,
					       "FETCH %d:%d (%s)", first,
					       last, summary_specifier);
	}
	CAMEL_IMAP_STORE_UNLOCK(store, command_lock);
	if (!response)
		return;

	messages = g_ptr_array_new ();
	g_ptr_array_set_size (messages, last - first + 1);
	headers = response->untagged;
	for (i = 0; i < headers->len; i++) {
		p = headers->pdata[i];
		if (!g_strncasecmp (p, "* fetch ", 8))
			continue;
		seq = strtoul (p + 8, &p, 10);
		if (!seq || seq < camel_folder_summary_count (folder->summary))
			continue;

		mi = messages->pdata[seq - first];
		flags = size = 0;
		content = NULL;
		uid = NULL;
		while (p && *p != ')') {
			if (*p == ' ')
				p++;
			if (!g_strncasecmp (p, "flags ", 6)) {
				p += 6;
				/* FIXME user flags */
				flags = imap_parse_flag_list (&p);
			} else if (!g_strncasecmp (p, "size ", 5)) {
				size = strtoul (p + 5, &p, 10);
			} else if (!g_strncasecmp (p, "uid ", 4)) {
				uid = p + 4;
				strtoul (uid, &p, 10);
				uid = g_strndup (uid, p - uid);
			} else if (!g_strncasecmp (p, "body ", 5)) {
				p += 5;
				content = parse_body (folder->summary, &p, "");
			} else if (!g_strncasecmp (p, "body[header] ", 13) ||
				   !g_strncasecmp (p, "rfc822.header ", 14)) {
				p = strchr (p + 13, ' ');
				mi = parse_headers (&p);
			} else {
				g_warning ("Waiter, I did not order this %.*s",
					   (int)strcspn (p, " \n"), p);
				p = NULL;
			}
		}

		/* Ideally we got everything on one line, but if we
		 * we didn't, and we didn't get the body yet, then we
		 * have to postpone this line for later.
		 */
		if (mi == NULL) {
			p = headers->pdata[i];
			g_ptr_array_remove_index (headers, i);
			g_ptr_array_add (headers, p);
			continue;
		}

		messages->pdata[seq - first] = mi;
		if (uid)
			camel_message_info_set_uid (mi, uid);
		if (flags)
			mi->flags = flags;
		if (content)
			mi->content = content;
		if (size)
			mi->size = size;
	}
	camel_imap_response_free (response);

	for (i = 0; i < messages->len; i++) {
		mi = messages->pdata[i];
		camel_folder_summary_add (folder->summary, mi);
	}
	g_ptr_array_free (messages, TRUE);
}

static const char *
imap_protocol_get_summary_specifier (CamelImapStore *store)
{
	if (store->server_level >= IMAP_LEVEL_IMAP4REV1)
		return "UID FLAGS RFC822.SIZE BODY BODY.PEEK[HEADER]";
	else
		return "UID FLAGS RFC822.SIZE BODY RFC822.HEADER";
}

/**
 * skip_char:
 * @str_p: a pointer to a string
 * @ch: the character to skip
 *
 * Skip the specified character, or fail. Updates the position of
 * *@str_p on success, sets it to %NULL on failure.
 **/
static inline void
skip_char (char **str_p, char ch)
{
	if (*str_p && **str_p == ch)
		*str_p = *str_p + 1;
	else
		*str_p = NULL;
}

/**
 * skip_astring:
 * @str_p: a pointer to a string
 *
 * Skip an astring, or fail. Updates the position of *@str_p on
 * success, sets it to %NULL on failure.
 **/
static void
skip_astring (char **str_p)
{
	char *str = *str_p;

	if (!str)
		return;
	else if (*str == '"') {
		while (*++str && *str != '"') {
			if (*str == '\\') {
				str++;
				if (!*str)
					break;
			}
		}
		if (*str == '"')
			*str_p = str + 1;
		else
			*str_p = NULL;
	} else if (*str == '{') {
		unsigned long len;

		len = strtoul (str + 1, &str, 10);
		if (*str != '}' || *(str + 1) != '\n' ||
		    strlen (str + 2) < len) {
			*str_p = NULL;
			return;
		}
		*str_p = str + 2 + len;
	} else {
		/* We assume the string is well-formed and don't
		 * bother making sure it's a valid atom.
		 */
		while (*str && *str != ')' && *str != ' ')
			str++;
		*str_p = str;
	}
}

/**
 * skip_list:
 * @str_p: a pointer to the open parenthesis of a list
 *
 * Skips over a list of astrings and lists. Updates the position of
 * *@str_p on success, sets it to %NULL on failure.
 **/
void
skip_list (char **str_p)
{
	skip_char (str_p, '(');
	while (*str_p && **str_p != ')') {
		if (**str_p == '(')
			skip_list (str_p);
		else
			skip_astring (str_p);
		if (*str_p && **str_p == ' ')
			skip_char (str_p, ' ');
	}
	skip_char (str_p, ')');
}

/**
 * parse_params:
 * @parms_p: a pointer to the start of an IMAP "body_fld_param".
 * @ct: a content-type structure
 *
 * This parses the body_fld_param and sets parameters on @ct
 * appropriately.
 *
 * On a successful return, *@params_p will be set to point to the
 * character after the last character of the body_fld_param. On
 * failure, it will be set to %NULL.
 **/
static void
parse_params (char **parms_p, struct _header_content_type *ct)
{
	char *parms = *parms_p, *name, *value;
	int len;

	if (!g_strncasecmp (parms, "nil", 3)) {
		*parms_p += 3;
		return;
	}

	if (*parms++ != '(') {
		*parms_p = NULL;
		return;
	}

	while (*parms == '(') {
		parms++;

		name = imap_parse_nstring (&parms, &len);
		value = imap_parse_nstring (&parms, &len);

		if (name && value)
			header_content_type_set_param (ct, name, value);
		g_free (name);
		g_free (value);

		if (!parms)
			break;
		if (*parms++ != ')') {
			parms = NULL;
			break;
		}
	}

	if (!parms || *parms++ != ')') {
		*parms_p = NULL;
		return;
	}
	*parms_p = parms;
}

static CamelMessageContentInfo *
parse_body (CamelFolderSummary *summary, char **body_p,
	    const char *part_specifier)
{
	char *body = *body_p;
	CamelMessageContentInfo *ci;
	CamelImapMessageContentInfo *ici;
	char *child_specifier;
	int speclen, len;

	if (*body++ != '(') {
		*body_p = NULL;
		return NULL;
	}

	ci = camel_folder_summary_content_info_new (summary);
	ici = (CamelImapMessageContentInfo *)ci;
	ici->part_specifier = g_strdup (part_specifier);

	if (*body == '(') {
		/* multipart */
		GPtrArray *children;
		CamelMessageContentInfo *child;
		char *subtype;
		int i;

		speclen = strlen (part_specifier);
		child_specifier = g_malloc (speclen + 10);
		memcpy (child_specifier, part_specifier, speclen);
		child_specifier[speclen] = '.';

		/* Parse the child body parts */
		children = g_ptr_array_new ();
		i = 0;
		while (body && *body == '(') {
			sprintf (child_specifier + speclen + 1, "%d", i++);
			child = parse_body (summary, &body, child_specifier);
			if (!child)
				break;
			child->parent = ci;
			g_ptr_array_add (children, child);
		}
		g_free (child_specifier);
		skip_char (&body, ' ');

		/* If there is a parse error, or there are no children,
		 * abort.
		 */
		if (!body || !children->len) {
			for (i = 0; i < children->len; i++) {
				child = children->pdata[i];
				camel_folder_summary_content_info_free (summary, child);
			}
			g_ptr_array_free (children, TRUE);
			camel_folder_summary_content_info_free (summary, ci);
			*body_p = NULL;
			return NULL;
		}

		/* Chain the children. */
		ci->childs = children->pdata[0];
		for (i = 0; i < children->len - 1; i++) {
			child = children->pdata[i];
			child->next = children->pdata[i + 1];
		}
		g_ptr_array_free (children, TRUE);

		/* Parse the multipart subtype */
		subtype = imap_parse_string (&body, &len);
		ci->type = header_content_type_new ("multipart", subtype);
		g_free (subtype);
	} else {
		/* single part */
		char *type, *subtype;

		type = imap_parse_string (&body, &len);
		skip_char (&body, ' ');
		subtype = imap_parse_string (&body, &len);
		skip_char (&body, ' ');
		if (!body) {
			camel_folder_summary_content_info_free (summary, ci);
			*body_p = NULL;
			return NULL;
		}
		ci->type = header_content_type_new (type, subtype);
		parse_params (&body, ci->type);
		skip_char (&body, ' ');

		ci->id = imap_parse_string (&body, &len);
		skip_char (&body, ' ');
		ci->description = imap_parse_string (&body, &len);
		skip_char (&body, ' ');
		ci->encoding = imap_parse_string (&body, &len);
		skip_char (&body, ' ');
		ci->size = strtoul (body, &body, 10);

		if (header_content_type_is (ci->type, "message", "rfc822")) {
			skip_char (&body, ' ');
			ici->message_info = camel_folder_summary_info_new (summary);
			skip_list (&body); /* envelope */
			skip_char (&body, ' ');
			ci->childs = parse_body (summary, &body,
						 part_specifier);
			skip_char (&body, ' ');
			strtoul (body, &body, 10);
		} else if (header_content_type_is (ci->type, "text", "*"))
			strtoul (body, &body, 10);

		g_free (type);
		g_free (subtype);
	}

	if (!body || *body++ != ')') {
		*body_p = NULL;
		camel_folder_summary_content_info_free (summary, ci);
		return NULL;
	}

	*body_p = body;
	return ci;
}

void
parse_bodypart (char **body_p, CamelFolder *folder, CamelMessageInfo *mi)
{
	CamelMessageContentInfo *ci;
	char *body = *body_p;
	int num;

	/* Skip "body[" */
	body += 5;
	ci = mi->content;

	/* Parse the 'nz_number *["." nz_number]' prefix. This is fun,
	 * because the numbers mean different things depending on where
	 * you are. See RFC 2060 for details.
	 */
	while (isdigit((unsigned char)*body)) {
		num = strtoul (body, &body, 10);
		if (num == 0 || (*body != '.' && *body != ']')) {
			*body = NULL;
			return;
		}

		if (header_content_type_is (ci->type, "multipart", "*")) {
			ci = ci->childs;
			while (ci && --num)
				ci = ci->next;
			if (!ci) {
				*body = NULL;
				return;
			}
		} else if (num != 1) {
			*body = NULL;
			return;
		}

		if (*body == ']')
			break;

		if (isdigit ((unsigned char)*++body) &&
		    header_content_type_is (ci->type, "message", "rfc822")) {
			mi = ((CamelImapMessageContentInfo *)ci)->message_info;
			ci = mi->content;
		}
	}

	if (!g_strncasecmp (body, "header] ", 8)) {
		char *headers;
		int len;
		CamelMimeMessage *msg;
		CamelStream *stream;

		body += 9;
		mi = parse_headers (&body, folder);
		/* XXX */
	} else if (!g_strncasecmp (body, "] ", 2)) {
		CamelImapMessageContentInfo *ici = (CamelImapMessageContentInfo *)ci;
		body += 2;
		/* XXX */
	} else
		*body = NULL;
}

CamelMessageInfo *
imap_parse_headers (char **headers_p, CamelFolder *folder)
{
	CamelMimeMessage *msg;
	CamelStream *stream;
	char *headers;
	int len;

	headers = imap_parse_nstring (headers_p, &len);
	msg = camel_mime_message_new ();
	stream = camel_stream_mem_new_with_buffer (headers, len);
	g_free (headers);
	camel_data_wrapper_construct_from_stream (CAMEL_DATA_WRAPPER (msg), stream);
	camel_object_unref (CAMEL_OBJECT (stream));

	mi = camel_folder_summary_info_new_from_message (folder->summary, msg);
	camel_imap_folder_cache_message (folder, mi, msg);
}
