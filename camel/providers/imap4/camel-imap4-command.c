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

#include <camel/camel-stream-null.h>
#include <camel/camel-stream-filter.h>
#include <camel/camel-mime-filter-crlf.h>
#include <camel/camel-i18n.h>

#include "camel-imap4-stream.h"
#include "camel-imap4-engine.h"
#include "camel-imap4-folder.h"
#include "camel-imap4-specials.h"

#include "camel-imap4-command.h"


#define d(x) x


enum {
	IMAP4_STRING_ATOM,
	IMAP4_STRING_QSTRING,
	IMAP4_STRING_LITERAL,
};

static int
imap4_string_get_type (const char *str)
{
	int type = 0;
	
	while (*str) {
		if (!is_atom (*str)) {
			if (is_qsafe (*str))
				type = IMAP4_STRING_QSTRING;
			else
				return IMAP4_STRING_LITERAL;
		}
		str++;
	}
	
	return type;
}

#if 0
static gboolean
imap4_string_is_atom_safe (const char *str)
{
	while (is_atom (*str))
		str++;
	
	return *str == '\0';
}

static gboolean
imap4_string_is_quote_safe (const char *str)
{
	while (is_qsafe (*str))
		str++;
	
	return *str == '\0';
}
#endif

static size_t
camel_imap4_literal_length (CamelIMAP4Literal *literal)
{
	CamelStream *stream, *null;
	CamelMimeFilter *crlf;
	size_t len;
	
	if (literal->type == CAMEL_IMAP4_LITERAL_STRING)
		return strlen (literal->literal.string);
	
	null = camel_stream_null_new ();
	crlf = camel_mime_filter_crlf_new (CAMEL_MIME_FILTER_CRLF_ENCODE, CAMEL_MIME_FILTER_CRLF_MODE_CRLF_ONLY);
	stream = (CamelStream *) camel_stream_filter_new_with_stream (null);
	camel_stream_filter_add ((CamelStreamFilter *) stream, crlf);
	camel_object_unref (crlf);
	
	switch (literal->type) {
	case CAMEL_IMAP4_LITERAL_STREAM:
		camel_stream_write_to_stream (literal->literal.stream, stream);
		camel_stream_reset (literal->literal.stream);
		break;
	case CAMEL_IMAP4_LITERAL_WRAPPER:
		camel_data_wrapper_write_to_stream (literal->literal.wrapper, stream);
		break;
	default:
		g_assert_not_reached ();
		break;
	}
	
	len = ((CamelStreamNull *) null)->written;
	
	camel_object_unref (stream);
	camel_object_unref (null);
	
	return len;
}

static CamelIMAP4CommandPart *
command_part_new (void)
{
	CamelIMAP4CommandPart *part;
	
	part = g_new (CamelIMAP4CommandPart, 1);
	part->next = NULL;
	part->buffer = NULL;
	part->buflen = 0;
	part->literal = NULL;
	
	return part;
}

static void
imap4_command_append_string (CamelIMAP4Engine *engine, CamelIMAP4CommandPart **tail, GString *str, const char *string)
{
	CamelIMAP4CommandPart *part;
	CamelIMAP4Literal *literal;
	
	switch (imap4_string_get_type (string)) {
	case IMAP4_STRING_ATOM:
		/* string is safe as it is... */
		g_string_append (str, string);
		break;
	case IMAP4_STRING_QSTRING:
		/* we need to quote the string */
		/* FIXME: need to escape stuff */
		g_string_append_printf (str, "\"%s\"", string);
		break;
	case IMAP4_STRING_LITERAL:
		if (engine->capa & CAMEL_IMAP4_CAPABILITY_LITERALPLUS) {
			/* we have to send a literal, but the server supports LITERAL+ so use that */
			g_string_append_printf (str, "{%u+}\r\n%s", strlen (string), string);
		} else {
			/* we have to make it a literal */
			literal = g_new (CamelIMAP4Literal, 1);
			literal->type = CAMEL_IMAP4_LITERAL_STRING;
			literal->literal.string = g_strdup (string);
			
			g_string_append_printf (str, "{%u}\r\n", strlen (string));
			
			(*tail)->buffer = g_strdup (str->str);
			(*tail)->buflen = str->len;
			(*tail)->literal = literal;
			
			part = command_part_new ();
			(*tail)->next = part;
			(*tail) = part;
			
			g_string_truncate (str, 0);
		}
		break;
	}
}

