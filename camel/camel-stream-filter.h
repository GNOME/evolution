/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright (C) 2000 Ximian Inc.
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


#ifndef _CAMEL_STREAM_FILTER_H
#define _CAMEL_STREAM_FILTER_H

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <camel/camel-stream.h>
#include <camel/camel-mime-filter.h>

#define CAMEL_STREAM_FILTER(obj)         CAMEL_CHECK_CAST (obj, camel_stream_filter_get_type (), CamelStreamFilter)
#define CAMEL_STREAM_FILTER_CLASS(klass) CAMEL_CHECK_CLASS_CAST (klass, camel_stream_filter_get_type (), CamelStreamFilterClass)
#define CAMEL_IS_STREAM_FILTER(obj)      CAMEL_CHECK_TYPE (obj, camel_stream_filter_get_type ())

typedef struct _CamelStreamFilterClass CamelStreamFilterClass;

struct _CamelStreamFilter {
	CamelStream parent;

	CamelStream *source;

	struct _CamelStreamFilterPrivate *priv;
};

struct _CamelStreamFilterClass {
	CamelStreamClass parent_class;
};

CamelType			camel_stream_filter_get_type	(void);

CamelStreamFilter      *camel_stream_filter_new_with_stream	(CamelStream *stream);

int camel_stream_filter_add	(CamelStreamFilter *filter, CamelMimeFilter *);
void camel_stream_filter_remove	(CamelStreamFilter *filter, int id);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ! _CAMEL_STREAM_FILTER_H */
