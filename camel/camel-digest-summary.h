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


#ifndef __CAMEL_DIGEST_SUMMARY_H__
#define __CAMEL_DIGEST_SUMMARY_H__

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <camel/camel-folder-summary.h>
#include <camel/camel-folder.h>
#include <camel/camel-exception.h>

#define CAMEL_DIGEST_SUMMARY(obj)         CAMEL_CHECK_CAST (obj, camel_digest_summary_get_type (), CamelDigestSummary)
#define CAMEL_DIGEST_SUMMARY_CLASS(klass) CAMEL_CHECK_CLASS_CAST (klass, camel_digest_summary_get_type (), CamelDigestSummaryClass)
#define CAMEL_IS_DIGEST_SUMMARY(obj)      CAMEL_CHECK_TYPE (obj, camel_digest_summary_get_type ())

typedef struct _CamelDigestSummary      CamelDigestSummary;
typedef struct _CamelDigestSummaryClass CamelDigestSummaryClass;

struct _CamelDigestSummary {
	CamelFolderSummary parent_object;
	
};

struct _CamelDigestSummaryClass {
	CamelFolderSummaryClass parent_class;
	
};


CamelType camel_digest_summary_get_type (void);

CamelFolderSummary *camel_digest_summary_new (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __CAMEL_DIGEST_SUMMARY_H__ */
