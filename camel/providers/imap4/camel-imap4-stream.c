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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include "camel-imap4-specials.h"

#include "camel-imap4-stream.h"

#define d(x) x

#define IMAP4_TOKEN_LEN  128

static void camel_imap4_stream_class_init (CamelIMAP4StreamClass *klass);
static void camel_imap4_stream_init (CamelIMAP4Stream *stream, CamelIMAP4StreamClass *klass);
static void camel_imap4_stream_finalize (CamelObject *object);

static ssize_t stream_read (CamelStream *stream, char *buffer, size_t n);
static ssize_t stream_write (CamelStream *stream, const char *buffer, size_t n);
static int stream_flush  (CamelStream *stream);
static int stream_close  (CamelStream *stream);
static gboolean stream_eos (CamelStream *stream);


static CamelStreamClass *parent_class = NULL;


CamelType
camel_imap4_stream_get_type (void)
{
	static CamelType type = 0;
	
	if (!type) {
		type = camel_type_register (CAMEL_STREAM_TYPE,
					    "CamelIMAP4Stream",
					    sizeof (CamelIMAP4Stream),
					    sizeof (CamelIMAP4StreamClass),
					    (CamelObjectClassInitFunc) camel_imap4_stream_class_init,
					    NULL,
					    (CamelObjectInitFunc) camel_imap4_stream_init,
					    (CamelObjectFinalizeFunc) camel_imap4_stream_finalize);
	}
	
	return type;
}

static void
camel_imap4_stream_class_init (CamelIMAP4StreamClass *klass)
{
	CamelStreamClass *stream_class = (CamelStreamClass *) klass;
	
	parent_class = (CamelStreamClass *) camel_type_get_global_classfuncs (CAMEL_STREAM_TYPE);
	
	/* virtual method overload */
	stream_class->read = stream_read;
	stream_class->write = stream_write;
	stream_class->flush = stream_flush;
	stream_class->close = stream_close;
	stream_class->eos = stream_eos;
}

static void
camel_imap4_stream_init (CamelIMAP4Stream *imap4, CamelIMAP4StreamClass *klass)
{
	imap4->stream = NULL;
	
	imap4->mode = CAMEL_IMAP4_STREAM_MODE_TOKEN;
	imap4->disconnected = FALSE;
	imap4->have_unget = FALSE;
	imap4->eol = FALSE;
	
	imap4->literal = 0;
	
	imap4->inbuf = imap4->realbuf + IMAP4_READ_PRELEN;
	imap4->inptr = imap4->inbuf;
	imap4->inend = imap4->inbuf;
	
	imap4->tokenbuf = g_malloc (IMAP4_TOKEN_LEN);
	imap4->tokenptr = imap4->tokenbuf;
	imap4->tokenleft = IMAP4_TOKEN_LEN;
}

static void
camel_imap4_stream_finalize (CamelObject *object)
{
	CamelIMAP4Stream *imap4 = (CamelIMAP4Stream *) object;
	
	if (imap4->stream)
		camel_object_unref (imap4->stream);
	
	g_free (imap4->tokenbuf);
}


static ssize_t
imap4_fill (CamelIMAP4Stream *imap4)
{
	unsigned char *inbuf, *inptr, *inend;
	ssize_t nread;
	size_t inlen;
	
	if (imap4->disconnected) {
		errno = EINVAL;
		return -1;
	}
	
	inbuf = imap4->inbuf;
	inptr = imap4->inptr;
	inend = imap4->inend;
	inlen = inend - inptr;
	
	g_assert (inptr <= inend);
	
	/* attempt to align 'inend' with realbuf + SCAN_HEAD */
	if (inptr >= inbuf) {
		inbuf -= inlen < IMAP4_READ_PRELEN ? inlen : IMAP4_READ_PRELEN;
		memmove (inbuf, inptr, inlen);
		inptr = inbuf;
		inbuf += inlen;
	} else if (inptr > imap4->realbuf) {
		size_t shift;
		
		shift = MIN (inptr - imap4->realbuf, inend - inbuf);
		memmove (inptr - shift, inptr, inlen);
		inptr -= shift;
		inbuf = inptr + inlen;
	} else {
		/* we can't shift... */
		inbuf = inend;
	}
	
	imap4->inptr = inptr;
	imap4->inend = inbuf;
	inend = imap4->realbuf + IMAP4_READ_PRELEN + IMAP4_READ_BUFLEN - 1;
	
	if ((nread = camel_stream_read (imap4->stream, inbuf, inend - inbuf)) == -1)
		return -1;
	else if (nread == 0)
		imap4->disconnected = TRUE;
	
	imap4->inend += nread;
	
	return imap4->inend - imap4->inptr;
}

