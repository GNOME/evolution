/*
 *  Copyright (C) 2000 Ximian Inc.
 *
 *  Authors:
 *    Michael Zucchi <notzed@ximian.com>
 *    Dan Winship <danw@ximian.com>
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

#ifndef _CAMEL_VEE_SUMMARY_H
#define _CAMEL_VEE_SUMMARY_H

#include <camel/camel-folder-summary.h>
#include <camel/camel-exception.h>

struct _CamelVeeFolder;
struct _CamelFolder;

#define CAMEL_VEE_SUMMARY(obj)         CAMEL_CHECK_CAST (obj, camel_vee_summary_get_type (), CamelVeeSummary)
#define CAMEL_VEE_SUMMARY_CLASS(klass) CAMEL_CHECK_CLASS_CAST (klass, camel_vee_summary_get_type (), CamelVeeSummaryClass)
#define CAMEL_IS_VEE_SUMMARY(obj)      CAMEL_CHECK_TYPE (obj, camel_vee_summary_get_type ())

typedef struct _CamelVeeSummary CamelVeeSummary;
typedef struct _CamelVeeSummaryClass CamelVeeSummaryClass;

typedef struct _CamelVeeMessageInfo CamelVeeMessageInfo;

struct _CamelVeeMessageInfo {
	CamelMessageInfo info;

	CamelMessageInfo *real;
};

struct _CamelVeeSummary {
	CamelFolderSummary summary;
};

struct _CamelVeeSummaryClass {
	CamelFolderSummaryClass parent_class;

};

CamelType               camel_vee_summary_get_type     (void);
CamelFolderSummary *camel_vee_summary_new(struct _CamelFolder *parent);

CamelVeeMessageInfo * camel_vee_summary_add(CamelVeeSummary *s, CamelMessageInfo *info, const char hash[8]);

#endif /* ! _CAMEL_VEE_SUMMARY_H */

