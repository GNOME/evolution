/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
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
 *
 */

#ifndef EM_HTML_STREAM_H
#define EM_HTML_STREAM_H

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define EM_HTML_STREAM_TYPE     (em_html_stream_get_type ())
#define EM_HTML_STREAM(obj)     (CAMEL_CHECK_CAST((obj), EM_HTML_STREAM_TYPE, EMHTMLStream))
#define EM_HTML_STREAM_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), EM_HTML_STREAM_TYPE, EMHTMLStreamClass))
#define EM_IS_HTML_STREAM(o)    (CAMEL_CHECK_TYPE((o), EM_HTML_STREAM_TYPE))

struct _GtkHTML;
struct _GtkHTMLStream;

#include "em-sync-stream.h"

typedef struct _EMHTMLStream {
	EMSyncStream sync;

	guint destroy_id;
	struct _GtkHTML *html;
	struct _GtkHTMLStream *html_stream;
	GtkHTMLBeginFlags flags;
} EMHTMLStream;

typedef struct {
	EMSyncStreamClass parent_class;
	
} EMHTMLStreamClass;


CamelType    em_html_stream_get_type (void);

/* the html_stream is closed when we are finalised (with an error), or closed (ok) */
CamelStream *em_html_stream_new(struct _GtkHTML *html, struct _GtkHTMLStream *html_stream);
void em_html_stream_set_flags (EMHTMLStream *emhs, GtkHTMLBeginFlags flags);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* EM_HTML_STREAM_H */
