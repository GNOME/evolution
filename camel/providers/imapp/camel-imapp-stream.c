/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; fill-column: 160 -*-
 *
 * Author:
 *  Michael Zucchi <notzed@ximian.com>
 *
 * Copyright 1999, 2000 Ximian, Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

#include <glib.h>

#include <camel/camel-stream-mem.h>

#include "camel-imapp-stream.h"
#include "camel-imapp-exception.h"

#define t(x) 
#define io(x) x

static void setup_table(void);

static CamelObjectClass *parent_class = NULL;

/* Returns the class for a CamelStream */
#define CS_CLASS(so) CAMEL_IMAPP_STREAM_CLASS(CAMEL_OBJECT_GET_CLASS(so))

#define CAMEL_IMAPP_STREAM_SIZE (4096)
#define CAMEL_IMAPP_STREAM_TOKEN (4096) /* maximum token size */

static int
stream_fill(CamelIMAPPStream *is)
{
	int left = 0;

	if (is->source) {
		left = is->end - is->ptr;
		memcpy(is->buf, is->ptr, left);
		is->end = is->buf + left;
		is->ptr = is->buf;
		left = camel_stream_read(is->source, is->end, CAMEL_IMAPP_STREAM_SIZE - (is->end - is->buf));
		if (left > 0) {
			is->end += left;
			io(printf("camel_imapp_read: buffer is '%.*s'\n", is->end - is->ptr, is->ptr));
			return is->end - is->ptr;
		} else {
			io(printf("camel_imapp_read: -1\n"));
			return -1;
		}
	}

	printf("camel_imapp_read: 0\n");

	return 0;
}

static ssize_t
stream_read(CamelStream *stream, char *buffer, size_t n)
{
	CamelIMAPPStream *is = (CamelIMAPPStream *)stream;
	ssize_t max;

	if (is->literal == 0 || n == 0)
		return 0;

	max = is->end - is->ptr;
	if (max > 0) {
		max = MIN(max, is->literal);
		max = MIN(max, n);
		memcpy(buffer, is->ptr, max);
		is->ptr += max;
	} else {
		max = MIN(is->literal, n);
		max = camel_stream_read(is->source, buffer, max);
		if (max <= 0)
			return max;
	}

	is->literal -= max;
	
	return max;
}

static ssize_t
stream_write(CamelStream *stream, const char *buffer, size_t n)
{
	CamelIMAPPStream *is = (CamelIMAPPStream *)stream;

	return camel_stream_write(is->source, buffer, n);
}

static int
stream_close(CamelStream *stream)
{
	/* nop? */
	return 0;
}

static int
stream_flush(CamelStream *stream)
{
	/* nop? */
	return 0;
}

static gboolean
stream_eos(CamelStream *stream)
{
	CamelIMAPPStream *is = (CamelIMAPPStream *)stream;

	return is->literal == 0;
}

static int
stream_reset(CamelStream *stream)
{
	/* nop?  reset literal mode? */
	return 0;
}

static void
camel_imapp_stream_class_init (CamelStreamClass *camel_imapp_stream_class)
{
	CamelStreamClass *camel_stream_class = (CamelStreamClass *)camel_imapp_stream_class;

	parent_class = camel_type_get_global_classfuncs( CAMEL_OBJECT_TYPE );

	/* virtual method definition */
	camel_stream_class->read = stream_read;
	camel_stream_class->write = stream_write;
	camel_stream_class->close = stream_close;
	camel_stream_class->flush = stream_flush;
	camel_stream_class->eos = stream_eos;
	camel_stream_class->reset = stream_reset;
}

static void
camel_imapp_stream_init(CamelIMAPPStream *is, CamelIMAPPStreamClass *isclass)
{
	/* +1 is room for appending a 0 if we need to for a token */
	is->ptr = is->end = is->buf = g_malloc(CAMEL_IMAPP_STREAM_SIZE+1);
	is->tokenptr = is->tokenbuf = g_malloc(CAMEL_IMAPP_STREAM_SIZE+1);
	is->tokenend = is->tokenbuf + CAMEL_IMAPP_STREAM_SIZE;
}

static void
camel_imapp_stream_finalise(CamelIMAPPStream *is)
{
	g_free(is->buf);
	if (is->source)
		camel_object_unref((CamelObject *)is->source);
}