CamelIMAP4Command *
camel_imap4_command_newv (CamelIMAP4Engine *engine, CamelIMAP4Folder *imap4_folder, const char *format, va_list args)
{
	CamelIMAP4CommandPart *parts, *part, *tail;
	CamelIMAP4Command *ic;
	const char *start;
	GString *str;
	
	tail = parts = command_part_new ();
	
	str = g_string_new ("");
	start = format;
	
	while (*format) {
		register char ch = *format++;
		
		if (ch == '%') {
			CamelIMAP4Literal *literal;
			CamelIMAP4Folder *folder;
			char *function, **strv;
			unsigned int u;
			char *string;
			size_t len;
			void *obj;
			int c, d;
			
			g_string_append_len (str, start, format - start - 1);
			
			switch (*format) {
			case '%':
				/* literal % */
				g_string_append_c (str, '%');
				break;
			case 'c':
				/* character */
				c = va_arg (args, int);
				g_string_append_c (str, c);
				break;
			case 'd':
				/* integer */
				d = va_arg (args, int);
				g_string_append_printf (str, "%d", d);
				break;
			case 'u':
				/* unsigned integer */
				u = va_arg (args, unsigned int);
				g_string_append_printf (str, "%u", u);
				break;
			case 'F':
				/* CamelIMAP4Folder */
				folder = va_arg (args, CamelIMAP4Folder *);
				string = (char *) camel_imap4_folder_utf7_name (folder);
				imap4_command_append_string (engine, &tail, str, string);
				break;
			case 'L':
				/* Literal */
				obj = va_arg (args, void *);
				
				literal = g_new (CamelIMAP4Literal, 1);
				if (CAMEL_IS_DATA_WRAPPER (obj)) {
					literal->type = CAMEL_IMAP4_LITERAL_WRAPPER;
					literal->literal.wrapper = obj;
				} else if (CAMEL_IS_STREAM (obj)) {
					literal->type = CAMEL_IMAP4_LITERAL_STREAM;
					literal->literal.stream = obj;
				} else {
					g_assert_not_reached ();
				}
				
				camel_object_ref (obj);
				
				/* FIXME: take advantage of LITERAL+? */
				len = camel_imap4_literal_length (literal);
				g_string_append_printf (str, "{%u}\r\n", len);
				
				tail->buffer = g_strdup (str->str);
				tail->buflen = str->len;
				tail->literal = literal;
				
				part = command_part_new ();
				tail->next = part;
				tail = part;
				
				g_string_truncate (str, 0);
				
				break;
			case 'V':
				/* a string vector of arguments which may need to be quoted or made into literals */
				function = str->str + str->len - 2;
				while (*function != ' ')
					function--;
				function++;
				
				function = g_strdup (function);
				
				strv = va_arg (args, char **);
				for (d = 0; strv[d]; d++) {
					if (d > 0)
						g_string_append (str, function);
					imap4_command_append_string (engine, &tail, str, strv[d]);
				}
				
				g_free (function);
				break;
			case 'S':
				/* string which may need to be quoted or made into a literal */
				string = va_arg (args, char *);
				imap4_command_append_string (engine, &tail, str, string);
				break;
			case 's':
				/* safe atom string */
				string = va_arg (args, char *);
				g_string_append (str, string);
				break;
			default:
				g_warning ("unknown formatter %%%c", *format);
				g_string_append_c (str, '%');
				g_string_append_c (str, *format);
				break;
			}
			
			format++;
			
			start = format;
		}
	}
	
	g_string_append (str, start);
	tail->buffer = str->str;
	tail->buflen = str->len;
	tail->literal = NULL;
	g_string_free (str, FALSE);
	
	ic = g_new0 (CamelIMAP4Command, 1);
	((EDListNode *) ic)->next = NULL;
	((EDListNode *) ic)->prev = NULL;
	ic->untagged = g_hash_table_new (g_str_hash, g_str_equal);
	ic->status = CAMEL_IMAP4_COMMAND_QUEUED;
	ic->resp_codes = g_ptr_array_new ();
	ic->engine = engine;
	ic->ref_count = 1;
	ic->parts = parts;
	ic->part = parts;
	
	camel_exception_init (&ic->ex);
	
	if (imap4_folder) {
		camel_object_ref (imap4_folder);
		ic->folder = imap4_folder;
	} else
		ic->folder = NULL;
	
	return ic;
}

