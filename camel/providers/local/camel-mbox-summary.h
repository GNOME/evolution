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

#ifndef _CAMEL_MBOX_SUMMARY_H
#define _CAMEL_MBOX_SUMMARY_H

#include "camel-local-summary.h"

#define CAMEL_MBOX_SUMMARY(obj)         CAMEL_CHECK_CAST (obj, camel_mbox_summary_get_type (), CamelMboxSummary)
#define CAMEL_MBOX_SUMMARY_CLASS(klass) CAMEL_CHECK_CLASS_CAST (klass, camel_mbox_summary_get_type (), CamelMboxSummaryClass)
#define CAMEL_IS_MBOX_SUMMARY(obj)      CAMEL_CHECK_TYPE (obj, camel_mbox_summary_get_type ())

typedef struct _CamelMboxSummary      CamelMboxSummary;
typedef struct _CamelMboxSummaryClass CamelMboxSummaryClass;

typedef struct _CamelMboxMessageContentInfo {
	CamelMessageContentInfo info;
} CamelMboxMessageContentInfo;

typedef struct _CamelMboxMessageInfo {
	CamelMessageInfo info;

	off_t frompos;
} CamelMboxMessageInfo;

struct _CamelMboxSummary {
	CamelLocalSummary parent;

	struct _CamelMboxSummaryPrivate *priv;

	size_t folder_size;	/* size of the mbox file, last sync */
};

struct _CamelMboxSummaryClass {
	CamelLocalSummaryClass parent_class;
};

CamelType		camel_mbox_summary_get_type	(void);
CamelMboxSummary      *camel_mbox_summary_new	(const char *filename, const char *mbox_name, CamelIndex *index);

/* generate a From line from headers */
char *camel_mbox_summary_build_from(struct _header_raw *header);

#endif /* ! _CAMEL_MBOX_SUMMARY_H */

