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


#ifndef _CAMEL_STREAM_NULL_H
#define _CAMEL_STREAM_NULL_H

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <camel/camel-stream.h>

#define CAMEL_STREAM_NULL(obj)         CAMEL_CHECK_CAST (obj, camel_stream_null_get_type (), CamelStreamNull)
#define CAMEL_STREAM_NULL_CLASS(klass) CAMEL_CHECK_CLASS_CAST (klass, camel_stream_null_get_type (), CamelStreamNullClass)
#define CAMEL_IS_STREAM_NULL(obj)      CAMEL_CHECK_TYPE (obj, camel_stream_null_get_type ())

typedef struct _CamelStreamNullClass CamelStreamNullClass;

struct _CamelStreamNull {
	CamelStream parent;
	
	size_t written;
};

struct _CamelStreamNullClass {
	CamelStreamClass parent_class;
};

CamelType			camel_stream_null_get_type	(void);

CamelStream            *camel_stream_null_new		(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ! _CAMEL_STREAM_NULL_H */
