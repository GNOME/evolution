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
#include <errno.h>

#include "camel-imap-engine.h"
#include "camel-imap-stream.h"
#include "camel-imap-command.h"

#include "camel-imap-utils.h"

#define d(x) x


void
camel_imap_flags_diff (flags_diff_t *diff, guint32 old, guint32 new)
{
	diff->changed = old ^ new;
	diff->bits = new & diff->changed;
}


guint32
camel_imap_flags_merge (flags_diff_t *diff, guint32 flags)
{
	return (flags & ~diff->changed) | diff->bits;
}


/**
 * camel_imap_merge_flags:
 * @original: original server flags
 * @local: local flags (after changes)
 * @server: new server flags (another client updated the server flags)
 *
 * Merge the local flag changes into the new server flags.
 *
 * Returns the merged flags.
 **/
guint32
camel_imap_merge_flags (guint32 original, guint32 local, guint32 server)
{
	flags_diff_t diff;
	
	camel_imap_flags_diff (&diff, original, local);
	
	return camel_imap_flags_merge (&diff, server);
}


void
camel_imap_utils_set_unexpected_token_error (CamelException *ex, CamelIMAPEngine *engine, camel_imap_token_t *token)
{
	GString *errmsg;
	
	if (ex == NULL)
		return;
	
	errmsg = g_string_new ("");
	g_string_append_printf (errmsg, _("Unexpected token in response from IMAP server %s: "),
				engine->url->host);
	
	switch (token->token) {
	case CAMEL_IMAP_TOKEN_NIL:
		g_string_append (errmsg, "NIL");
		break;
	case CAMEL_IMAP_TOKEN_ATOM:
		g_string_append (errmsg, token->v.atom);
		break;
	case CAMEL_IMAP_TOKEN_FLAG:
		g_string_append (errmsg, token->v.flag);
		break;
	case CAMEL_IMAP_TOKEN_QSTRING:
		g_string_append (errmsg, token->v.qstring);
		break;
	case CAMEL_IMAP_TOKEN_LITERAL:
		g_string_append_printf (errmsg, "{%u}", token->v.literal);
		break;
	case CAMEL_IMAP_TOKEN_NUMBER:
		g_string_append_printf (errmsg, "%u", token->v.number);
		break;
	case CAMEL_IMAP_TOKEN_NO_DATA:
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
} imap_flags[] = {
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
} imap_user_flags[] = {
	{ "Forwarded",  CAMEL_MESSAGE_FORWARDED   },
};
#endif


int
camel_imap_parse_flags_list (CamelIMAPEngine *engine, guint32 *flags, CamelException *ex)
{
	camel_imap_token_t token;
	guint32 new = 0;
	int i;
	
	if (camel_imap_engine_next_token (engine, &token, ex) == -1)
		return -1;
	
	if (token.token != '(') {
		d(fprintf (stderr, "Expected to find a '(' token starting the flags list\n"));
		camel_imap_utils_set_unexpected_token_error (ex, engine, &token);
		return -1;
	}
	
	if (camel_imap_engine_next_token (engine, &token, ex) == -1)
		return -1;
	
	while (token.token == CAMEL_IMAP_TOKEN_ATOM || token.token == CAMEL_IMAP_TOKEN_FLAG) {
		/* parse the flags list */
		for (i = 0; i < G_N_ELEMENTS (imap_flags); i++) {
			if (!strcasecmp (imap_flags[i].name, token.v.atom)) {
				new |= imap_flags[i].flag;
				break;
			}
		}
		
#if 0
		if (i == G_N_ELEMENTS (imap_flags)) {
			for (i = 0; i < G_N_ELEMENTS (imap_user_flags); i++) {
				if (!strcasecmp (imap_user_flags[i].name, token.v.atom)) {
					new |= imap_user_flags[i].flag;
					break;
				}
			}
			
			if (i == G_N_ELEMENTS (imap_user_flags))
				fprintf (stderr, "Encountered unknown flag: %s\n", token.v.atom);
		}
#else
		if (i == G_N_ELEMENTS (imap_flags))
			fprintf (stderr, "Encountered unknown flag: %s\n", token.v.atom);
#endif
		
		if (camel_imap_engine_next_token (engine, &token, ex) == -1)
			return -1;
	}
	
	if (token.token != ')') {
		d(fprintf (stderr, "Expected to find a ')' token terminating the flags list\n"));
		camel_imap_utils_set_unexpected_token_error (ex, engine, &token);
		return -1;
	}
	
	*flags = new;
	
	return 0;
}


struct {
	const char *name;
	guint32 flag;
} list_flags[] = {
	{ "\\Marked",        CAMEL_IMAP_FOLDER_MARKED          },
	{ "\\Unmarked",      CAMEL_IMAP_FOLDER_UNMARKED        },
	{ "\\Noselect",      CAMEL_IMAP_FOLDER_NOSELECT        },
	{ "\\Noinferiors",   CAMEL_IMAP_FOLDER_NOINFERIORS     },
	{ "\\HasChildren",   CAMEL_IMAP_FOLDER_HAS_CHILDREN    },
	{ "\\HasNoChildren", CAMEL_IMAP_FOLDER_HAS_NO_CHILDREN },
};

int
camel_imap_untagged_list (CamelIMAPEngine *engine, CamelIMAPCommand *ic, guint32 index, camel_imap_token_t *token, CamelException *ex)
{
	GPtrArray *array = ic->user_data;
	camel_imap_list_t *list;
	unsigned char *buf;
	guint32 flags = 0;
	GString *literal;
	char delim;
	size_t n;
	int i;
	
	if (camel_imap_engine_next_token (engine, token, ex) == -1)
		return -1;
	
	/* parse the flag list */
	if (token->token != '(')
		goto unexpected;
	
	if (camel_imap_engine_next_token (engine, token, ex) == -1)
		return -1;
	
	while (token->token == CAMEL_IMAP_TOKEN_FLAG || token->token == CAMEL_IMAP_TOKEN_ATOM) {
		for (i = 0; i < G_N_ELEMENTS (list_flags); i++) {
			if (!g_ascii_strcasecmp (list_flags[i].name, token->v.atom)) {
				flags |= list_flags[i].flag;
				break;
			}
		}
		
		if (camel_imap_engine_next_token (engine, token, ex) == -1)
			return -1;
	}
	
	if (token->token != ')')
		goto unexpected;
	
	/* parse the path delimiter */
	if (camel_imap_engine_next_token (engine, token, ex) == -1)
		return -1;
	
	switch (token->token) {
	case CAMEL_IMAP_TOKEN_NIL:
		delim = '\0';
		break;
	case CAMEL_IMAP_TOKEN_QSTRING:
		delim = *token->v.qstring;
		break;
	default:
		goto unexpected;
	}
	
	/* parse the folder name */
	if (camel_imap_engine_next_token (engine, token, ex) == -1)
		return -1;
	
	list = g_new (camel_imap_list_t, 1);
	list->flags = flags;
	list->delim = delim;
	
	switch (token->token) {
	case CAMEL_IMAP_TOKEN_ATOM:
		list->name = g_strdup (token->v.atom);
		break;
	case CAMEL_IMAP_TOKEN_QSTRING:
		list->name = g_strdup (token->v.qstring);
		break;
	case CAMEL_IMAP_TOKEN_LITERAL:
		literal = g_string_new ("");
		while ((i = camel_imap_stream_literal (engine->istream, &buf, &n)) == 1)
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
	
	return camel_imap_engine_eat_line (engine, ex);
	
 unexpected:
	
	camel_imap_utils_set_unexpected_token_error (ex, engine, token);
	
	return -1;
}
