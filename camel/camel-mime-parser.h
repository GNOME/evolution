/*
 *  Copyright (C) 2000 Helix Code Inc.
 *
 *  Authors: Michael Zucchi <notzed@helixcode.com>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License
 *  as published by the Free Software Foundation; either version 2 of
 *  the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef _CAMEL_MIME_PARSER_H
#define _CAMEL_MIME_PARSER_H

#include <gtk/gtk.h>

#include <camel/camel-mime-utils.h>
#include <camel/camel-mime-filter.h>
#include <camel/camel-stream.h>

#define CAMEL_MIME_PARSER(obj)         GTK_CHECK_CAST (obj, camel_mime_parser_get_type (), CamelMimeParser)
#define CAMEL_MIME_PARSER_CLASS(klass) GTK_CHECK_CLASS_CAST (klass, camel_mime_parser_get_type (), CamelMimeParserClass)
#define IS_CAMEL_MIME_PARSER(obj)      GTK_CHECK_TYPE (obj, camel_mime_parser_get_type ())

typedef struct _CamelMimeParser      CamelMimeParser;
typedef struct _CamelMimeParserClass CamelMimeParserClass;

/* NOTE: if you add more states, you may need to bump the
   start of the END tags to 16 or 32, etc - so they are
   the same as the matching start tag, with a bit difference */
enum _header_state {
	HSCAN_INITIAL,
	HSCAN_FROM,		/* got 'From' line */
	HSCAN_HEADER,		/* toplevel header */
	HSCAN_BODY,		/* scanning body of message */
	HSCAN_MULTIPART,	/* got multipart header */
	HSCAN_MESSAGE,		/* rfc822 message */

	HSCAN_PART,		/* part of a multipart */

	HSCAN_END = 8,		/* bit mask for 'end' flags */

	HSCAN_EOF = 8,		/* end of file */
	HSCAN_FROM_END,		/* end of whole from bracket */
	HSCAN_HEADER_END,	/* dummy value */
	HSCAN_BODY_END,		/* end of message */
	HSCAN_MULTIPART_END,	/* end of multipart  */
	HSCAN_MESSAGE_END,	/* end of message */

};

struct _CamelMimeParser {
	GtkObject parent;

	struct _CamelMimeParserPrivate *priv;
};

struct _CamelMimeParserClass {
	GtkObjectClass parent_class;

	void (*message)(CamelMimeParser *, void *headers);
	void (*part)(CamelMimeParser *);
	void (*content)(CamelMimeParser *);
};

guint		camel_mime_parser_get_type	(void);
CamelMimeParser      *camel_mime_parser_new	(void);

/* using an fd will be a little faster, but not much (over a simple stream) */
int		camel_mime_parser_init_with_fd(CamelMimeParser *, int fd);
int		camel_mime_parser_init_with_stream(CamelMimeParser *m, CamelStream *stream);

/* scan 'From' separators? */
void camel_mime_parser_scan_from(CamelMimeParser *, int);

/* normal interface */
enum _header_state camel_mime_parser_step(CamelMimeParser *, char **, int *);

/* get content type for the current part/header */
struct _header_content_type *camel_mime_parser_content_type(CamelMimeParser *);

/* get a raw header by name */
const char *camel_mime_parser_header(CamelMimeParser *, const char *, int *offset);
/* get all raw headers */
struct _header_raw *camel_mime_parser_headers_raw(CamelMimeParser *);

/* add a processing filter for body contents */
int camel_mime_parser_filter_add(CamelMimeParser *, CamelMimeFilter *);
void camel_mime_parser_filter_remove(CamelMimeParser *, int);

/* these should be used with caution, because the state will not
   track the seeked position */
/* FIXME: something to bootstrap the state? */
off_t camel_mime_parser_tell(CamelMimeParser *);
off_t camel_mime_parser_seek(CamelMimeParser *, off_t, int);

off_t camel_mime_parser_tell_start_headers(CamelMimeParser *);
off_t camel_mime_parser_tell_start_from(CamelMimeParser *);

#endif /* ! _CAMEL_MIME_PARSER_H */