CamelType
camel_imapp_stream_get_type (void)
{
	static CamelType camel_imapp_stream_type = CAMEL_INVALID_TYPE;

	if (camel_imapp_stream_type == CAMEL_INVALID_TYPE) {
		setup_table();
		camel_imapp_stream_type = camel_type_register( camel_stream_get_type(),
							    "CamelIMAPPStream",
							    sizeof( CamelIMAPPStream ),
							    sizeof( CamelIMAPPStreamClass ),
							    (CamelObjectClassInitFunc) camel_imapp_stream_class_init,
							    NULL,
							    (CamelObjectInitFunc) camel_imapp_stream_init,
							    (CamelObjectFinalizeFunc) camel_imapp_stream_finalise );
	}

	return camel_imapp_stream_type;
}

/**
 * camel_imapp_stream_new:
 *
 * Returns a NULL stream.  A null stream is always at eof, and
 * always returns success for all reads and writes.
 *
 * Return value: the stream
 **/
CamelStream *
camel_imapp_stream_new(CamelStream *source)
{
	CamelIMAPPStream *is;

	is = (CamelIMAPPStream *)camel_object_new(camel_imapp_stream_get_type ());
	camel_object_ref((CamelObject *)source);
	is->source = source;

	return (CamelStream *)is;
}


/*
 From rfc2060

ATOM_CHAR       ::= <any CHAR except atom_specials>

atom_specials   ::= "(" / ")" / "{" / SPACE / CTL / list_wildcards /
                    quoted_specials

CHAR            ::= <any 7-bit US-ASCII character except NUL,
                     0x01 - 0x7f>

CTL             ::= <any ASCII control character and DEL,
                        0x00 - 0x1f, 0x7f>

SPACE           ::= <ASCII SP, space, 0x20>

list_wildcards  ::= "%" / "*"

quoted_specials ::= <"> / "\"
*/

static unsigned char imap_specials[256] = {
/* 00 */0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 10 */0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 20 */0, 1, 0, 1, 1, 0, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1,
/* 30 */1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
/* 40 */1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
/* 50 */1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1,
/* 60 */1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
/* 70 */1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
};

#define imap_is_atom(c) ((imap_specials[(c)&0xff] & 0x01) != 0)
#define imap_is_simple(c) ((imap_specials[(c)&0xff] & 0x02) != 0)
#define imap_not_id(c) ((imap_specials[(c)&0xff] & 0x04) != 0)

/* could be pregenerated, but this is cheap */
static struct {
	unsigned char *chars;
	unsigned char mask;
} is_masks[] = {
	{ "\n*()[]+", 2 },
	{ " \r\n()[]+", 4 },
};

static void setup_table(void)
{
	int i;
	unsigned char *p, c;

	for (i=0;i<(int)(sizeof(is_masks)/sizeof(is_masks[0]));i++) {
		p = is_masks[i].chars;
		while ((c = *p++))
			imap_specials[c] |= is_masks[i].mask;
	}
}

#if 0

static int
skip_ws(CamelIMAPPStream *is, unsigned char *pp, unsigned char *pe)
{
	register unsigned char c, *p;
	unsigned char *e;

	p = is->ptr;
	e = is->end;

	do {
		while (p >= e ) {
			is->ptr = p;
			if (stream_fill(is) == IMAP_TOK_ERROR)
				return IMAP_TOK_ERROR;
			p = is->ptr;
			e = is->end;
		}
		c = *p++;
	} while (c == ' ' || c == '\r');

	is->ptr = p;
	is->end = e;

	return c;
}
#endif

/* FIXME: these should probably handle it themselves,
   and get rid of the token interface? */
int
camel_imapp_stream_atom(CamelIMAPPStream *is, unsigned char **data, unsigned int *lenp)
{
	unsigned char *p, c;

	/* this is only 'approximate' atom */
	switch(camel_imapp_stream_token(is, data, lenp)) {
	case IMAP_TOK_TOKEN:
		p = *data;
		while ((c = *p))
			*p++ = toupper(c);
	case IMAP_TOK_INT:
		return 0;
	case IMAP_TOK_ERROR:
		return IMAP_TOK_ERROR;
	default:
		camel_exception_throw(1, "expecting atom");
		printf("expecting atom!\n");
		return IMAP_TOK_PROTOCOL;
	}
}