static ssize_t
stream_read (CamelStream *stream, char *buffer, size_t n)
{
	CamelIMAP4Stream *imap4 = (CamelIMAP4Stream *) stream;
	ssize_t len, nread = 0;
	
	if (imap4->mode == CAMEL_IMAP4_STREAM_MODE_LITERAL) {
		/* don't let our caller read past the end of the literal */
		n = MIN (n, imap4->literal);
	}
	
	if (imap4->inptr < imap4->inend) {
		len = MIN (n, imap4->inend - imap4->inptr);
		memcpy (buffer, imap4->inptr, len);
		imap4->inptr += len;
		nread = len;
	}
	
	if (nread < n) {
		if ((len = camel_stream_read (imap4->stream, buffer + nread, n - nread)) == 0)
			imap4->disconnected = TRUE;
		else if (len == -1)
			return -1;
		
		nread += len;
	}
	
	if (imap4->mode == CAMEL_IMAP4_STREAM_MODE_LITERAL) {
		imap4->literal -= nread;
		
		if (imap4->literal == 0) {
			imap4->mode = CAMEL_IMAP4_STREAM_MODE_TOKEN;
			imap4->eol = TRUE;
		}
	}
	
	return nread;
}

static ssize_t
stream_write (CamelStream *stream, const char *buffer, size_t n)
{
	CamelIMAP4Stream *imap4 = (CamelIMAP4Stream *) stream;
	ssize_t nwritten;
	
	if (imap4->disconnected) {
		errno = EINVAL;
		return -1;
	}
	
	if ((nwritten = camel_stream_write (imap4->stream, buffer, n)) == 0)
		imap4->disconnected = TRUE;
	
	return nwritten;
}

static int
stream_flush (CamelStream *stream)
{
	CamelIMAP4Stream *imap4 = (CamelIMAP4Stream *) stream;
	
	return camel_stream_flush (imap4->stream);
}

static int
stream_close (CamelStream *stream)
{
	CamelIMAP4Stream *imap4 = (CamelIMAP4Stream *) stream;
	
	if (camel_stream_close (imap4->stream) == -1)
		return -1;
	
	camel_object_unref (imap4->stream);
	imap4->stream = NULL;
	
	imap4->disconnected = TRUE;
	
	return 0;
}

static gboolean
stream_eos (CamelStream *stream)
{
	CamelIMAP4Stream *imap4 = (CamelIMAP4Stream *) stream;
	
	if (imap4->eol)
		return TRUE;
	
	if (imap4->disconnected && imap4->inptr == imap4->inend)
		return TRUE;
	
	if (camel_stream_eos (imap4->stream))
		return TRUE;
	
	return FALSE;
}


/**
 * camel_imap4_stream_new:
 * @stream: tcp stream
 *
 * Returns a new imap4 stream
 **/
CamelStream *
camel_imap4_stream_new (CamelStream *stream)
{
	CamelIMAP4Stream *imap4;
	
	g_return_val_if_fail (CAMEL_IS_STREAM (stream), NULL);
	
	imap4 = (CamelIMAP4Stream *) camel_object_new (CAMEL_TYPE_IMAP4_STREAM);
	camel_object_ref (stream);
	imap4->stream = stream;
	
	return (CamelStream *) imap4;
}



