/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  Camel
 *  Copyright (C) 1999-2004 Jeffrey Stedfast
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
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include <camel/camel-store.h>
#include <camel/camel-i18n.h>

#include "camel-imap4-engine.h"
#include "camel-imap4-stream.h"
#include "camel-imap4-command.h"

#include "camel-imap4-utils.h"

#define d(x) x


void
camel_imap4_flags_diff (flags_diff_t *diff, guint32 old, guint32 new)
{
	diff->changed = old ^ new;
	diff->bits = new & diff->changed;
}


guint32
camel_imap4_flags_merge (flags_diff_t *diff, guint32 flags)
{
	return (flags & ~diff->changed) | diff->bits;
}


/**
 * camel_imap4_merge_flags:
 * @original: original server flags
 * @local: local flags (after changes)
 * @server: new server flags (another client updated the server flags)
 *
 * Merge the local flag changes into the new server flags.
 *
 * Returns the merged flags.
 **/
guint32
camel_imap4_merge_flags (guint32 original, guint32 local, guint32 server)
{
	flags_diff_t diff;
	
	camel_imap4_flags_diff (&diff, original, local);
	
	return camel_imap4_flags_merge (&diff, server);
}


struct _uidset_range {
	struct _uidset_range *next;
	guint32 first, last;
	uint8_t buflen;
	char buf[24];
};

struct _uidset {
	CamelFolderSummary *summary;
	struct _uidset_range *ranges;
	struct _uidset_range *tail;
	size_t maxlen, setlen;
};

static void
uidset_range_free (struct _uidset_range *range)
{
	struct _uidset_range *next;
	
	while (range != NULL) {
		next = range->next;
		g_free (range);
		range = next;
	}
}

static void
uidset_init (struct _uidset *uidset, CamelFolderSummary *summary, size_t maxlen)
{
	uidset->ranges = g_new (struct _uidset_range, 1);
	uidset->ranges->first = (guint32) -1;
	uidset->ranges->last = (guint32) -1;
	uidset->ranges->next = NULL;
	uidset->ranges->buflen = 0;
	
	uidset->tail = uidset->ranges;
	uidset->summary = summary;
	uidset->maxlen = maxlen;
	uidset->setlen = 0;
}

/* returns: -1 on full-and-not-added, 0 on added-and-not-full or 1 on added-and-full */
static int
uidset_add (struct _uidset *uidset, CamelMessageInfo *info)
{
	GPtrArray *messages = uidset->summary->messages;
	struct _uidset_range *node, *tail = uidset->tail;
	const char *iuid = camel_message_info_uid (info);
	size_t uidlen, len;
	const char *colon;
	guint32 index;
	
	/* Note: depends on integer overflow for initial 'add' */
	for (index = tail->last + 1; index < messages->len; index++) {
		if (info == messages->pdata[index])
			break;
	}
	
	g_assert (index < messages->len);
	
	uidlen = strlen (iuid);
	
	if (tail->buflen == 0) {
		/* first add */
		tail->first = tail->last = index;
		strcpy (tail->buf, iuid);
		uidset->setlen = uidlen;
		tail->buflen = uidlen;
	} else if (index == (tail->last + 1)) {
		/* add to last range */
		if (tail->last == tail->first) {
			/* make sure we've got enough room to add this one... */
			if ((uidset->setlen + uidlen + 1) > uidset->maxlen)
				return -1;
			
			tail->buf[tail->buflen++] = ':';
			uidset->setlen++;
		} else {
			colon = strchr (tail->buf, ':') + 1;
			
			len = strlen (colon);
			uidset->setlen -= len;
			tail->buflen -= len;
		}
		
		strcpy (tail->buf + tail->buflen, iuid);
		uidset->setlen += uidlen;
		tail->buflen += uidlen;
		
		tail->last = index;
	} else if ((uidset->setlen + uidlen + 1) < uidset->maxlen) {
		/* the beginning of a new range */
		tail->next = node = g_new (struct _uidset_range, 1);
		node->first = node->last = index;
		strcpy (node->buf, iuid);
		uidset->setlen += uidlen + 1;
		node->buflen = uidlen;
		uidset->tail = node;
		node->next = NULL;
	} else {
		/* can't add this one... */
		return -1;
	}
	
	fprintf (stderr, "added uid %s to uidset (summary index = %u)\n", iuid, index);
	
	if (uidset->setlen < uidset->maxlen)
		return 0;
	
	return 1;
}

static char *
uidset_to_string (struct _uidset *uidset)
{
	struct _uidset_range *range;
	GString *string;
	char *str;
	
	string = g_string_new ("");
	
	range = uidset->ranges;
	while (range != NULL) {
		g_string_append (string, range->buf);
		range = range->next;
		if (range)
			g_string_append_c (string, ',');
	}
	
	str = string->str;
	g_string_free (string, FALSE);
	
	return str;
}

