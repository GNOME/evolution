/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright (C) 2000 Helix Code Inc.
 *
 *  Authors: Jeffrey Stedfast <fejj@helixcode.com>
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

#include <gtk/gtk.h>
#include <camel/camel-folder-summary.h>
#include <libibex/ibex.h>

#define CAMEL_IMAP_SUMMARY(obj)         GTK_CHECK_CAST (obj, camel_imap_summary_get_type (), CamelImapSummary)
#define CAMEL_IMAP_SUMMARY_CLASS(klass) GTK_CHECK_CLASS_CAST (klass, camel_imap_summary_get_type (), CamelImapSummaryClass)
#define IS_CAMEL_IMAP_SUMMARY(obj)      GTK_CHECK_TYPE (obj, camel_imap_summary_get_type ())

typedef struct _CamelImapSummary      CamelImapSummary;
typedef struct _CamelImapSummaryClass CamelImapSummaryClass;

/* extra summary flags */
enum {
	CAMEL_MESSAGE_FOLDER_NOXEV = 1<<16,
/*	CAMEL_MESSAGE_FOLDER_FLAGGED = 1<<17,*/
};

typedef struct _CamelImapMessageContentInfo {
	CamelMessageContentInfo info;
} CamelImapMessageContentInfo;

typedef struct _CamelImapMessageInfo {
	CamelMessageInfo info;

	off_t frompos;
} CamelImapMessageInfo;

struct _CamelImapSummary {
	CamelFolderSummary parent;

	struct _CamelImapSummaryPrivate *priv;

	char *folder_path;	/* name of matching folder */
	size_t folder_size;	/* size of the imap file, last sync */

	ibex *index;
	int index_force;	/* do we force index during creation? */
};

struct _CamelImapSummaryClass {
	CamelFolderSummaryClass parent_class;
};

guint camel_imap_summary_get_type (void);

CamelImapSummary *camel_imap_summary_new (const char *filename, const char *imap_name, ibex *index);

/* load/check the summary */
int camel_imap_summary_load (CamelImapSummary *mbs, int forceindex);

/* incremental update */
int camel_imap_summary_update (CamelImapSummary *mbs, off_t offset);

/* perform a folder expunge */
int camel_imap_summary_expunge (CamelImapSummary *mbs);

#endif /* ! _CAMEL_IMAP_SUMMARY_H */