#define token_save(imap4, start, len) G_STMT_START {                         \
	if (imap4->tokenleft <= len) {                                       \
		unsigned int tlen, toff;                                    \
		                                                            \
		tlen = toff = imap4->tokenptr - imap4->tokenbuf;              \
		tlen = tlen ? tlen : 1;                                     \
		                                                            \
		while (tlen < toff + len)                                   \
			tlen <<= 1;                                         \
		                                                            \
		imap4->tokenbuf = g_realloc (imap4->tokenbuf, tlen + 1);      \
		imap4->tokenptr = imap4->tokenbuf + toff;                     \
		imap4->tokenleft = tlen - toff;                              \
	}                                                                   \
	                                                                    \
	memcpy (imap4->tokenptr, start, len);                                \
	imap4->tokenptr += len;                                              \
	imap4->tokenleft -= len;                                             \
} G_STMT_END

#define token_clear(imap4) G_STMT_START {                                    \
	imap4->tokenleft += imap4->tokenptr - imap4->tokenbuf;                 \
	imap4->tokenptr = imap4->tokenbuf;                                    \
	imap4->literal = 0;                                                  \
} G_STMT_END


/**
 * camel_imap4_stream_next_token:
 * @stream: imap4 stream
 * @token: imap4 token
 *
 * Reads the next token from the imap4 stream and saves it in @token.
 *
 * Returns 0 on success or -1 on fail.
 **/
