/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright (C) 2000-2003 Ximian Inc.
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

#ifndef _CAMEL_MIME_FILTER_BASIC_H
#define _CAMEL_MIME_FILTER_BASIC_H

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <camel/camel-mime-filter.h>

#define CAMEL_MIME_FILTER_BASIC(obj)         CAMEL_CHECK_CAST (obj, camel_mime_filter_basic_get_type (), CamelMimeFilterBasic)
#define CAMEL_MIME_FILTER_BASIC_CLASS(klass) CAMEL_CHECK_CLASS_CAST (klass, camel_mime_filter_basic_get_type (), CamelMimeFilterBasicClass)
#define CAMEL_IS_MIME_FILTER_BASIC(obj)      CAMEL_CHECK_TYPE (obj, camel_mime_filter_basic_get_type ())

typedef struct _CamelMimeFilterBasicClass CamelMimeFilterBasicClass;

typedef enum {
	CAMEL_MIME_FILTER_BASIC_BASE64_ENC = 1,
	CAMEL_MIME_FILTER_BASIC_BASE64_DEC,
	CAMEL_MIME_FILTER_BASIC_QP_ENC,
	CAMEL_MIME_FILTER_BASIC_QP_DEC,
	CAMEL_MIME_FILTER_BASIC_UU_ENC,
	CAMEL_MIME_FILTER_BASIC_UU_DEC,
} CamelMimeFilterBasicType;

struct _CamelMimeFilterBasic {
	CamelMimeFilter parent;

	struct _CamelMimeFilterBasicPrivate *priv;

	CamelMimeFilterBasicType type;
	
	unsigned char uubuf[60];
	int state;
	int save;
};

struct _CamelMimeFilterBasicClass {
	CamelMimeFilterClass parent_class;
};

CamelType		camel_mime_filter_basic_get_type	(void);
CamelMimeFilterBasic      *camel_mime_filter_basic_new	(void);
CamelMimeFilterBasic      *camel_mime_filter_basic_new_type	(CamelMimeFilterBasicType type);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ! _CAMEL_MIME_FILTER_BASIC_H */
