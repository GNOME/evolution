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

#ifndef _CAMEL_MIME_FILTER_BASIC_H
#define _CAMEL_MIME_FILTER_BASIC_H

#include <camel/camel-mime-filter.h>

#define CAMEL_MIME_FILTER_BASIC(obj)         GTK_CHECK_CAST (obj, camel_mime_filter_basic_get_type (), CamelMimeFilterBasic)
#define CAMEL_MIME_FILTER_BASIC_CLASS(klass) GTK_CHECK_CLASS_CAST (klass, camel_mime_filter_basic_get_type (), CamelMimeFilterBasicClass)
#define IS_CAMEL_MIME_FILTER_BASIC(obj)      GTK_CHECK_TYPE (obj, camel_mime_filter_basic_get_type ())

typedef struct _CamelMimeFilterBasicClass CamelMimeFilterBasicClass;

typedef enum {
	CAMEL_MIME_FILTER_BASIC_BASE64_ENC = 1,
	CAMEL_MIME_FILTER_BASIC_BASE64_DEC,
	CAMEL_MIME_FILTER_BASIC_QP_ENC,
	CAMEL_MIME_FILTER_BASIC_QP_DEC,
} CamelMimeFilterBasicType;

struct _CamelMimeFilterBasic {
	CamelMimeFilter parent;

	struct _CamelMimeFilterBasicPrivate *priv;

	CamelMimeFilterBasicType type;

	int state;
	int save;
};

struct _CamelMimeFilterBasicClass {
	CamelMimeFilterClass parent_class;
};

guint		camel_mime_filter_basic_get_type	(void);
CamelMimeFilterBasic      *camel_mime_filter_basic_new	(void);
CamelMimeFilterBasic      *camel_mime_filter_basic_new_type	(CamelMimeFilterBasicType type);

#endif /* ! _CAMEL_MIME_FILTER_BASIC_H */
