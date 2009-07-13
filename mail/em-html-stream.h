/*
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef EM_HTML_STREAM_H
#define EM_HTML_STREAM_H

G_BEGIN_DECLS

#define EM_HTML_STREAM_TYPE     (em_html_stream_get_type ())
#define EM_HTML_STREAM(obj)     (CAMEL_CHECK_CAST((obj), EM_HTML_STREAM_TYPE, EMHTMLStream))
#define EM_HTML_STREAM_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), EM_HTML_STREAM_TYPE, EMHTMLStreamClass))
#define EM_IS_HTML_STREAM(o)    (CAMEL_CHECK_TYPE((o), EM_HTML_STREAM_TYPE))

#include "mail/em-sync-stream.h"

typedef struct _EMHTMLStream {
	EMSyncStream sync;

	guint destroy_id;
	GtkHTML *html;
	GtkHTMLStream *html_stream;
	GtkHTMLBeginFlags flags;
} EMHTMLStream;

typedef struct {
	EMSyncStreamClass parent_class;

} EMHTMLStreamClass;

CamelType    em_html_stream_get_type (void);

/* the html_stream is closed when we are finalised (with an error), or closed (ok) */
CamelStream *em_html_stream_new(GtkHTML *html, GtkHTMLStream *html_stream);
void em_html_stream_set_flags (EMHTMLStream *emhs, GtkHTMLBeginFlags flags);

G_END_DECLS

#endif /* EM_HTML_STREAM_H */
