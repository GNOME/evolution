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

#include <errno.h>

#include <string.h>
#include <stdio.h>

#include <glib.h>

#include "camel-nntp-stream.h"
#include "camel-debug.h"

#define dd(x) (camel_debug("nntp:stream")?(x):0)

static CamelObjectClass *parent_class = NULL;

/* Returns the class for a CamelStream */
#define CS_CLASS(so) CAMEL_NNTP_STREAM_CLASS(CAMEL_OBJECT_GET_CLASS(so))

#define CAMEL_NNTP_STREAM_SIZE (4096)
#define CAMEL_NNTP_STREAM_LINE_SIZE (1024) /* maximum line size */

static int
stream_fill(CamelNNTPStream *is)
{
	int left = 0;

	if (is->source) {
		left = is->end - is->ptr;
		memcpy(is->buf, is->ptr, left);
		is->end = is->buf + left;
		is->ptr = is->buf;
		left = camel_stream_read(is->source, is->end, CAMEL_NNTP_STREAM_SIZE - (is->end - is->buf));
		if (left > 0) {
			is->end += left;
			is->end[0] = '\n';
			return is->end - is->ptr;
		} else {
			if (left == 0)
				errno = ECONNRESET;
			dd(printf("NNTP_STREAM_FILL(ERROR): %d - '%s'\n", left, strerror(errno)));
			return -1;
		}
	}

	return 0;
}

static ssize_t
stream_read(CamelStream *stream, char *buffer, size_t n)
{
	CamelNNTPStream *is = (CamelNNTPStream *)stream;
	char *o, *oe;
	unsigned char *p, *e, c;
	int state;

	if (is->mode != CAMEL_NNTP_STREAM_DATA || n == 0)
		return 0;

	o = buffer;
	oe = buffer + n;
	state = is->state;

	/* Need to copy/strip '.'s and whatnot */
	p = is->ptr;
	e = is->end;

	switch(state) {
	state_0:
	case 0:		/* start of line, always read at least 3 chars */
		while (e - p < 3) {
			is->ptr = p;
			if (stream_fill(is) == -1)
				return -1;
			p = is->ptr;
			e = is->end;
		}
		if (p[0] == '.') {
			if (p[1] == '\r' && p[2] == '\n') {
				is->ptr = p+3;
				is->mode = CAMEL_NNTP_STREAM_EOD;
				is->state = 0;
				dd(printf("NNTP_STREAM_READ(%d):\n%.*s\n", o-buffer, o-buffer, buffer));
				return o-buffer;
			}
			p++;
		}
		state = 1;
		/* FALLS THROUGH */
	case 1:		/* looking for next sol */
		while (o < oe) {
			c = *p++;
			if (c == '\n') {
				/* end of input sentinal check */
				if (p > e) {
					is->ptr = e;
					if (stream_fill(is) == -1)
						return -1;
					p = is->ptr;
					e = is->end;
				} else {
					*o++ = '\n';
					state = 0;
					goto state_0;
				}
			} else if (c != '\r') {
				*o++ = c;
			}
		}
		break;
	}

	is->ptr = p;
	is->state = state;

	dd(printf("NNTP_STREAM_READ(%d):\n%.*s\n", o-buffer, o-buffer, buffer));

	return o-buffer;
}

static ssize_t
stream_write(CamelStream *stream, const char *buffer, size_t n)
{
	CamelNNTPStream *is = (CamelNNTPStream *)stream;

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
	CamelNNTPStream *is = (CamelNNTPStream *)stream;

	return is->mode != CAMEL_NNTP_STREAM_DATA;
}

static int
stream_reset(CamelStream *stream)
{
	/* nop?  reset literal mode? */
	return 0;
}