/* gets an atom, a quoted_string, or a literal */
int
camel_imapp_stream_astring(CamelIMAPPStream *is, unsigned char **data)
{
	unsigned char *p, *start;
	unsigned int len, inlen;

	switch(camel_imapp_stream_token(is, data, &len)) {
	case IMAP_TOK_TOKEN:
	case IMAP_TOK_INT:
	case IMAP_TOK_STRING:
		return 0;
	case IMAP_TOK_LITERAL:
		/* FIXME: just grow buffer */
		if (len >= CAMEL_IMAPP_STREAM_TOKEN) {
			camel_exception_throw(1, "astring: literal too long");
			printf("astring too long\n");
			return IMAP_TOK_PROTOCOL;
		}
		p = is->tokenptr;
		camel_imapp_stream_set_literal(is, len);
		do {
			len = camel_imapp_stream_getl(is, &start, &inlen);
			if (len < 0)
				return len;
			memcpy(p, start, inlen);
			p += inlen;
		} while (len > 0);
		*data = is->tokenptr;
		return 0;
	case IMAP_TOK_ERROR:
		/* wont get unless no exception hanlder*/
		return IMAP_TOK_ERROR;
	default:
		camel_exception_throw(1, "expecting astring");
		printf("expecting astring!\n");
		return IMAP_TOK_PROTOCOL;
	}
}

/* check for NIL or (small) quoted_string or literal */
int
camel_imapp_stream_nstring(CamelIMAPPStream *is, unsigned char **data)
{
	unsigned char *p, *start;
	unsigned int len, inlen;

	switch(camel_imapp_stream_token(is, data, &len)) {
	case IMAP_TOK_STRING:
		return 0;
	case IMAP_TOK_LITERAL:
		/* FIXME: just grow buffer */
		if (len >= CAMEL_IMAPP_STREAM_TOKEN) {
			camel_exception_throw(1, "nstring: literal too long");
			return IMAP_TOK_PROTOCOL;
		}
		p = is->tokenptr;
		camel_imapp_stream_set_literal(is, len);
		do {
			len = camel_imapp_stream_getl(is, &start, &inlen);
			if (len < 0)
				return len;
			memcpy(p, start, inlen);
			p += inlen;
		} while (len > 0);
		*data = is->tokenptr;
		return 0;
	case IMAP_TOK_TOKEN:
		p = *data;
		if (toupper(p[0]) == 'N' && toupper(p[1]) == 'I' && toupper(p[2]) == 'L' && p[3] == 0) {
			*data = NULL;
			return 0;
		}
	default:
		camel_exception_throw(1, "expecting nstring");
		return IMAP_TOK_PROTOCOL;
	case IMAP_TOK_ERROR:
		/* we'll never get this unless there are no exception  handlers anyway */
		return IMAP_TOK_ERROR;

	}
}

/* parse an nstring as a stream */
int
camel_imapp_stream_nstring_stream(CamelIMAPPStream *is, CamelStream **stream)
/* throws IO,PARSE exception */
{
	unsigned char *token;
	unsigned int len;
	int ret = 0;
	CamelStream * volatile mem = NULL;

	*stream = NULL;

	CAMEL_TRY {
		switch(camel_imapp_stream_token(is, &token, &len)) {
		case IMAP_TOK_STRING:
			mem = camel_stream_mem_new_with_buffer(token, len);
			*stream = mem;
			break;
		case IMAP_TOK_LITERAL:
			/* if len is big, we could automatically use a file backing */
			camel_imapp_stream_set_literal(is, len);
			mem = camel_stream_mem_new();
			if (camel_stream_write_to_stream((CamelStream *)is, mem) == -1)
				camel_exception_throw(1, "nstring: io error: %s", strerror(errno));
			camel_stream_reset(mem);
			*stream = mem;
			break;
		case IMAP_TOK_TOKEN:
			if (toupper(token[0]) == 'N' && toupper(token[1]) == 'I' && toupper(token[2]) == 'L' && token[3] == 0) {
				*stream = NULL;
				break;
			}
		default:
			ret = -1;
			camel_exception_throw(1, "nstring: token not string");
		}
	} CAMEL_CATCH(ex) {
		if (mem)
			camel_object_unref((CamelObject *)mem);
		camel_exception_throw_ex(ex);
	} CAMEL_DONE;

	/* never reaches here anyway */
	return ret;
}

