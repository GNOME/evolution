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
 *		Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef EM_INLINE_FILTER_H
#define EM_INLINE_FILTER_H

#include <camel/camel.h>

/* Standard GObject macros */
#define EM_TYPE_INLINE_FILTER \
	(em_inline_filter_get_type ())
#define EM_INLINE_FILTER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), EM_TYPE_INLINE_FILTER, EMInlineFilter))
#define EM_INLINE_FILTER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), EM_TYPE_INLINE_FILTER, EMInlineFilterClass))
#define EM_IS_INLINE_FILTER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), EM_TYPE_INLINE_FILTER))
#define EM_IS_INLINE_FILTER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), EM_TYPE_INLINE_FILTER))
#define EM_INLINE_FILTER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), EM_TYPE_INLINE_FILTER, EMInlineFilterClass))

G_BEGIN_DECLS

typedef struct _EMInlineFilter EMInlineFilter;
typedef struct _EMInlineFilterClass EMInlineFilterClass;

struct _EMInlineFilter {
	CamelMimeFilter filter;

	gint state;

	CamelTransferEncoding base_encoding;
	CamelContentType *base_type;

	GByteArray *data;
	gchar *filename;
	GSList *parts;
};

struct _EMInlineFilterClass {
	CamelMimeFilterClass filter_class;
};

GType		em_inline_filter_get_type	(void);
EMInlineFilter *em_inline_filter_new		(CamelTransferEncoding base_encoding,
						 CamelContentType *type);
CamelMultipart *em_inline_filter_get_multipart	(EMInlineFilter *emif);

G_END_DECLS

#endif /* EM_INLINE_FILTER_H */
