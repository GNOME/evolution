/*
 *  Copyright (C) 2000 Helix Code Inc.
 *
 *  Authors: Michael Zucchi <notzed@helixcode.com>
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

#ifndef _CAMEL_MBOX_SUMMARY_H
#define _CAMEL_MBOX_SUMMARY_H

#include <gtk/gtk.h>
#include <camel/camel-folder-summary.h>
#include <libibex/ibex.h>

#define CAMEL_MBOX_SUMMARY(obj)         GTK_CHECK_CAST (obj, camel_mbox_summary_get_type (), CamelMboxSummary)
#define CAMEL_MBOX_SUMMARY_CLASS(klass) GTK_CHECK_CLASS_CAST (klass, camel_mbox_summary_get_type (), CamelMboxSummaryClass)
#define IS_CAMEL_MBOX_SUMMARY(obj)      GTK_CHECK_TYPE (obj, camel_mbox_summary_get_type ())

typedef struct _CamelMboxSummary      CamelMboxSummary;
typedef struct _CamelMboxSummaryClass CamelMboxSummaryClass;

/* extra summary flags */
enum {
	CAMEL_MESSAGE_FOLDER_NOXEV = 1<<17,
};

typedef struct _CamelMboxMessageContentInfo {
	CamelMessageContentInfo info;
} CamelMboxMessageContentInfo;

typedef struct _CamelMboxMessageInfo {
	CamelMessageInfo info;

	off_t frompos;
} CamelMboxMessageInfo;

struct _CamelMboxSummary {
	CamelFolderSummary parent;

	struct _CamelMboxSummaryPrivate *priv;

	char *folder_path;	/* name of matching folder */
	size_t folder_size;	/* size of the mbox file, last sync */

	ibex *index;
	int index_force;	/* do we force index during creation? */
};

struct _CamelMboxSummaryClass {
	CamelFolderSummaryClass parent_class;
};

guint		camel_mbox_summary_get_type	(void);
CamelMboxSummary      *camel_mbox_summary_new	(const char *filename, const char *mbox_name, ibex *index);

/* load/check the summary */
int camel_mbox_summary_load(CamelMboxSummary *mbs, int forceindex);
/* incremental update */
int camel_mbox_summary_update(CamelMboxSummary *mbs, off_t offset);
/* perform a folder sync or expunge, if needed */
int camel_mbox_summary_sync (CamelMboxSummary *mbs, gboolean expunge);

#endif /* ! _CAMEL_MBOX_SUMMARY_H */
