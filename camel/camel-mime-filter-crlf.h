/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright (C) 2000 Ximian Inc.
 *
 *  Authors: Dan Winship <danw@ximian.com>
 *           Jeffrey Stedfast <fejj@ximian.com>
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


#ifndef _CAMEL_MIME_FILTER_CRLF_H
#define _CAMEL_MIME_FILTER_CRLF_H

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <camel/camel-mime-filter.h>

#define CAMEL_MIME_FILTER_CRLF_TYPE         (camel_mime_filter_crlf_get_type ())
#define CAMEL_MIME_FILTER_CRLF(obj)         CAMEL_CHECK_CAST (obj, CAMEL_MIME_FILTER_CRLF_TYPE, CamelMimeFilterCRLF)
#define CAMEL_MIME_FILTER_CRLF_CLASS(klass) CAMEL_CHECK_CLASS_CAST (klass, CAMEL_MIME_FILTER_CRLF_TYPE, CamelMimeFilterCRLFClass)
#define CAMEL_IS_MIME_FILTER_CRLF(obj)      CAMEL_CHECK_TYPE (obj, CAMEL_MIME_FILTER_CRLF_TYPE)

typedef struct _CamelMimeFilterCRLFClass CamelMimeFilterCRLFClass;

typedef enum {
	CAMEL_MIME_FILTER_CRLF_ENCODE,
	CAMEL_MIME_FILTER_CRLF_DECODE
} CamelMimeFilterCRLFDirection;

typedef enum {
	CAMEL_MIME_FILTER_CRLF_MODE_CRLF_DOTS,
	CAMEL_MIME_FILTER_CRLF_MODE_CRLF_ONLY,
} CamelMimeFilterCRLFMode;

struct _CamelMimeFilterCRLF {
	CamelMimeFilter parent;

	CamelMimeFilterCRLFDirection direction;
	CamelMimeFilterCRLFMode mode;
	gboolean saw_cr;
	gboolean saw_lf;
	gboolean saw_dot;
};

struct _CamelMimeFilterCRLFClass {
	CamelMimeFilterClass parent_class;
};

CamelType camel_mime_filter_crlf_get_type (void);

CamelMimeFilter *camel_mime_filter_crlf_new (CamelMimeFilterCRLFDirection direction, CamelMimeFilterCRLFMode mode);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ! _CAMEL_MIME_FILTER_CRLF_H */
