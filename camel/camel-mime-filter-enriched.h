/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002 Ximian, Inc. (www.ximian.com)
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


#ifndef __CAMEL_MIME_FILTER_ENRICHED_H__
#define __CAMEL_MIME_FILTER_ENRICHED_H__

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <camel/camel-mime-filter.h>

#define CAMEL_TYPE_MIME_FILTER_ENRICHED         (camel_mime_filter_enriched_get_type ())
#define CAMEL_MIME_FILTER_ENRICHED(obj)         (CAMEL_CHECK_CAST (obj, CAMEL_TYPE_MIME_FILTER_ENRICHED, CamelMimeFilterEnriched))
#define CAMEL_MIME_FILTER_ENRICHED_CLASS(klass) (CAMEL_CHECK_CLASS_CAST (klass, CAMEL_TYPE_MIME_FILTER_ENRICHED, CamelMimeFilterEnrichedClass))
#define CAMEL_IS_MIME_FILTER_ENRICHED(obj)      (CAMEL_CHECK_TYPE (obj, CAMEL_TYPE_MIME_FILTER_ENRICHED))


#define CAMEL_MIME_FILTER_ENRICHED_IS_RICHTEXT  (1 << 0)


typedef struct _CamelMimeFilterEnriched CamelMimeFilterEnriched;
typedef struct _CamelMimeFilterEnrichedClass CamelMimeFilterEnrichedClass;

struct _CamelMimeFilterEnriched {
	CamelMimeFilter parent_object;
	
	guint32 flags;
	int nofill;
};

struct _CamelMimeFilterEnrichedClass {
	CamelMimeFilterClass parent_class;
	
};

CamelType        camel_mime_filter_enriched_get_type (void);

CamelMimeFilter *camel_mime_filter_enriched_new (guint32 flags);
char *camel_enriched_to_html(const char *in, guint32 flags);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __CAMEL_MIME_FILTER_ENRICHED_H__ */