int
camel_imap4_stream_next_token (CamelIMAP4Stream *stream, camel_imap4_token_t *token)
{
	register unsigned char *inptr;
	unsigned char *inend, *start, *p;
	gboolean escaped = FALSE;
	size_t literal = 0;
	guint32 nz_number;
	int ret;
	
	g_return_val_if_fail (CAMEL_IS_IMAP4_STREAM (stream), -1);
	g_return_val_if_fail (stream->mode != CAMEL_IMAP4_STREAM_MODE_LITERAL, -1);
	g_return_val_if_fail (token != NULL, -1);
	
	if (stream->have_unget) {
		memcpy (token, &stream->unget, sizeof (camel_imap4_token_t));
		stream->have_unget = FALSE;
		return 0;
	}
	
	token_clear (stream);
	
	inptr = stream->inptr;
	inend = stream->inend;
	*inend = '\0';
	
	do {
		if (inptr == inend) {
			if ((ret = imap4_fill (stream)) < 0) {
				token->token = CAMEL_IMAP4_TOKEN_ERROR;
				return -1;
			} else if (ret == 0) {
				token->token = CAMEL_IMAP4_TOKEN_NO_DATA;
				return 0;
			}
			
			inptr = stream->inptr;
			inend = stream->inend;
			*inend = '\0';
		}
		
		while (*inptr == ' ' || *inptr == '\r')
			inptr++;
	} while (inptr == inend);
	
	do {
		if (inptr < inend) {
			if (*inptr == '"') {
				/* qstring token */
				escaped = FALSE;
				start = inptr;
				
				/* eat the beginning " */
				inptr++;
				
				p = inptr;
				while (inptr < inend) {
					if (*inptr == '"' && !escaped)
						break;
					
					if (*inptr == '\\' && !escaped) {
						token_save (stream, p, inptr - p);
						escaped = TRUE;
						inptr++;
						p = inptr;
					} else {
						inptr++;
						escaped = FALSE;
					}
				}
				
				token_save (stream, p, inptr - p);
				
				if (inptr == inend) {
					stream->inptr = start;
					goto refill;
				}
				
				/* eat the ending " */
				inptr++;
				
				/* nul-terminate the atom token */
				token_save (stream, "", 1);
				
				token->token = CAMEL_IMAP4_TOKEN_QSTRING;
				token->v.qstring = stream->tokenbuf;
				
				d(fprintf (stderr, "token: \"%s\"\n", token->v.qstring));
				
				break;
			} else if (strchr ("+*()[]\n", *inptr)) {
				/* special character token */
				token->token = *inptr++;
#if d(!)0
				if (token->token != '\n')
					fprintf (stderr, "token: %c\n", token->token);
				else
					fprintf (stderr, "token: \\n\n");
#endif
				break;
			} else if (*inptr == '{') {
				/* literal identifier token */
				if ((p = strchr (inptr, '}')) && strchr (p, '\n')) {
					inptr++;
					
					while (isdigit ((int) *inptr) && literal < UINT_MAX / 10)
						literal = (literal * 10) + (*inptr++ - '0');
					
					if (*inptr != '}') {
						if (isdigit ((int) *inptr))
							g_warning ("illegal literal identifier: literal too large");
						else if (*inptr != '+')
							g_warning ("illegal literal identifier: garbage following size");
						
						while (*inptr != '}')
							inptr++;
					}
					
					/* skip over '}' */
					inptr++;
					
					/* skip over any trailing whitespace */
					while (*inptr == ' ' || *inptr == '\r')
						inptr++;
					
					if (*inptr != '\n') {
						g_warning ("illegal token following literal identifier: %s", inptr);
						
						/* skip ahead to the eoln */
						inptr = strchr (inptr, '\n');
					}
					
					/* skip over '\n' */
					inptr++;
					
					token->token = CAMEL_IMAP4_TOKEN_LITERAL;
					token->v.literal = literal;
					
					d(fprintf (stderr, "token: {%u}\n", literal));
					
					stream->mode = CAMEL_IMAP4_STREAM_MODE_LITERAL;
					stream->literal = literal;
					stream->eol = FALSE;
					
					break;
				} else {
					stream->inptr = inptr;
					goto refill;
				}
			} else if (*inptr >= '0' && *inptr <= '9') {
				/* number token */
				*inend = '\0';
				nz_number = strtoul ((char *) inptr, (char **) &start, 10);
				if (start == inend)
					goto refill;
				
				if (*start == ':' || *start == ',') {
					/* workaround for 'set' tokens (APPENDUID / COPYUID) */
					goto atom_token;
				}
				
				inptr = start;
				token->token = CAMEL_IMAP4_TOKEN_NUMBER;
				token->v.number = nz_number;
				
				d(fprintf (stderr, "token: %u\n", nz_number));
				
				break;
			} else if (is_atom (*inptr)) {
			atom_token:
				/* simple atom token */
				start = inptr;
				
				while (inptr < inend && is_atom (*inptr))
					inptr++;
				
				if (inptr == inend) {
					stream->inptr = start;
					goto refill;
				}
				
				token_save (stream, start, inptr - start);
				
				/* nul-terminate the atom token */
				token_save (stream, "", 1);
				
				if (!strcmp (stream->tokenbuf, "NIL")) {
					/* special atom token */
					token->token = CAMEL_IMAP4_TOKEN_NIL;
					d(fprintf (stderr, "token: NIL\n"));
				} else {
					token->token = CAMEL_IMAP4_TOKEN_ATOM;
					token->v.atom = stream->tokenbuf;
					d(fprintf (stderr, "token: %s\n", token->v.atom));
				}
				
				break;
			} else if (*inptr == '\\') {
				/* possible flag token ("\" atom) */
				start = inptr++;
				
				while (inptr < inend && is_atom (*inptr))
					inptr++;
				
				if (inptr == inend) {
					stream->inptr = start;
					goto refill;
				}
				
				/* handle the \* case */
				if ((inptr - start) == 1 && *inptr == '*')
					inptr++;
				
				if ((inptr - start) > 1) {
					token_save (stream, start, inptr - start);
					
					/* nul-terminate the flag token */
					token_save (stream, "", 1);
					
					token->token = CAMEL_IMAP4_TOKEN_FLAG;
					token->v.atom = stream->tokenbuf;
					d(fprintf (stderr, "token: %s\n", token->v.atom));
				} else {
					token->token = '\\';
					d(fprintf (stderr, "token: %c\n", token->token));
				}
				break;
			} else if (is_lwsp (*inptr)) {
				inptr++;
			} else {
				/* unknown character token? */
				token->token = *inptr++;
				d(fprintf (stderr, "token: %c\n", token->token));
				break;
			}
		} else {
		refill:
			token_clear (stream);
			
			if (imap4_fill (stream) <= 0) {
				token->token = CAMEL_IMAP4_TOKEN_ERROR;
				return -1;
			}
			
			inptr = stream->inptr;
			inend = stream->inend;
			*inend = '\0';
		}
	} while (inptr < inend);
	
	stream->inptr = inptr;
	
	return 0;
}


