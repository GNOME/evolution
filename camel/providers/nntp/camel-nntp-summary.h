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

#ifndef _CAMEL_NNTP_SUMMARY_H
#define _CAMEL_NNTP_SUMMARY_H

#include <camel/camel-folder-summary.h>
#include <camel/camel-folder.h>
#include <camel/camel-exception.h>

#define CAMEL_NNTP_SUMMARY(obj)         CAMEL_CHECK_CAST (obj, camel_nntp_summary_get_type (), CamelNNTPSummary)
#define CAMEL_NNTP_SUMMARY_CLASS(klass) CAMEL_CHECK_CLASS_CAST (klass, camel_nntp_summary_get_type (), CamelNNTPSummaryClass)
#define CAMEL_IS_LOCAL_SUMMARY(obj)      CAMEL_CHECK_TYPE (obj, camel_nntp_summary_get_type ())

typedef struct _CamelNNTPSummary      CamelNNTPSummary;
typedef struct _CamelNNTPSummaryClass CamelNNTPSummaryClass;

struct _CamelNNTPSummary {
	CamelFolderSummary parent;

	struct _CamelNNTPSummaryPrivate *priv;

	struct _CamelNNTPFolder *folder;

	guint32 version;
	guint32 high, low;
};

struct _CamelNNTPSummaryClass {
	CamelFolderSummaryClass parent_class;
};

CamelType	camel_nntp_summary_get_type	(void);
CamelNNTPSummary *camel_nntp_summary_new(struct _CamelNNTPFolder *folder);

int camel_nntp_summary_check(CamelNNTPSummary *cns, CamelFolderChangeInfo *, CamelException *ex);

#if 0
/* load/check the summary */
int camel_nntp_summary_load(CamelNNTPSummary *cls, CamelException *ex);
/* check for new/removed messages */
int camel_nntp_summary_check(CamelNNTPSummary *cls, CamelFolderChangeInfo *, CamelException *ex);
/* perform a folder sync or expunge, if needed */
int camel_nntp_summary_sync(CamelNNTPSummary *cls, gboolean expunge, CamelFolderChangeInfo *, CamelException *ex);
/* add a new message to the summary */
CamelMessageInfo *camel_nntp_summary_add(CamelNNTPSummary *cls, CamelMimeMessage *msg, const CamelMessageInfo *info, CamelFolderChangeInfo *, CamelException *ex);
#endif

#endif /* ! _CAMEL_NNTP_SUMMARY_H */

