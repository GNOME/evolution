/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#ifndef __CAMEL_MIME_FILTER_TOHTML_H__
#define __CAMEL_MIME_FILTER_TOHTML_H__

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <camel/camel-mime-filter.h>

#define CAMEL_MIME_FILTER_TOHTML(obj)         CAMEL_CHECK_CAST (obj, camel_mime_filter_tohtml_get_type (), CamelMimeFilterToHTML)
#define CAMEL_MIME_FILTER_TOHTML_CLASS(klass) CAMEL_CHECK_CLASS_CAST (klass, camel_mime_filter_tohtml_get_type (), CamelMimeFilterToHTMLClass)
#define CAMEL_IS_MIME_FILTER_TOHTML(obj)      CAMEL_CHECK_TYPE (obj, camel_mime_filter_tohtml_get_type ())

#define CAMEL_MIME_FILTER_TOHTML_PRE               (1 << 0)
#define CAMEL_MIME_FILTER_TOHTML_CONVERT_NL        (1 << 1)
#define CAMEL_MIME_FILTER_TOHTML_CONVERT_SPACES    (1 << 2)
#define CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS      (1 << 3)
#define CAMEL_MIME_FILTER_TOHTML_MARK_CITATION     (1 << 4)
#define CAMEL_MIME_FILTER_TOHTML_CONVERT_ADDRESSES (1 << 5)
#define CAMEL_MIME_FILTER_TOHTML_ESCAPE_8BIT       (1 << 6)
#define CAMEL_MIME_FILTER_TOHTML_CITE              (1 << 7)
#define CAMEL_MIME_FILTER_TOHTML_PRESERVE_8BIT     (1 << 8)

typedef struct _CamelMimeFilterToHTMLClass CamelMimeFilterToHTMLClass;
typedef struct _CamelMimeFilterToHTML CamelMimeFilterToHTML;

struct _CamelMimeFilterToHTML {
	CamelMimeFilter parent;
	
	guint32 flags;
	guint32 colour;
	
	guint32 column       : 29;
	guint32 pre_open     : 1;
	guint32 saw_citation : 1;
	guint32 coloured     : 1;
};

struct _CamelMimeFilterToHTMLClass {
	CamelMimeFilterClass parent_class;
};


CamelType        camel_mime_filter_tohtml_get_type (void);

CamelMimeFilter *camel_mime_filter_tohtml_new (guint32 flags, guint32 colour);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __CAMEL_MIME_FILTER_TOHTML_H__ */
