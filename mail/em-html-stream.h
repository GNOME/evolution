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

#include <gtkhtml/gtkhtml.h>
#include <gtkhtml/gtkhtml-stream.h>
#include <mail/em-sync-stream.h>

/* Standard GObject macros */
#define EM_TYPE_HTML_STREAM \
	(em_html_stream_get_type ())
#define EM_HTML_STREAM(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), EM_TYPE_HTML_STREAM, EMHTMLStream))
#define EM_HTML_STREAM_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), EM_TYPE_HTML_STREAM, EMHTMLStreamClass))
#define EM_IS_HTML_STREAM(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), EM_TYPE_HTML_STREAM))
#define EM_IS_HTML_STREAM_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), EM_TYPE_HTML_STREAM))
#define EM_HTML_STREAM_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), EM_TYPE_HTML_STREAM, EMHTMLStreamClass))

G_BEGIN_DECLS

typedef struct _EMHTMLStream EMHTMLStream;
typedef struct _EMHTMLStreamClass EMHTMLStreamClass;

struct _EMHTMLStream {
	EMSyncStream sync;

	guint destroy_id;
	GtkHTML *html;
	GtkHTMLStream *html_stream;
	GtkHTMLBeginFlags flags;
};

struct _EMHTMLStreamClass {
	EMSyncStreamClass parent_class;

};

GType		em_html_stream_get_type		(void);
CamelStream *	em_html_stream_new		(GtkHTML *html,
						 GtkHTMLStream *html_stream);
void		em_html_stream_set_flags	(EMHTMLStream *emhs,
						 GtkHTMLBeginFlags flags);

G_END_DECLS

#endif /* EM_HTML_STREAM_H */