guint32
camel_imapp_stream_number(CamelIMAPPStream *is)
{
	unsigned char *token;
	unsigned int len;

	if (camel_imapp_stream_token(is, &token, &len) != IMAP_TOK_INT) {
		camel_exception_throw(1, "expecting number");
		return 0;
	}

	return strtoul(token, 0, 10);
}

int
camel_imapp_stream_text(CamelIMAPPStream *is, unsigned char **text)
{
	GByteArray *build = g_byte_array_new();
	unsigned char *token;
	unsigned int len;
	int tok;

	CAMEL_TRY {
		while (is->unget > 0) {
			switch (is->unget_tok) {
			case IMAP_TOK_TOKEN:
			case IMAP_TOK_STRING:
			case IMAP_TOK_INT:
				g_byte_array_append(build, is->unget_token, is->unget_len);
				g_byte_array_append(build, " ", 1);
			default: /* invalid, but we'll ignore */
				break;
			}
			is->unget--;
		}

		do {
			tok = camel_imapp_stream_gets(is, &token, &len);
			if (tok < 0)
				camel_exception_throw(1, "io error: %s", strerror(errno));
			if (len)
				g_byte_array_append(build, token, len);
		} while (tok > 0);
	} CAMEL_CATCH(ex) {
		*text = NULL;
		g_byte_array_free(build, TRUE);
		camel_exception_throw_ex(ex);
	} CAMEL_DONE;

	g_byte_array_append(build, "", 1);
	*text = build->data;
	g_byte_array_free(build, FALSE);

	return 0;
}

