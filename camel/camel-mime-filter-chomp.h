/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright (C) 2002 Ximian Inc.
 *
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
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


#ifndef _CAMEL_MIME_FILTER_CHOMP_H
#define _CAMEL_MIME_FILTER_CHOMP_H

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <camel/camel-mime-filter.h>

#define CAMEL_MIME_FILTER_CHOMP_TYPE         (camel_mime_filter_chomp_get_type ())
#define CAMEL_MIME_FILTER_CHOMP(obj)         CAMEL_CHECK_CAST (obj, CAMEL_MIME_FILTER_CHOMP_TYPE, CamelMimeFilterChomp)
#define CAMEL_MIME_FILTER_CHOMP_CLASS(klass) CAMEL_CHECK_CLASS_CAST (klass, CAMEL_MIME_FILTER_CHOMP_TYPE, CamelMimeFilterChompClass)
#define CAMEL_IS_MIME_FILTER_CHOMP(obj)      CAMEL_CHECK_TYPE (obj, CAMEL_MIME_FILTER_CHOMP_TYPE)

typedef struct _CamelMimeFilterChompClass CamelMimeFilterChompClass;

struct _CamelMimeFilterChomp {
	CamelMimeFilter parent;
	
};

struct _CamelMimeFilterChompClass {
	CamelMimeFilterClass parent_class;
};

CamelType camel_mime_filter_chomp_get_type (void);

CamelMimeFilter *camel_mime_filter_chomp_new (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ! _CAMEL_MIME_FILTER_CHOMP_H */
