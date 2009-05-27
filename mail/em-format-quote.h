/*
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

#ifndef EM_FORMAT_QUOTE_H
#define EM_FORMAT_QUOTE_H

#include <camel/camel-stream.h>
#include "mail/em-format.h"

/* Standard GObject macros */
#define EM_TYPE_FORMAT_QUOTE \
	(em_format_quote_get_type ())
#define EM_FORMAT_QUOTE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), EM_TYPE_FORMAT_QUOTE, EMFormatQuote))
#define EM_FORMAT_QUOTE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), EM_TYPE_FORMAT_QUOTE, EMFormatQuoteClass))
#define EM_IS_FORMAT_QUOTE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), EM_TYPE_FORMAT_QUOTE))
#define EM_IS_FORMAT_QUOTE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), EM_TYPE_FORMAT_QUOTE))
#define EM_FORMAT_QUOTE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), EM_TYPE_FORMAT_QUOTE, EMFormatQuoteClass))

#define EM_FORMAT_QUOTE_CITE (1<<0)
#define EM_FORMAT_QUOTE_HEADERS (1<<1)

G_BEGIN_DECLS

typedef struct _EMFormatQuote EMFormatQuote;
typedef struct _EMFormatQuoteClass EMFormatQuoteClass;
typedef struct _EMFormatQuotePrivate EMFormatQuotePrivate;

struct _EMFormatQuote {
	EMFormat format;

	EMFormatQuotePrivate *priv;

	gchar *credits;
	CamelStream *stream;
	guint32 flags;

	guint32 text_html_flags;
	guint32 citation_colour;
};

struct _EMFormatQuoteClass {
	EMFormatClass format_class;
};

GType		em_format_quote_get_type	(void);
EMFormatQuote *	em_format_quote_new		(const gchar *credits,
						 CamelStream *stream,
						 guint32 flags);

G_END_DECLS

#endif /* EM_FORMAT_QUOTE_H */
