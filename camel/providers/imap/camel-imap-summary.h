/*
 *  Copyright (C) 2000 Helix Code Inc.
 *
 *  Authors:
 *    Michael Zucchi <notzed@helixcode.com>
 *    Dan Winship <danw@helixcode.com>
 *
 *  This program is free software; you can redistribute it and/or 
 *  modify it under the terms of the GNU General Public License as 
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 *  USA
 */

#ifndef _CAMEL_IMAP_SUMMARY_H
#define _CAMEL_IMAP_SUMMARY_H

#include "camel-imap-types.h"
#include <camel/camel-folder-summary.h>
#include <camel/camel-exception.h>

#define CAMEL_IMAP_SUMMARY(obj)         CAMEL_CHECK_CAST (obj, camel_imap_summary_get_type (), CamelImapSummary)
#define CAMEL_IMAP_SUMMARY_CLASS(klass) CAMEL_CHECK_CLASS_CAST (klass, camel_imap_summary_get_type (), CamelImapSummaryClass)
#define CAMEL_IS_IMAP_SUMMARY(obj)      CAMEL_CHECK_TYPE (obj, camel_imap_summary_get_type ())

typedef struct _CamelImapSummaryClass CamelImapSummaryClass;

typedef struct _CamelImapMessageContentInfo {
	CamelMessageContentInfo info;

} CamelImapMessageContentInfo;

typedef struct _CamelImapMessageInfo {
	CamelMessageInfo info;

	guint32 server_flags;
} CamelImapMessageInfo;

struct _CamelImapSummary {
	CamelFolderSummary parent;

	guint32 validity;
};

struct _CamelImapSummaryClass {
	CamelFolderSummaryClass parent_class;

};

guint               camel_imap_summary_get_type     (void);
CamelFolderSummary *camel_imap_summary_new          (const char *filename,
						     guint32 validity);

#endif /* ! _CAMEL_IMAP_SUMMARY_H */

