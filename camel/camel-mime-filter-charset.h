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

#ifndef _CAMEL_MIME_FILTER_CHARSET_H
#define _CAMEL_MIME_FILTER_CHARSET_H

#include <gtk/gtk.h>
#include <camel/camel-mime-filter.h>
#include <unicode.h>

#define CAMEL_MIME_FILTER_CHARSET(obj)         GTK_CHECK_CAST (obj, camel_mime_filter_charset_get_type (), CamelMimeFilterCharset)
#define CAMEL_MIME_FILTER_CHARSET_CLASS(klass) GTK_CHECK_CLASS_CAST (klass, camel_mime_filter_charset_get_type (), CamelMimeFilterCharsetClass)
#define IS_CAMEL_MIME_FILTER_CHARSET(obj)      GTK_CHECK_TYPE (obj, camel_mime_filter_charset_get_type ())

typedef struct _CamelMimeFilterCharset      CamelMimeFilterCharset;
typedef struct _CamelMimeFilterCharsetClass CamelMimeFilterCharsetClass;

struct _CamelMimeFilterCharset {
	CamelMimeFilter parent;

	struct _CamelMimeFilterCharsetPrivate *priv;

	unicode_iconv_t ic;
	char *from;
	char *to;
};

struct _CamelMimeFilterCharsetClass {
	CamelMimeFilterClass parent_class;
};

guint		camel_mime_filter_charset_get_type	(void);
CamelMimeFilterCharset      *camel_mime_filter_charset_new	(void);

CamelMimeFilterCharset      *camel_mime_filter_charset_new_convert	(const char *from_charset, const char *to_charset);

#endif /* ! _CAMEL_MIME_FILTER_CHARSET_H */
