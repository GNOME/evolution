/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright (C) 2000-2003 Ximian Inc.
 *
 *  Authors: Michael Zucchi <notzed@ximian.com>
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


#ifndef _CAMEL_MIME_PARSER_H
#define _CAMEL_MIME_PARSER_H

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <camel/camel-object.h>

#include <camel/camel-mime-utils.h>
#include <camel/camel-mime-filter.h>
#include <camel/camel-stream.h>

#define CAMEL_MIME_PARSER(obj)         CAMEL_CHECK_CAST (obj, camel_mime_parser_get_type (), CamelMimeParser)
#define CAMEL_MIME_PARSER_CLASS(klass) CAMEL_CHECK_CLASS_CAST (klass, camel_mime_parser_get_type (), CamelMimeParserClass)
#define CAMEL_IS_MIME_PARSER(obj)      CAMEL_CHECK_TYPE (obj, camel_mime_parser_get_type ())

typedef struct _CamelMimeParserClass CamelMimeParserClass;

/* NOTE: if you add more states, you may need to bump the
   start of the END tags to 16 or 32, etc - so they are
   the same as the matching start tag, with a bit difference */
enum _camel_mime_parser_state {
	CAMEL_MIME_PARSER_STATE_INITIAL,
	CAMEL_MIME_PARSER_STATE_PRE_FROM,       /* data before a 'From' line */
	CAMEL_MIME_PARSER_STATE_FROM,           /* got 'From' line */
	CAMEL_MIME_PARSER_STATE_HEADER,         /* toplevel header */
	CAMEL_MIME_PARSER_STATE_BODY,           /* scanning body of message */
	CAMEL_MIME_PARSER_STATE_MULTIPART,      /* got multipart header */
	CAMEL_MIME_PARSER_STATE_MESSAGE,        /* rfc822 message */
	
	CAMEL_MIME_PARSER_STATE_PART,           /* part of a multipart */
	
	CAMEL_MIME_PARSER_STATE_END = 8,        /* bit mask for 'end' flags */
	
	CAMEL_MIME_PARSER_STATE_EOF = 8,        /* end of file */
	CAMEL_MIME_PARSER_STATE_PRE_FROM_END,   /* pre from end */
	CAMEL_MIME_PARSER_STATE_FROM_END,       /* end of whole from bracket */
	CAMEL_MIME_PARSER_STATE_HEADER_END,     /* dummy value */
	CAMEL_MIME_PARSER_STATE_BODY_END,       /* end of message */
	CAMEL_MIME_PARSER_STATE_MULTIPART_END,  /* end of multipart  */
	CAMEL_MIME_PARSER_STATE_MESSAGE_END,    /* end of message */
};

struct _CamelMimeParser {
	CamelObject parent;

	struct _CamelMimeParserPrivate *priv;
};

struct _CamelMimeParserClass {
	CamelObjectClass parent_class;

	void (*message) (CamelMimeParser *parser, void *headers);
	void (*part) (CamelMimeParser *parser);
	void (*content) (CamelMimeParser *parser);
};

CamelType camel_mime_parser_get_type (void);
CamelMimeParser *camel_mime_parser_new (void);

/* quick-fix for parser not erroring, we can find out if it had an error afterwards */
int		camel_mime_parser_errno (CamelMimeParser *parser);

/* using an fd will be a little faster, but not much (over a simple stream) */
int		camel_mime_parser_init_with_fd (CamelMimeParser *parser, int fd);
int		camel_mime_parser_init_with_stream (CamelMimeParser *parser, CamelStream *stream);

/* get the stream or fd back of the parser */
CamelStream    *camel_mime_parser_stream (CamelMimeParser *parser);
int		camel_mime_parser_fd (CamelMimeParser *parser);

/* scan 'From' separators? */
void camel_mime_parser_scan_from (CamelMimeParser *parser, gboolean scan_from);
/* Do we want to know about the pre-from data? */
void camel_mime_parser_scan_pre_from (CamelMimeParser *parser, gboolean scan_pre_from);

/* what headers to save, MUST include ^Content-Type: */
int camel_mime_parser_set_header_regex (CamelMimeParser *parser, char *matchstr);

/* normal interface */
enum _camel_mime_parser_state camel_mime_parser_step (CamelMimeParser *parser, char **buf, size_t *buflen);
void camel_mime_parser_unstep (CamelMimeParser *parser);
void camel_mime_parser_drop_step (CamelMimeParser *parser);
enum _camel_mime_parser_state camel_mime_parser_state (CamelMimeParser *parser);
void camel_mime_parser_push_state(CamelMimeParser *mp, enum _camel_mime_parser_state newstate, const char *boundary);

/* read through the parser */
int camel_mime_parser_read (CamelMimeParser *parser, const char **databuffer, int len);

/* get content type for the current part/header */
CamelContentType *camel_mime_parser_content_type (CamelMimeParser *parser);

/* get/change raw header by name */
const char *camel_mime_parser_header (CamelMimeParser *parser, const char *name, int *offset);

/* get all raw headers. READ ONLY! */
struct _camel_header_raw *camel_mime_parser_headers_raw (CamelMimeParser *parser);

/* get multipart pre/postface */
const char *camel_mime_parser_preface (CamelMimeParser *parser);
const char *camel_mime_parser_postface (CamelMimeParser *parser);

/* return the from line content */
const char *camel_mime_parser_from_line (CamelMimeParser *parser);

/* add a processing filter for body contents */
int camel_mime_parser_filter_add (CamelMimeParser *parser, CamelMimeFilter *filter);
void camel_mime_parser_filter_remove (CamelMimeParser *parser, int id);

/* these should be used with caution, because the state will not
   track the seeked position */
/* FIXME: something to bootstrap the state? */
off_t camel_mime_parser_tell (CamelMimeParser *parser);
off_t camel_mime_parser_seek (CamelMimeParser *parser, off_t offset, int whence);

off_t camel_mime_parser_tell_start_headers (CamelMimeParser *parser);
off_t camel_mime_parser_tell_start_from (CamelMimeParser *parser);
off_t camel_mime_parser_tell_start_boundary(CamelMimeParser *parser);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ! _CAMEL_MIME_PARSER_H */