/* Get one token from the imap stream */
camel_imapp_token_t
/* throws IO,PARSE exception */
camel_imapp_stream_token(CamelIMAPPStream *is, unsigned char **data, unsigned int *len)
{
	register unsigned char c, *p, *o, *oe;
	unsigned char *e;
	unsigned int literal;
	int digits;

	if (is->unget > 0) {
		is->unget--;
		*data = is->unget_token;
		*len = is->unget_len;
		/*printf("token UNGET '%c' %s\n", is->unget_tok, is->unget_token);*/
		return is->unget_tok;
	}

	if (is->literal > 0)
		g_warning("stream_token called with literal %d", is->literal);

	p = is->ptr;
	e = is->end;

	/* skip whitespace/prefill buffer */
	do {
		while (p >= e ) {
			is->ptr = p;
			if (stream_fill(is) == IMAP_TOK_ERROR)
				goto io_error;
			p = is->ptr;
			e = is->end;
		}
		c = *p++;
	} while (c == ' ' || c == '\r');

	/*strchr("\n*()[]+", c)*/
	if (imap_is_simple(c)) {
		is->ptr = p;
		t(printf("token '%c'\n", c));
		return c;
	} else if (c == '{') {
		literal = 0;
		*data = p;
		while (1) {
			while (p < e) {
				c = *p++;
				if (isdigit(c) && literal < (UINT_MAX/10)) {
					literal = literal * 10 + (c - '0');
				} else if (c == '}') {
					while (1) {
						while (p < e) {
							c = *p++;
							if (c == '\n') {
								*len = literal;
								is->ptr = p;
								is->literal = literal;
								t(printf("token LITERAL %d\n", literal));
								return IMAP_TOK_LITERAL;
							}
						}
						is->ptr = p;
						if (stream_fill(is) == IMAP_TOK_ERROR)
							goto io_error;
						p = is->ptr;
						e = is->end;
					}
				} else {
					if (isdigit(c))
						printf("Protocol error: literal too big\n");
					else
						printf("Protocol error: literal contains invalid char %02x '%c'\n", c, isprint(c)?c:c);
					goto protocol_error;
				}
			}
			is->ptr = p;
			if (stream_fill(is) == IMAP_TOK_ERROR)
				goto io_error;
			p = is->ptr;
			e = is->end;
		}
	} else if (c == '"') {
		o = is->tokenptr;
		oe = is->tokenptr + CAMEL_IMAPP_STREAM_TOKEN - 1;
		while (1) {
			while (p < e) {
				c = *p++;
				if (c == '\\') {
					while (p >= e) {
						is->ptr = p;
						if (stream_fill(is) == IMAP_TOK_ERROR)
							goto io_error;
						p = is->ptr;
						e = is->end;
					}
					c = *p++;
				} else if (c == '\"') {
					is->ptr = p;
					*o = 0;
					*data = is->tokenbuf;
					*len = o - is->tokenbuf;
					t(printf("token STRING '%s'\n", is->tokenbuf));
					return IMAP_TOK_STRING;
				}

				if (c == '\n' || c == '\r' || o>=oe) {
					if (o >= oe)
						printf("Protocol error: string too long\n");
					else
						printf("Protocol error: truncated string\n");
					goto protocol_error;
				} else {
					*o++ = c;
				}
			}
			is->ptr = p;
			if (stream_fill(is) == IMAP_TOK_ERROR)
				goto io_error;
			p = is->ptr;
			e = is->end;
		}
	} else {
		o = is->tokenptr;
		oe = is->tokenptr + CAMEL_IMAPP_STREAM_TOKEN - 1;
		digits = isdigit(c);
		*o++ = c;
		while (1) {
			while (p < e) {
				c = *p++;
				/*if (strchr(" \r\n*()[]+", c) != NULL) {*/
				if (imap_not_id(c)) {
					if (c == ' ' || c == '\r')
						is->ptr = p;
					else
						is->ptr = p-1;
					*o = 0;
					*data = is->tokenbuf;
					*len = o - is->tokenbuf;
					t(printf("token TOKEN '%s'\n", is->tokenbuf));
					return digits?IMAP_TOK_INT:IMAP_TOK_TOKEN;
				} else if (o < oe) {
					digits &= isdigit(c);
					*o++ = c;
				} else {
					printf("Protocol error: token too long\n");
					goto protocol_error;
				}
			}
			is->ptr = p;
			if (stream_fill(is) == IMAP_TOK_ERROR)
				goto io_error;
			p = is->ptr;
			e = is->end;
		}
	}

	/* Had an i/o erorr */
io_error:
	printf("Got io error\n");
	camel_exception_throw(1, "io error");
	return IMAP_TOK_ERROR;

	/* Protocol error, skip until next lf? */
protocol_error:
	printf("Got protocol error\n");

	if (c == '\n')
		is->ptr = p-1;
	else
		is->ptr = p;

	camel_exception_throw(1, "protocol error");
	return IMAP_TOK_PROTOCOL;
}

void
camel_imapp_stream_ungettoken(CamelIMAPPStream *is, camel_imapp_token_t tok, unsigned char *token, unsigned int len)
{
	/*printf("ungettoken: '%c' '%s'\n", tok, token);*/
	is->unget_tok = tok;
	is->unget_token = token;
	is->unget_len = len;
	is->unget++;
}

/* returns -1 on error, 0 if last lot of data, >0 if more remaining */
int camel_imapp_stream_gets(CamelIMAPPStream *is, unsigned char **start, unsigned int *len)
{
	int max;
	unsigned char *end;

	*len = 0;

	max = is->end - is->ptr;
	if (max == 0) {
		max = stream_fill(is);
		if (max <= 0)
			return max;
	}

	*start = is->ptr;
	end = memchr(is->ptr, '\n', max);
	if (end)
		max = (end - is->ptr) + 1;
	*start = is->ptr;
	*len = max;
	is->ptr += max;

	return end == NULL?1:0;
}

void camel_imapp_stream_set_literal(CamelIMAPPStream *is, unsigned int literal)
{
	is->literal = literal;
}

/* returns -1 on erorr, 0 if last data, >0 if more data left */
int camel_imapp_stream_getl(CamelIMAPPStream *is, unsigned char **start, unsigned int *len)
{
	int max;

	*len = 0;

	if (is->literal > 0) {
		max = is->end - is->ptr;
		if (max == 0) {
			max = stream_fill(is);
			if (max <= 0)
				return max;
		}

		max = MIN(max, is->literal);
		*start = is->ptr;
		*len = max;
		is->ptr += max;
		is->literal -= max;
	}

	if (is->literal > 0)
		return 1;

	return 0;
}