int
camel_imap4_get_uid_set (CamelIMAP4Engine *engine, CamelFolderSummary *summary, GPtrArray *infos, int cur, size_t linelen, char **set)
{
	struct _uidset uidset;
	size_t maxlen;
	int rv = 0;
	int i;
	
	if (engine->maxlentype == CAMEL_IMAP4_ENGINE_MAXLEN_LINE)
		maxlen = engine->maxlen - linelen;
	else
		maxlen = engine->maxlen;
	
	uidset_init (&uidset, summary, maxlen);
	
	for (i = cur; i < infos->len && rv != 1; i++) {
		if ((rv = uidset_add (&uidset, infos->pdata[i])) == -1)
			break;
	}
	
	if (i > cur)
		*set = uidset_to_string (&uidset);
	
	uidset_range_free (uidset.ranges);
	
	return (i - cur);
}


void
camel_imap4_utils_set_unexpected_token_error (CamelException *ex, CamelIMAP4Engine *engine, camel_imap4_token_t *token)
{
	GString *errmsg;
	
	if (ex == NULL)
		return;
	
	errmsg = g_string_new ("");
	g_string_append_printf (errmsg, _("Unexpected token in response from IMAP server %s: "),
				engine->url->host);
	
	switch (token->token) {
	case CAMEL_IMAP4_TOKEN_NIL:
		g_string_append (errmsg, "NIL");
		break;
	case CAMEL_IMAP4_TOKEN_ATOM:
		g_string_append (errmsg, token->v.atom);
		break;
	case CAMEL_IMAP4_TOKEN_FLAG:
		g_string_append (errmsg, token->v.flag);
		break;
	case CAMEL_IMAP4_TOKEN_QSTRING:
		g_string_append (errmsg, token->v.qstring);
		break;
	case CAMEL_IMAP4_TOKEN_LITERAL:
		g_string_append_printf (errmsg, "{%u}", token->v.literal);
		break;
	case CAMEL_IMAP4_TOKEN_NUMBER:
		g_string_append_printf (errmsg, "%u", token->v.number);
		break;
	case CAMEL_IMAP4_TOKEN_NO_DATA:
		g_string_append (errmsg, _("No data"));
		break;
	default:
		g_string_append_c (errmsg, (unsigned char) (token->token & 0xff));
		break;
	}
	
	camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM, errmsg->str);
	
	g_string_free (errmsg, TRUE);
}


static struct {
	const char *name;
	guint32 flag;
} imap4_flags[] = {
	{ "\\Answered", CAMEL_MESSAGE_ANSWERED    },
	{ "\\Deleted",  CAMEL_MESSAGE_DELETED     },
	{ "\\Draft",    CAMEL_MESSAGE_DRAFT       },
	{ "\\Flagged",  CAMEL_MESSAGE_FLAGGED     },
	{ "\\Seen",     CAMEL_MESSAGE_SEEN        },
	/*{ "\\Recent",   CAMEL_MESSAGE_RECENT      },*/
	{ "\\*",        CAMEL_MESSAGE_USER        },
};

#if 0
static struct {
	const char *name;
	guint32 flag;
} imap4_user_flags[] = {
	{ "Forwarded",  CAMEL_MESSAGE_FORWARDED   },
};
#endif


int
camel_imap4_parse_flags_list (CamelIMAP4Engine *engine, guint32 *flags, CamelException *ex)
{
	camel_imap4_token_t token;
	guint32 new = 0;
	int i;
	
	if (camel_imap4_engine_next_token (engine, &token, ex) == -1)
		return -1;
	
	if (token.token != '(') {
		d(fprintf (stderr, "Expected to find a '(' token starting the flags list\n"));
		camel_imap4_utils_set_unexpected_token_error (ex, engine, &token);
		return -1;
	}
	
	if (camel_imap4_engine_next_token (engine, &token, ex) == -1)
		return -1;
	
	while (token.token == CAMEL_IMAP4_TOKEN_ATOM || token.token == CAMEL_IMAP4_TOKEN_FLAG) {
		/* parse the flags list */
		for (i = 0; i < G_N_ELEMENTS (imap4_flags); i++) {
			if (!g_ascii_strcasecmp (imap4_flags[i].name, token.v.atom)) {
				new |= imap4_flags[i].flag;
				break;
			}
		}
		
#if 0
		if (i == G_N_ELEMENTS (imap4_flags)) {
			for (i = 0; i < G_N_ELEMENTS (imap4_user_flags); i++) {
				if (!g_ascii_strcasecmp (imap4_user_flags[i].name, token.v.atom)) {
					new |= imap4_user_flags[i].flag;
					break;
				}
			}
			
			if (i == G_N_ELEMENTS (imap4_user_flags))
				fprintf (stderr, "Encountered unknown flag: %s\n", token.v.atom);
		}
#else
		if (i == G_N_ELEMENTS (imap4_flags))
			fprintf (stderr, "Encountered unknown flag: %s\n", token.v.atom);
#endif
		
		if (camel_imap4_engine_next_token (engine, &token, ex) == -1)
			return -1;
	}
	
	if (token.token != ')') {
		d(fprintf (stderr, "Expected to find a ')' token terminating the flags list\n"));
		camel_imap4_utils_set_unexpected_token_error (ex, engine, &token);
		return -1;
	}
	
	*flags = new;
	
	return 0;
}


