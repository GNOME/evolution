/*
 *  Copyright (C) 2000 Helix Code Inc.
 *
 *  Authors: Not Zed <notzed@lostzed.mmc.com.au>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License
 *  as published by the Free Software Foundation; either version 2 of
 *  the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef _CAMEL_MAILDIR_SUMMARY_H
#define _CAMEL_MAILDIR_SUMMARY_H

#include "camel-local-summary.h"
#include <camel/camel-folder.h>
#include <camel/camel-exception.h>
#include <libibex/ibex.h>

#define CAMEL_MAILDIR_SUMMARY(obj)	CAMEL_CHECK_CAST (obj, camel_maildir_summary_get_type (), CamelMaildirSummary)
#define CAMEL_MAILDIR_SUMMARY_CLASS(klass)	CAMEL_CHECK_CLASS_CAST (klass, camel_maildir_summary_get_type (), CamelMaildirSummaryClass)
#define IS_CAMEL_MAILDIR_SUMMARY(obj)      CAMEL_CHECK_TYPE (obj, camel_maildir_summary_get_type ())

typedef struct _CamelMaildirSummary	CamelMaildirSummary;
typedef struct _CamelMaildirSummaryClass	CamelMaildirSummaryClass;

typedef struct _CamelMaildirMessageContentInfo {
	CamelMessageContentInfo info;
} CamelMaildirMessageContentInfo;

typedef struct _CamelMaildirMessageInfo {
	CamelMessageInfo info;

	char *filename;		/* maildir has this annoying status shit on the end of the filename, use this to get the real message id */
} CamelMaildirMessageInfo;

struct _CamelMaildirSummary {
	CamelLocalSummary parent;
	struct _CamelMaildirSummaryPrivate *priv;
};

struct _CamelMaildirSummaryClass {
	CamelLocalSummaryClass parent_class;

	/* virtual methods */

	/* signals */
};

CamelType	 camel_maildir_summary_get_type	(void);
CamelMaildirSummary	*camel_maildir_summary_new	(const char *filename, const char *maildirdir, ibex *index);

/* convert some info->flags to/from the messageinfo */
char *camel_maildir_summary_info_to_name(const CamelMessageInfo *info);
int camel_maildir_summary_name_to_info(CamelMessageInfo *info, const char *name);

#endif /* ! _CAMEL_MAILDIR_SUMMARY_H */

