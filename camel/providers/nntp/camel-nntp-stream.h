/*
 * Copyright (C) 2001 Ximian Inc.
 *
 * Authors: Michael Zucchi <notzed@ximian.com>
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
 */

#ifndef _CAMEL_NNTP_STREAM_H
#define _CAMEL_NNTP_STREAM_H

#include <camel/camel-stream.h>

#define CAMEL_NNTP_STREAM(obj)         CAMEL_CHECK_CAST (obj, camel_nntp_stream_get_type (), CamelNNTPStream)
#define CAMEL_NNTP_STREAM_CLASS(klass) CAMEL_CHECK_CLASS_CAST (klass, camel_nntp_stream_get_type (), CamelNNTPStreamClass)
#define CAMEL_IS_NNTP_STREAM(obj)      CAMEL_CHECK_TYPE (obj, camel_nntp_stream_get_type ())

typedef struct _CamelNNTPStreamClass CamelNNTPStreamClass;
typedef struct _CamelNNTPStream CamelNNTPStream;

typedef enum {
	CAMEL_NNTP_STREAM_LINE,
	CAMEL_NNTP_STREAM_DATA,
	CAMEL_NNTP_STREAM_EOD,	/* end of data, acts as if end of stream */
} camel_nntp_stream_mode_t;

struct _CamelNNTPStream {
	CamelStream parent;

	CamelStream *source;

	camel_nntp_stream_mode_t mode;
	int state;

	unsigned char *buf, *ptr, *end;
	unsigned char *linebuf, *lineptr, *lineend;
};

struct _CamelNNTPStreamClass {
	CamelStreamClass parent_class;
};

CamelType		 camel_nntp_stream_get_type	(void);

CamelStream     *camel_nntp_stream_new		(CamelStream *source);


void		 camel_nntp_stream_set_mode     (CamelNNTPStream *is, camel_nntp_stream_mode_t mode);

int              camel_nntp_stream_line		(CamelNNTPStream *is, unsigned char **data, unsigned int *len);
int 		 camel_nntp_stream_gets		(CamelNNTPStream *is, unsigned char **start, unsigned int *len);
int 		 camel_nntp_stream_getd		(CamelNNTPStream *is, unsigned char **start, unsigned int *len);

#endif /* ! _CAMEL_NNTP_STREAM_H */
