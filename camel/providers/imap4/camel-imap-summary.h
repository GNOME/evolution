/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  Camel
 *  Copyright (C) 1999-2004 Jeffrey Stedfast
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
 */


#ifndef __CAMEL_IMAP_SUMMARY_H__
#define __CAMEL_IMAP_SUMMARY_H__

#include <sys/types.h>

#include <camel/camel-folder.h>
#include <camel/camel-folder-summary.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define CAMEL_TYPE_IMAP_SUMMARY            (camel_imap_summary_get_type ())
#define CAMEL_IMAP_SUMMARY(obj)            (CAMEL_CHECK_CAST ((obj), CAMEL_TYPE_IMAP_SUMMARY, CamelIMAPSummary))
#define CAMEL_IMAP_SUMMARY_CLASS(klass)    (CAMEL_CHECK_CLASS_CAST ((klass), CAMEL_TYPE_IMAP_SUMMARY, CamelIMAPSummaryClass))
#define CAMEL_IS_IMAP_SUMMARY(obj)         (CAMEL_CHECK_TYPE ((obj), CAMEL_TYPE_IMAP_SUMMARY))
#define CAMEL_IS_IMAP_SUMMARY_CLASS(klass) (CAMEL_CHECK_CLASS_TYPE ((klass), CAMEL_TYPE_IMAP_SUMMARY))
#define CAMEL_IMAP_SUMMARY_GET_CLASS(obj)  (CAMEL_CHECK_GET_CLASS ((obj), CAMEL_TYPE_FOLDER_SUMMARY, CamelIMAPSummaryClass))

typedef struct _CamelIMAPMessageInfo CamelIMAPMessageInfo;
typedef struct _CamelIMAPSummary CamelIMAPSummary;
typedef struct _CamelIMAPSummaryClass CamelIMAPSummaryClass;

struct _CamelIMAPMessageInfo {
	CamelMessageInfo parent_info;
	
	guint32 server_flags;
};

struct _CamelIMAPSummary {
	CamelFolderSummary parent_object;
	
	CamelFolder *folder;
	
	guint32 exists;
	guint32 recent;
	guint32 unseen;
	
	guint32 uidvalidity;
	
	guint uidvalidity_changed:1;
	guint exists_changed:1;
};

struct _CamelIMAPSummaryClass {
	CamelFolderSummaryClass parent_class;
	
};


CamelType camel_imap_summary_get_type (void);

CamelFolderSummary *camel_imap_summary_new (CamelFolder *folder);

void camel_imap_summary_set_exists (CamelFolderSummary *summary, guint32 exists);
void camel_imap_summary_set_recent (CamelFolderSummary *summary, guint32 recent);
void camel_imap_summary_set_unseen (CamelFolderSummary *summary, guint32 unseen);
void camel_imap_summary_set_uidnext (CamelFolderSummary *summary, guint32 uidnext);

void camel_imap_summary_set_uidvalidity (CamelFolderSummary *summary, guint32 uidvalidity);

void camel_imap_summary_expunge (CamelFolderSummary *summary, int seqid);

int camel_imap_summary_flush_updates (CamelFolderSummary *summary, CamelException *ex);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __CAMEL_IMAP_SUMMARY_H__ */
