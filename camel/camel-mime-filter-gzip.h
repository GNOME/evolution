/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001-2004 Ximian, Inc. (www.ximian.com)
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


#ifndef __CAMEL_MIME_FILTER_GZIP_H__
#define __CAMEL_MIME_FILTER_GZIP_H__

#include <camel/camel-mime-filter.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define CAMEL_TYPE_MIME_FILTER_GZIP            (camel_mime_filter_gzip_get_type ())
#define CAMEL_MIME_FILTER_GZIP(obj)            (CAMEL_CHECK_CAST ((obj), CAMEL_TYPE_MIME_FILTER_GZIP, CamelMimeFilterGZip))
#define CAMEL_MIME_FILTER_GZIP_CLASS(klass)    (CAMEL_CHECK_CLASS_CAST ((klass), CAMEL_TYPE_MIME_FILTER_GZIP, CamelMimeFilterGZipClass))
#define CAMEL_IS_MIME_FILTER_GZIP(obj)         (CAMEL_CHECK_TYPE ((obj), CAMEL_TYPE_MIME_FILTER_GZIP))
#define CAMEL_IS_MIME_FILTER_GZIP_CLASS(klass) (CAMEL_CHECK_CLASS_TYPE ((klass), CAMEL_TYPE_MIME_FILTER_GZIP))
#define CAMEL_MIME_FILTER_GZIP_GET_CLASS(obj)  (CAMEL_CHECK_GET_CLASS ((obj), CAMEL_TYPE_MIME_FILTER_GZIP, CamelMimeFilterGZipClass))

typedef struct _CamelMimeFilterGZip CamelMimeFilterGZip;
typedef struct _CamelMimeFilterGZipClass CamelMimeFilterGZipClass;

typedef enum {
	CAMEL_MIME_FILTER_GZIP_MODE_ZIP,
	CAMEL_MIME_FILTER_GZIP_MODE_UNZIP
} CamelMimeFilterGZipMode;

struct _CamelMimeFilterGZip {
	CamelMimeFilter parent_object;
	
	struct _CamelMimeFilterGZipPrivate *priv;
	
	CamelMimeFilterGZipMode mode;
	int level;
};

struct _CamelMimeFilterGZipClass {
	CamelMimeFilterClass parent_class;
	
};


CamelType camel_mime_filter_gzip_get_type (void);

CamelMimeFilter *camel_mime_filter_gzip_new (CamelMimeFilterGZipMode mode, int level);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __CAMEL_MIME_FILTER_GZIP_H__ */
