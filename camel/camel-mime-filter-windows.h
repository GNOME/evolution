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


#ifndef __CAMEL_MIME_FILTER_WINDOWS_H__
#define __CAMEL_MIME_FILTER_WINDOWS_H__

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <camel/camel-mime-filter.h>

#define CAMEL_MIME_FILTER_WINDOWS(obj)         CAMEL_CHECK_CAST (obj, camel_mime_filter_windows_get_type (), CamelMimeFilterWindows)
#define CAMEL_MIME_FILTER_WINDOWS_CLASS(klass) CAMEL_CHECK_CLASS_CAST (klass, camel_mime_filter_windows_get_type (), CamelMimeFilterWindowsClass)
#define CAMEL_IS_MIME_FILTER_WINDOWS(obj)      CAMEL_CHECK_TYPE (obj, camel_mime_filter_windows_get_type ())

typedef struct _CamelMimeFilterWindowsClass CamelMimeFilterWindowsClass;
typedef struct _CamelMimeFilterWindows CamelMimeFilterWindows;

struct _CamelMimeFilterWindows {
	CamelMimeFilter parent;
	
	gboolean is_windows;
	char *claimed_charset;
};

struct _CamelMimeFilterWindowsClass {
	CamelMimeFilterClass parent_class;
	
};


CamelType camel_mime_filter_windows_get_type (void);

CamelMimeFilter *camel_mime_filter_windows_new (const char *claimed_charset);

gboolean camel_mime_filter_windows_is_windows_charset (CamelMimeFilterWindows *filter);
const char *camel_mime_filter_windows_real_charset (CamelMimeFilterWindows *filter);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __CAMEL_MIME_FILTER_WINDOWS_H__ */