/**
 * camel_imap4_stream_unget_token:
 * @stream: imap4 stream
 * @token: token to 'unget'
 *
 * Ungets an imap4 token (as in ungetc()).
 *
 * Note: you may *ONLY* unget a single token. Trying to unget another
 * token will fail.
 *
 * Returns 0 on success or -1 on fail.
 **/
int
camel_imap4_stream_unget_token (CamelIMAP4Stream *stream, camel_imap4_token_t *token)
{
	if (stream->have_unget)
		return -1;
	
	if (token->token != CAMEL_IMAP4_TOKEN_NO_DATA) {
		memcpy (&stream->unget, token, sizeof (camel_imap4_token_t));
		stream->have_unget = TRUE;
	}
	
	return 0;
}


/**
 * camel_imap4_stream_line:
 * @stream: imap4 stream
 * @line: line pointer
 * @len: line length
 *
 * Reads a single line from the imap4 stream and points @line at an
 * internal buffer containing the line read and sets @len to the
 * length of the line buffer.
 *
 * Returns -1 on error, 0 if the line read is complete, or 1 if the
 * read is incomplete.
 **/
int
camel_imap4_stream_line (CamelIMAP4Stream *stream, unsigned char **line, size_t *len)
{
	register unsigned char *inptr;
	unsigned char *inend;
	
	g_return_val_if_fail (CAMEL_IS_IMAP4_STREAM (stream), -1);
	g_return_val_if_fail (stream->mode != CAMEL_IMAP4_STREAM_MODE_LITERAL, -1);
	g_return_val_if_fail (line != NULL, -1);
	g_return_val_if_fail (len != NULL, -1);
	
	inptr = stream->inptr;
	inend = stream->inend;
	
	if (inptr == inend || ((inend - inptr) < 2 && *inptr != '\n')) {
		if (imap4_fill (stream) == -1 && stream->inptr == stream->inend)
			return -1;
	}
	
	*line = stream->inptr;
	inptr = stream->inptr;
	inend = stream->inend;
	*inend = '\n';
	
	while (*inptr != '\n')
		inptr++;
	
	*len = (inptr - stream->inptr);
	
	if (inptr > stream->inptr && inptr[-1] == '\r')
		inptr[-1] = '\0';
	
	if (inptr < inend) {
		/* got the eoln */
		inptr[0] = '\0';
		*len += 1;
		
		stream->inptr = inptr + 1;
		
		return 0;
	}
	
	stream->inptr = inptr;
	
	return 1;
}


/**
 * camel_imap4_stream_literal:
 * @stream: IMAP stream
 * @literal: literal pointer
 * @len: literal length
 *
 * Sets @literal to the beginning of the next chunk of the literal
 * buffer from the IMAP stream and sets @len to the length of the
 * @literal buffer.
 *
 * Returns >0 if more literal data exists, 0 if the end of the literal
 * has been reached or -1 on fail.
 **/
int
camel_imap4_stream_literal (CamelIMAP4Stream *stream, unsigned char **literal, size_t *len)
{
	unsigned char *inptr, *inend;
	size_t nread;
	
	g_return_val_if_fail (CAMEL_IS_IMAP4_STREAM (stream), -1);
	g_return_val_if_fail (stream->mode == CAMEL_IMAP4_STREAM_MODE_LITERAL, -1);
	g_return_val_if_fail (literal != NULL, -1);
	g_return_val_if_fail (len != NULL, -1);
	
	if (stream->eol) {
		*len = 0;
		return 0;
	}
	
	if ((stream->inend - stream->inptr) < 1) {
		/* keep our buffer full to the optimal size */
		if (imap4_fill (stream) == -1 && stream->inptr == stream->inend)
			return -1;
	}
	
	*literal = inptr = stream->inptr;
	inend = stream->inend;
	if ((inend - inptr) > stream->literal)
		inend = inptr + stream->literal;
	else
		inend = stream->inend;
	
	*len = nread = inend - inptr;
	
	stream->literal -= nread;
	stream->inptr += nread;
	
	if (stream->literal == 0) {
		stream->mode = CAMEL_IMAP4_STREAM_MODE_TOKEN;
		stream->eol = TRUE;
		return 0;
	}
	
	return 1;
}