static void
camel_nntp_stream_class_init (CamelStreamClass *camel_nntp_stream_class)
{
	CamelStreamClass *camel_stream_class = (CamelStreamClass *)camel_nntp_stream_class;

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
camel_nntp_stream_init(CamelNNTPStream *is, CamelNNTPStreamClass *isclass)
{
	/* +1 is room for appending a 0 if we need to for a line */
	is->ptr = is->end = is->buf = g_malloc(CAMEL_NNTP_STREAM_SIZE+1);
	is->lineptr = is->linebuf = g_malloc(CAMEL_NNTP_STREAM_LINE_SIZE+1);
	is->lineend = is->linebuf + CAMEL_NNTP_STREAM_LINE_SIZE;

	/* init sentinal */
	is->ptr[0] = '\n';

	is->state = 0;
	is->mode = CAMEL_NNTP_STREAM_LINE;
}

static void
camel_nntp_stream_finalise(CamelNNTPStream *is)
{
	g_free(is->buf);
	g_free(is->linebuf);
	if (is->source)
		camel_object_unref((CamelObject *)is->source);
}

CamelType
camel_nntp_stream_get_type (void)
{
	static CamelType camel_nntp_stream_type = CAMEL_INVALID_TYPE;

	if (camel_nntp_stream_type == CAMEL_INVALID_TYPE) {
		camel_nntp_stream_type = camel_type_register( camel_stream_get_type(),
							    "CamelNNTPStream",
							    sizeof( CamelNNTPStream ),
							    sizeof( CamelNNTPStreamClass ),
							    (CamelObjectClassInitFunc) camel_nntp_stream_class_init,
							    NULL,
							    (CamelObjectInitFunc) camel_nntp_stream_init,
							    (CamelObjectFinalizeFunc) camel_nntp_stream_finalise );
	}

	return camel_nntp_stream_type;
}

/**
 * camel_nntp_stream_new:
 *
 * Returns a NULL stream.  A null stream is always at eof, and
 * always returns success for all reads and writes.
 *
 * Return value: the stream
 **/
CamelStream *
camel_nntp_stream_new(CamelStream *source)
{
	CamelNNTPStream *is;

	is = (CamelNNTPStream *)camel_object_new(camel_nntp_stream_get_type ());
	camel_object_ref((CamelObject *)source);
	is->source = source;

	return (CamelStream *)is;
}

/* Get one line from the nntp stream */
int
camel_nntp_stream_line(CamelNNTPStream *is, unsigned char **data, unsigned int *len)
{
	register unsigned char c, *p, *o, *oe;
	int newlen, oldlen;
	unsigned char *e;

	if (is->mode == CAMEL_NNTP_STREAM_EOD) {
		*data = is->linebuf;
		*len = 0;
		return 0;
	}

	o = is->linebuf;
	oe = is->lineend - 1;
	p = is->ptr;
	e = is->end;

	/* Data mode, convert leading '..' to '.', and stop when we reach a solitary '.' */
	if (is->mode == CAMEL_NNTP_STREAM_DATA) {
		/* need at least 3 chars in buffer */
		while (e-p < 3) {
			is->ptr = p;
			if (stream_fill(is) == -1)
				return -1;
			p = is->ptr;
			e = is->end;
		}

		/* check for isolated '.\r\n' or begging of line '.' */
		if (p[0] == '.') {
			if (p[1] == '\r' && p[2] == '\n') {
				is->ptr = p+3;
				is->mode = CAMEL_NNTP_STREAM_EOD;
				*data = is->linebuf;
				*len = 0;
				is->linebuf[0] = 0;

				dd(printf("NNTP_STREAM_LINE(END)\n"));

				return 0;
			}
			p++;
		}
	}

	while (1) {
		while (o < oe) {
			c = *p++;
			if (c == '\n') {
				/* sentinal? */
				if (p> e) {
					is->ptr = e;
					if (stream_fill(is) == -1)
						return -1;
					p = is->ptr;
					e = is->end;
				} else {
					is->ptr = p;
					*data = is->linebuf;
					*len = o - is->linebuf;
					*o = 0;

					dd(printf("NNTP_STREAM_LINE(%d): '%s'\n", *len, *data));

					return 1;
				}
			} else if (c != '\r') {
				*o++ = c;
			}
		}

		/* limit this for bad server data? */
		oldlen = o - is->linebuf;
		newlen = (is->lineend - is->linebuf) * 3 / 2;
		is->lineptr = is->linebuf = g_realloc(is->linebuf, newlen);
		is->lineend = is->linebuf + newlen;
		oe = is->lineend - 1;
		o = is->linebuf + oldlen;
	}

	return -1;
}

/* returns -1 on error, 0 if last lot of data, >0 if more remaining */
int camel_nntp_stream_gets(CamelNNTPStream *is, unsigned char **start, unsigned int *len)
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

	dd(printf("NNTP_STREAM_GETS(%s,%d): '%.*s'\n", end==NULL?"more":"last", *len, (int)*len, *start));

	return end == NULL?1:0;
}

void camel_nntp_stream_set_mode(CamelNNTPStream *is, camel_nntp_stream_mode_t mode)
{
	is->mode = mode;
}

/* returns -1 on erorr, 0 if last data, >0 if more data left */
int camel_nntp_stream_getd(CamelNNTPStream *is, unsigned char **start, unsigned int *len)
{
	unsigned char *p, *e, *s;
	int state;

	*len = 0;

	if (is->mode == CAMEL_NNTP_STREAM_EOD)
		return 0;

	if (is->mode == CAMEL_NNTP_STREAM_LINE) {
		g_warning("nntp_stream reading data in line mode\n");
		return 0;
	}

	state = is->state;
	p = is->ptr;
	e = is->end;

	while (e - p < 3) {
		is->ptr = p;
		if (stream_fill(is) == -1)
			return -1;
		p = is->ptr;
		e = is->end;
	}

	s = p;

	do {
		switch(state) {
		case 0:
			/* check leading '.', ... */
			if (p[0] == '.') {
				if (p[1] == '\r' && p[2] == '\n') {
					is->ptr = p+3;
					*len = p-s;
					*start = s;
					is->mode = CAMEL_NNTP_STREAM_EOD;
					is->state = 0;

					dd(printf("NNTP_STREAM_GETD(%s,%d): '%.*s'\n", "last", *len, (int)*len, *start));

					return 0;
				}

				/* If at start, just skip '.', else return data upto '.' but skip it */
				if (p == s) {
					s++;
					p++;
				} else {
					is->ptr = p+1;
					*len = p-s;
					*start = s;
					is->state = 1;

					dd(printf("NNTP_STREAM_GETD(%s,%d): '%.*s'\n", "more", *len, (int)*len, *start));

					return 1;
				}
			}
			state = 1;
		case 1:
			/* Scan for sentinal */
			while ((*p++)!='\n')
				;

			if (p > e) {
				p = e;
			} else {
				state = 0;
			}
			break;
		}
	} while ((e-p) >= 3);

	is->state = state;
	is->ptr = p;
	*len = p-s;
	*start = s;

	dd(printf("NNTP_STREAM_GETD(%s,%d): '%.*s'\n", "more", *len, (int)*len, *start));
	return 1;
}