struct {
	const char *name;
	guint32 flag;
} list_flags[] = {
	{ "\\Marked",        CAMEL_IMAP4_FOLDER_MARKED    },
	{ "\\Unmarked",      CAMEL_IMAP4_FOLDER_UNMARKED  },
	{ "\\Noselect",      CAMEL_FOLDER_NOSELECT        },
	{ "\\Noinferiors",   CAMEL_FOLDER_NOINFERIORS     },
	{ "\\HasChildren",   CAMEL_FOLDER_CHILDREN        },
	{ "\\HasNoChildren", CAMEL_FOLDER_NOCHILDREN      },
};

int
camel_imap4_untagged_list (CamelIMAP4Engine *engine, CamelIMAP4Command *ic, guint32 index, camel_imap4_token_t *token, CamelException *ex)
{
	GPtrArray *array = ic->user_data;
	camel_imap4_list_t *list;
	unsigned char *buf;
	guint32 flags = 0;
	GString *literal;
	char delim;
	size_t n;
	int i;
	
	if (camel_imap4_engine_next_token (engine, token, ex) == -1)
		return -1;
	
	/* parse the flag list */
	if (token->token != '(')
		goto unexpected;
	
	if (camel_imap4_engine_next_token (engine, token, ex) == -1)
		return -1;
	
	while (token->token == CAMEL_IMAP4_TOKEN_FLAG || token->token == CAMEL_IMAP4_TOKEN_ATOM) {
		for (i = 0; i < G_N_ELEMENTS (list_flags); i++) {
			if (!g_ascii_strcasecmp (list_flags[i].name, token->v.atom)) {
				flags |= list_flags[i].flag;
				break;
			}
		}
		
		if (camel_imap4_engine_next_token (engine, token, ex) == -1)
			return -1;
	}
	
	if (token->token != ')')
		goto unexpected;
	
	/* parse the path delimiter */
	if (camel_imap4_engine_next_token (engine, token, ex) == -1)
		return -1;
	
	switch (token->token) {
	case CAMEL_IMAP4_TOKEN_NIL:
		delim = '\0';
		break;
	case CAMEL_IMAP4_TOKEN_QSTRING:
		delim = *token->v.qstring;
		break;
	default:
		goto unexpected;
	}
	
	/* parse the folder name */
	if (camel_imap4_engine_next_token (engine, token, ex) == -1)
		return -1;
	
	list = g_new (camel_imap4_list_t, 1);
	list->flags = flags;
	list->delim = delim;
	
	switch (token->token) {
	case CAMEL_IMAP4_TOKEN_ATOM:
		list->name = g_strdup (token->v.atom);
		break;
	case CAMEL_IMAP4_TOKEN_QSTRING:
		list->name = g_strdup (token->v.qstring);
		break;
	case CAMEL_IMAP4_TOKEN_LITERAL:
		literal = g_string_new ("");
		while ((i = camel_imap4_stream_literal (engine->istream, &buf, &n)) == 1)
			g_string_append_len (literal, buf, n);
		
		if (i == -1) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      _("IMAP server %s unexpectedly disconnected: %s"),
					      engine->url->host, errno ? g_strerror (errno) : _("Unknown"));
			g_string_free (literal, TRUE);
			return -1;
		}
		
		g_string_append_len (literal, buf, n);
		list->name = literal->str;
		g_string_free (literal, FALSE);
		break;
	default:
		g_free (list);
		goto unexpected;
	}
	
	g_ptr_array_add (array, list);
	
	return camel_imap4_engine_eat_line (engine, ex);
	
 unexpected:
	
	camel_imap4_utils_set_unexpected_token_error (ex, engine, token);
	
	return -1;
}