CamelIMAP4Command *
camel_imap4_command_new (CamelIMAP4Engine *engine, CamelIMAP4Folder *folder, const char *format, ...)
{
	CamelIMAP4Command *command;
	va_list args;
	
	va_start (args, format);
	command = camel_imap4_command_newv (engine, folder, format, args);
	va_end (args);
	
	return command;
}

void
camel_imap4_command_register_untagged (CamelIMAP4Command *ic, const char *atom, CamelIMAP4UntaggedCallback untagged)
{
	g_hash_table_insert (ic->untagged, g_strdup (atom), untagged);
}

void
camel_imap4_command_ref (CamelIMAP4Command *ic)
{
	ic->ref_count++;
}

void
camel_imap4_command_unref (CamelIMAP4Command *ic)
{
	CamelIMAP4CommandPart *part, *next;
	int i;
	
	if (ic == NULL)
		return;
	
	ic->ref_count--;
	if (ic->ref_count == 0) {
		if (ic->folder)
			camel_object_unref (ic->folder);
		
		g_free (ic->tag);
		
		for (i = 0; i < ic->resp_codes->len; i++) {
			CamelIMAP4RespCode *resp_code;
			
			resp_code = ic->resp_codes->pdata[i];
			camel_imap4_resp_code_free (resp_code);
		}
		g_ptr_array_free (ic->resp_codes, TRUE);
		
		g_hash_table_foreach (ic->untagged, (GHFunc) g_free, NULL);
		g_hash_table_destroy (ic->untagged);
		
		camel_exception_clear (&ic->ex);
		
		part = ic->parts;
		while (part != NULL) {
			g_free (part->buffer);
			if (part->literal) {
				switch (part->literal->type) {
				case CAMEL_IMAP4_LITERAL_STRING:
					g_free (part->literal->literal.string);
					break;
				case CAMEL_IMAP4_LITERAL_STREAM:
					camel_object_unref (part->literal->literal.stream);
					break;
				case CAMEL_IMAP4_LITERAL_WRAPPER:
					camel_object_unref (part->literal->literal.wrapper);
					break;
				}
				
				g_free (part->literal);
			}
			
			next = part->next;
			g_free (part);
			part = next;
		}
		
		g_free (ic);
	}
}


static int
imap4_literal_write_to_stream (CamelIMAP4Literal *literal, CamelStream *stream)
{
	CamelStream *istream, *ostream = NULL;
	CamelDataWrapper *wrapper;
	CamelMimeFilter *crlf;
	char *string;
	
	if (literal->type == CAMEL_IMAP4_LITERAL_STRING) {
		string = literal->literal.string;
		if (camel_stream_write (stream, string, strlen (string)) == -1)
			return -1;
		
		return 0;
	}
	
	crlf = camel_mime_filter_crlf_new (CAMEL_MIME_FILTER_CRLF_ENCODE, CAMEL_MIME_FILTER_CRLF_MODE_CRLF_ONLY);
	ostream = (CamelStream *) camel_stream_filter_new_with_stream (stream);
	camel_stream_filter_add ((CamelStreamFilter *) ostream, crlf);
	camel_object_unref (crlf);
	
	/* write the literal */
	switch (literal->type) {
	case CAMEL_IMAP4_LITERAL_STREAM:
		istream = literal->literal.stream;
		if (camel_stream_write_to_stream (istream, ostream) == -1)
			goto exception;
		break;
	case CAMEL_IMAP4_LITERAL_WRAPPER:
		wrapper = literal->literal.wrapper;
		if (camel_data_wrapper_write_to_stream (wrapper, ostream) == -1)
			goto exception;
		break;
	}
	
	camel_object_unref (ostream);
	ostream = NULL;
	
#if 0
	if (camel_stream_write (stream, "\r\n", 2) == -1)
		return -1;
#endif
	
	return 0;
	
 exception:
	
	camel_object_unref (ostream);
	
	return -1;
}


