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

#ifndef _CAMEL_STREAM_FILTER_H
#define _CAMEL_STREAM_FILTER_H

#include <gtk/gtk.h>

#include <camel/camel-stream.h>
#include <camel/camel-mime-filter.h>

#define CAMEL_STREAM_FILTER(obj)         GTK_CHECK_CAST (obj, camel_stream_filter_get_type (), CamelStreamFilter)
#define CAMEL_STREAM_FILTER_CLASS(klass) GTK_CHECK_CLASS_CAST (klass, camel_stream_filter_get_type (), CamelStreamFilterClass)
#define IS_CAMEL_STREAM_FILTER(obj)      GTK_CHECK_TYPE (obj, camel_stream_filter_get_type ())

typedef struct _CamelStreamFilter      CamelStreamFilter;
typedef struct _CamelStreamFilterClass CamelStreamFilterClass;

struct _CamelStreamFilter {
	CamelStream parent;

	CamelStream *source;

	struct _CamelStreamFilterPrivate *priv;
};

struct _CamelStreamFilterClass {
	CamelStreamClass parent_class;
};

guint			camel_stream_filter_get_type	(void);

CamelStreamFilter      *camel_stream_filter_new_with_stream	(CamelStream *stream);

int camel_stream_filter_add	(CamelStreamFilter *filter, CamelMimeFilter *);
void camel_stream_filter_remove	(CamelStreamFilter *filter, int id);

#endif /* ! _CAMEL_STREAM_FILTER_H */
