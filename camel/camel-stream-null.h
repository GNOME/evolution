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

#ifndef _CAMEL_STREAM_NULL_H
#define _CAMEL_STREAM_NULL_H

#include <camel/camel-stream.h>

#define CAMEL_STREAM_NULL(obj)         CAMEL_CHECK_CAST (obj, camel_stream_null_get_type (), CamelStreamNull)
#define CAMEL_STREAM_NULL_CLASS(klass) CAMEL_CHECK_CLASS_CAST (klass, camel_stream_null_get_type (), CamelStreamNullClass)
#define CAMEL_IS_STREAM_NULL(obj)      CAMEL_CHECK_TYPE (obj, camel_stream_null_get_type ())

typedef struct _CamelStreamNullClass CamelStreamNullClass;

struct _CamelStreamNull {
	CamelStream parent;
};

struct _CamelStreamNullClass {
	CamelStreamClass parent_class;
};

guint			camel_stream_null_get_type	(void);

CamelStream            *camel_stream_null_new		(void);

#endif /* ! _CAMEL_STREAM_NULL_H */