static void
unexpected_token (camel_imap4_token_t *token)
{
	switch (token->token) {
	case CAMEL_IMAP4_TOKEN_NO_DATA:
		fprintf (stderr, "*** NO DATA ***");
		break;
	case CAMEL_IMAP4_TOKEN_ERROR:
		fprintf (stderr, "*** ERROR ***");
		break;
	case CAMEL_IMAP4_TOKEN_NIL:
		fprintf (stderr, "NIL");
		break;
	case CAMEL_IMAP4_TOKEN_ATOM:
	        fprintf (stderr, "%s", token->v.atom);
		break;
	case CAMEL_IMAP4_TOKEN_QSTRING:
	        fprintf (stderr, "\"%s\"", token->v.qstring);
		break;
	case CAMEL_IMAP4_TOKEN_LITERAL:
		fprintf (stderr, "{%u}", token->v.literal);
		break;
	default:
		fprintf (stderr, "%c", (unsigned char) (token->token & 0xff));
		break;
	}
}

int
camel_imap4_command_step (CamelIMAP4Command *ic)
{
	CamelIMAP4Engine *engine = ic->engine;
	int result = CAMEL_IMAP4_RESULT_NONE;
	CamelIMAP4Literal *literal;
	camel_imap4_token_t token;
	unsigned char *linebuf;
	ssize_t nwritten;
	size_t len;
	
	g_assert (ic->part != NULL);
	
	if (ic->part == ic->parts) {
		ic->tag = g_strdup_printf ("%c%.5u", engine->tagprefix, engine->tag++);
		camel_stream_printf (engine->ostream, "%s ", ic->tag);
		d(fprintf (stderr, "sending: %s ", ic->tag));
	}
	
#if d(!)0
	{
		int sending = ic->part != ic->parts;
		unsigned char *eoln, *eob;
		
		linebuf = ic->part->buffer;
		eob = linebuf + ic->part->buflen;
		
		do {
			eoln = linebuf;
			while (eoln < eob && *eoln != '\n')
				eoln++;
			
			if (eoln < eob)
				eoln++;
			
			if (sending)
				fwrite ("sending: ", 1, 10, stderr);
			fwrite (linebuf, 1, eoln - linebuf, stderr);
			
			linebuf = eoln + 1;
			sending = 1;
		} while (linebuf < eob);
	}
#endif
	
	linebuf = ic->part->buffer;
	len = ic->part->buflen;
	
	if ((nwritten = camel_stream_write (engine->ostream, linebuf, len)) == -1) {
		camel_exception_setv (&ic->ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Failed sending command to IMAP server %s: %s"),
				      engine->url->host, g_strerror (errno));
		goto exception;
	}
	
	if (camel_stream_flush (engine->ostream) == -1) {
		camel_exception_setv (&ic->ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Failed sending command to IMAP server %s: %s"),
				      engine->url->host, g_strerror (errno));
		goto exception;
	}
	
	/* now we need to read the response(s) from the IMAP4 server */
	
	do {
		if (camel_imap4_engine_next_token (engine, &token, &ic->ex) == -1)
			goto exception;
		
		if (token.token == '+') {
			/* we got a continuation response from the server */
			literal = ic->part->literal;
			
			if (camel_imap4_engine_line (engine, &linebuf, &len, &ic->ex) == -1)
				goto exception;
			
			if (literal) {
				if (imap4_literal_write_to_stream (literal, engine->ostream) == -1)
					goto exception;
				
				g_free (linebuf);
				linebuf = NULL;
				
				break;
			} else if (ic->plus) {
				/* command expected a '+' response - probably AUTHENTICATE? */
				if (ic->plus (engine, ic, linebuf, len, &ic->ex) == -1) {
					g_free (linebuf);
					return -1;
				}
				
				/* now we need to wait for a "<tag> OK/NO/BAD" response */
			} else {
				/* FIXME: error?? */
				g_assert_not_reached ();
			}
			
			g_free (linebuf);
			linebuf = NULL;
		} else if (token.token == '*') {
			/* we got an untagged response, let the engine handle this */
			if (camel_imap4_engine_handle_untagged_1 (engine, &token, &ic->ex) == -1)
				goto exception;
		} else if (token.token == CAMEL_IMAP4_TOKEN_ATOM && !strcmp (token.v.atom, ic->tag)) {
			/* we got "<tag> OK/NO/BAD" */
			fprintf (stderr, "got %s response\n", token.v.atom);
			
			if (camel_imap4_engine_next_token (engine, &token, &ic->ex) == -1)
				goto exception;
			
			if (token.token == CAMEL_IMAP4_TOKEN_ATOM) {
				if (!strcmp (token.v.atom, "OK"))
					result = CAMEL_IMAP4_RESULT_OK;
				else if (!strcmp (token.v.atom, "NO"))
					result = CAMEL_IMAP4_RESULT_NO;
				else if (!strcmp (token.v.atom, "BAD"))
					result = CAMEL_IMAP4_RESULT_BAD;
				
				if (result == CAMEL_IMAP4_RESULT_NONE) {
					fprintf (stderr, "expected OK/NO/BAD but got %s\n", token.v.atom);
					goto unexpected;
				}
				
				if (camel_imap4_engine_next_token (engine, &token, &ic->ex) == -1)
					goto exception;
				
				if (token.token == '[') {
					/* we have a response code */
					camel_imap4_stream_unget_token (engine->istream, &token);
					if (camel_imap4_engine_parse_resp_code (engine, &ic->ex) == -1)
						goto exception;
				} else if (token.token != '\n') {
					/* just gobble up the rest of the line */
					if (camel_imap4_engine_line (engine, NULL, NULL, &ic->ex) == -1)
						goto exception;
				}
			} else {
				fprintf (stderr, "expected anything but this: ");
				unexpected_token (&token);
				fprintf (stderr, "\n");
				
				goto unexpected;
			}
			
			break;
		} else {
			fprintf (stderr, "wtf is this: ");
			unexpected_token (&token);
			fprintf (stderr, "\n");
			
		unexpected:
			
			/* no fucking clue what we got... */
			if (camel_imap4_engine_line (engine, &linebuf, &len, &ic->ex) == -1)
				goto exception;
			
			camel_exception_setv (&ic->ex, CAMEL_EXCEPTION_SYSTEM,
					      _("Unexpected response from IMAP4 server %s: %s"),
					      engine->url->host, linebuf);
			
			g_free (linebuf);
			
			goto exception;
		}
	} while (1);
	
	/* status should always be ACTIVE here... */
	if (ic->status == CAMEL_IMAP4_COMMAND_ACTIVE) {
		ic->part = ic->part->next;
		if (ic->part == NULL || result) {
			ic->status = CAMEL_IMAP4_COMMAND_COMPLETE;
			ic->result = result;
			return 1;
		}
	}
	
	return 0;
	
 exception:
	
	ic->status = CAMEL_IMAP4_COMMAND_ERROR;
	
	return -1;
}


void
camel_imap4_command_reset (CamelIMAP4Command *ic)
{
	int i;
	
	for (i = 0; i < ic->resp_codes->len; i++)
		camel_imap4_resp_code_free (ic->resp_codes->pdata[i]);
	g_ptr_array_set_size (ic->resp_codes, 0);
	
	ic->status = CAMEL_IMAP4_COMMAND_QUEUED;
	ic->result = CAMEL_IMAP4_RESULT_NONE;
	ic->part = ic->parts;
	g_free (ic->tag);
	ic->tag = NULL;
	
	camel_exception_clear (&ic->ex);
}