static struct {
	const char *name;
	int type;
} imap4_status[] = {
	{ "MESSAGES",    CAMEL_IMAP4_STATUS_MESSAGES    },
	{ "RECENT",      CAMEL_IMAP4_STATUS_RECENT      },
	{ "UIDNEXT",     CAMEL_IMAP4_STATUS_UIDNEXT     },
	{ "UIDVALIDITY", CAMEL_IMAP4_STATUS_UIDVALIDITY },
	{ "UNSEEN",      CAMEL_IMAP4_STATUS_UNSEEN      },
};


void
camel_imap4_status_free (camel_imap4_status_t *status)
{
	camel_imap4_status_attr_t *attr, *next;
	
	attr = status->attr_list;
	while (attr != NULL) {
		next = attr->next;
		g_free (attr);
		attr = next;
	}
	
	g_free (status->mailbox);
	g_free (status);
}


int
camel_imap4_untagged_status (CamelIMAP4Engine *engine, CamelIMAP4Command *ic, guint32 index, camel_imap4_token_t *token, CamelException *ex)
{
	camel_imap4_status_attr_t *attr, *tail, *list = NULL;
	GPtrArray *array = ic->user_data;
	camel_imap4_status_t *status;
	char *mailbox;
	size_t len;
	int type;
	int i;
	
	if (camel_imap4_engine_next_token (engine, token, ex) == -1)
		return -1;
	
	switch (token->token) {
	case CAMEL_IMAP4_TOKEN_ATOM:
		mailbox = g_strdup (token->v.atom);
		break;
	case CAMEL_IMAP4_TOKEN_QSTRING:
		mailbox = g_strdup (token->v.qstring);
		break;
	case CAMEL_IMAP4_TOKEN_LITERAL:
		if (camel_imap4_engine_literal (engine, (unsigned char **) &mailbox, &len, ex) == -1)
			return -1;
		break;
	default:
		fprintf (stderr, "Unexpected token in IMAP4 untagged STATUS response: %s%c\n",
			 token->token == CAMEL_IMAP4_TOKEN_NIL ? "NIL" : "",
			 (unsigned char) (token->token & 0xff));
		camel_imap4_utils_set_unexpected_token_error (ex, engine, token);
		return -1;
	}
	
	if (camel_imap4_engine_next_token (engine, token, ex) == -1) {
		g_free (mailbox);
		return -1;
	}
	
	if (token->token != '(') {
		d(fprintf (stderr, "Expected to find a '(' token after the mailbox token in the STATUS response\n"));
		camel_imap4_utils_set_unexpected_token_error (ex, engine, token);
		g_free (mailbox);
		return -1;
	}
	
	if (camel_imap4_engine_next_token (engine, token, ex) == -1) {
		g_free (mailbox);
		return -1;
	}
	
	tail = (camel_imap4_status_attr_t *) &list;
	
	while (token->token == CAMEL_IMAP4_TOKEN_ATOM) {
		/* parse the status messages list */
		type = CAMEL_IMAP4_STATUS_UNKNOWN;
		for (i = 0; i < G_N_ELEMENTS (imap4_status); i++) {
			if (!g_ascii_strcasecmp (imap4_status[i].name, token->v.atom)) {
				type = imap4_status[i].type;
				break;
			}
		}
		
		if (type == CAMEL_IMAP4_STATUS_UNKNOWN)
			fprintf (stderr, "unrecognized token in STATUS list: %s\n", token->v.atom);
		
		if (camel_imap4_engine_next_token (engine, token, ex) == -1)
			goto exception;
		
		if (token->token != CAMEL_IMAP4_TOKEN_NUMBER)
			break;
		
		attr = g_new (camel_imap4_status_attr_t, 1);
		attr->next = NULL;
		attr->type = type;
		attr->value = token->v.number;
		
		tail->next = attr;
		tail = attr;
		
		if (camel_imap4_engine_next_token (engine, token, ex) == -1)
			goto exception;
	}
	
	status = g_new (camel_imap4_status_t, 1);
	status->mailbox = mailbox;
	status->attr_list = list;
	list = NULL;
	
	g_ptr_array_add (array, status);
	
	if (token->token != ')') {
		d(fprintf (stderr, "Expected to find a ')' token terminating the untagged STATUS response\n"));
		camel_imap4_utils_set_unexpected_token_error (ex, engine, token);
		return -1;
	}
	
	if (camel_imap4_engine_next_token (engine, token, ex) == -1)
		return -1;
	
	if (token->token != '\n') {
		d(fprintf (stderr, "Expected to find a '\\n' token after the STATUS response\n"));
		camel_imap4_utils_set_unexpected_token_error (ex, engine, token);
		return -1;
	}
	
	return 0;
	
 exception:
	
	g_free (mailbox);
	
	attr = list;
	while (attr != NULL) {
		list = attr->next;
		g_free (attr);
		attr = list;
	}
	
	return -1;
}
