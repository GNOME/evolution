/*
 *  Copyright (C) 2001 Ximian Inc. (www.ximian.com)
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

#ifndef _CAMEL_SPOOL_SUMMARY_H
#define _CAMEL_SPOOL_SUMMARY_H

#include <camel/camel-folder-summary.h>
#include <camel/camel-folder.h>
#include <camel/camel-exception.h>
#include <camel/camel-index.h>

#define CAMEL_SPOOL_SUMMARY(obj)         CAMEL_CHECK_CAST (obj, camel_spool_summary_get_type (), CamelSpoolSummary)
#define CAMEL_SPOOL_SUMMARY_CLASS(klass) CAMEL_CHECK_CLASS_CAST (klass, camel_spool_summary_get_type (), CamelSpoolSummaryClass)
#define CAMEL_IS_SPOOL_SUMMARY(obj)      CAMEL_CHECK_TYPE (obj, camel_spool_summary_get_type ())

typedef struct _CamelSpoolSummary      CamelSpoolSummary;
typedef struct _CamelSpoolSummaryClass CamelSpoolSummaryClass;

/* extra summary flags */
enum {
	CAMEL_MESSAGE_FOLDER_NOXEV = 1<<17,
	CAMEL_MESSAGE_FOLDER_XEVCHANGE = 1<<18,
};

typedef struct _CamelSpoolMessageInfo {
	CamelMessageInfo info;

	off_t frompos;
} CamelSpoolMessageInfo;

struct _CamelSpoolSummary {
	CamelFolderSummary parent;

	struct _CamelSpoolSummaryPrivate *priv;

	char *folder_path;	/* name of matching folder */

	size_t folder_size;
};

struct _CamelSpoolSummaryClass {
	CamelFolderSummaryClass parent_class;

	int (*load)(CamelSpoolSummary *cls, int forceindex, CamelException *ex);
	int (*check)(CamelSpoolSummary *cls, CamelFolderChangeInfo *changeinfo, CamelException *ex);
	int (*sync)(CamelSpoolSummary *cls, gboolean expunge, CamelFolderChangeInfo *changeinfo, CamelException *ex);
	CamelMessageInfo *(*add)(CamelSpoolSummary *cls, CamelMimeMessage *msg, const CamelMessageInfo *info, CamelFolderChangeInfo *, CamelException *ex);

	char *(*encode_x_evolution)(CamelSpoolSummary *cls, const CamelMessageInfo *info);
	int (*decode_x_evolution)(CamelSpoolSummary *cls, const char *xev, CamelMessageInfo *info);
};

guint	camel_spool_summary_get_type	(void);
void	camel_spool_summary_construct	(CamelSpoolSummary *new, const char *filename, const char *spool_name, CamelIndex *index);

/* create the summary, in-memory only */
CamelSpoolSummary *camel_spool_summary_new(const char *filename);

/* load/check the summary */
int camel_spool_summary_load(CamelSpoolSummary *cls, int forceindex, CamelException *ex);
/* check for new/removed messages */
int camel_spool_summary_check(CamelSpoolSummary *cls, CamelFolderChangeInfo *, CamelException *ex);
/* perform a folder sync or expunge, if needed */
int camel_spool_summary_sync(CamelSpoolSummary *cls, gboolean expunge, CamelFolderChangeInfo *, CamelException *ex);
/* add a new message to the summary */
CamelMessageInfo *camel_spool_summary_add(CamelSpoolSummary *cls, CamelMimeMessage *msg, const CamelMessageInfo *info, CamelFolderChangeInfo *, CamelException *ex);

/* generate an X-Evolution header line */
char *camel_spool_summary_encode_x_evolution(CamelSpoolSummary *cls, const CamelMessageInfo *info);
int camel_spool_summary_decode_x_evolution(CamelSpoolSummary *cls, const char *xev, CamelMessageInfo *info);

/* utility functions - write headers to a file with optional X-Evolution header */
int camel_spool_summary_write_headers(int fd, struct _header_raw *header, char *xevline);
/* build a from line: FIXME: remove, or move to common code */
char *camel_spool_summary_build_from(struct _header_raw *header);

#endif /* ! _CAMEL_SPOOL_SUMMARY_H */

