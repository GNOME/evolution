/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright (C) 2000 Ximian Inc.
 *
 *  Authors: Dan Winship <danw@ximian.com>
 *           Jeffrey Stedfast <fejj@ximian.com>
 *           Peter Williams <peterw@ximian.com>
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

#ifndef _CAMEL_MIME_FILTER_STRIPHEADER_H
#define _CAMEL_MIME_FILTER_STRIPHEADER_H

#include <camel/camel-mime-filter.h>

#define CAMEL_MIME_FILTER_STRIPHEADER_TYPE         (camel_mime_filter_stripheader_get_type ())
#define CAMEL_MIME_FILTER_STRIPHEADER(obj)         CAMEL_CHECK_CAST (obj, CAMEL_MIME_FILTER_STRIPHEADER_TYPE, CamelMimeFilterStripHeader)
#define CAMEL_MIME_FILTER_STRIPHEADER_CLASS(klass) CAMEL_CHECK_CLASS_CAST (klass, CAMEL_MIME_FILTER_STRIPHEADER_TYPE, CamelMimeFilterStripHeaderClass)
#define CAMEL_IS_MIME_FILTER_STRIPHEADER(obj)      CAMEL_CHECK_TYPE (obj, CAMEL_MIME_FILTER_STRIPHEADER_TYPE)

typedef struct _CamelMimeFilterStripHeader CamelMimeFilterStripHeader;
typedef struct _CamelMimeFilterStripHeaderClass CamelMimeFilterStripHeaderClass;

struct _CamelMimeFilterStripHeader {
	CamelMimeFilter parent;

	gchar *header;
	int header_len;
	gboolean seen_eoh; /* end of headers */
	gboolean in_header;
};

struct _CamelMimeFilterStripHeaderClass {
	CamelMimeFilterClass parent_class;
};

CamelType camel_mime_filter_stripheader_get_type (void);

CamelMimeFilter *camel_mime_filter_stripheader_new (const gchar *header);

#endif /* ! _CAMEL_MIME_FILTER_STRIPHEADER_H */
