/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2000 Ximian Inc.
 *
 * Authors: Michael Zucchi <notzed@ximian.com>
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


#ifndef _CAMEL_STORE_SUMMARY_H
#define _CAMEL_STORE_SUMMARY_H

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#include <camel/camel-mime-parser.h>
#include <camel/camel-object.h>

#define CAMEL_STORE_SUMMARY(obj)         CAMEL_CHECK_CAST (obj, camel_store_summary_get_type (), CamelStoreSummary)
#define CAMEL_STORE_SUMMARY_CLASS(klass) CAMEL_CHECK_CLASS_CAST (klass, camel_store_summary_get_type (), CamelStoreSummaryClass)
#define CAMEL_IS_FOLDER_SUMMARY(obj)      CAMEL_CHECK_TYPE (obj, camel_store_summary_get_type ())

typedef struct _CamelStoreSummary      CamelStoreSummary;
typedef struct _CamelStoreSummaryClass CamelStoreSummaryClass;

typedef struct _CamelFolderInfo CamelFolderInfo;

enum _CamelFolderFlags {
	CAMEL_STORE_SUMMARY_FOLDER_NOSELECT,
	CAMEL_STORE_SUMMARY_FOLDER_READONLY,
	CAMEL_STORE_SUMMARY_FOLDER_SUBSCRIBED,
	CAMEL_STORE_SUMMARY_FOLDER_FLAGGED,
};

#define CAMEL_STORE_SUMMARY_UNKNOWN (~0)

enum {
	CAMEL_STORE_SUMMARY_FULL = 0,
	CAMEL_STORE_SUMMARY_NAME,
	CAMEL_STORE_SUMMARY_URI,
	CAMEL_STORE_SUMMARY_LAST,
};

struct _CamelFolderInfo {
	guint32 refcount;
	char *uri;
	char *full;
	guint32 flags;
	guint32 unread;
	guint32 total;
};

enum _CamelStoreSummaryFlags {
	CAMEL_STORE_SUMMARY_DIRTY = 1<<0,
};

struct _CamelStoreSummary {
	CamelObject parent;

	struct _CamelStoreSummaryPrivate *priv;

	/* header info */
	guint32 version;	/* version of file required, should be set by implementors */
	guint32 flags;		/* flags */
	guint32 count;		/* how many were saved/loaded */
	time_t time;		/* timestamp for this summary (for implementors to use) */

	/* sizes of memory objects */
	guint32 folder_info_size;

	/* memory allocators (setup automatically) */
	struct _EMemChunk *folder_info_chunks;

	char *summary_path;
	char *uri_prefix;

	GPtrArray *folders;	/* CamelFolderInfo's */
	GHashTable *folders_full; /* CamelFolderInfo's by full name */
};

struct _CamelStoreSummaryClass {
	CamelObjectClass parent_class;

	/* load/save the global info */
	int (*summary_header_load)(CamelStoreSummary *, FILE *);
	int (*summary_header_save)(CamelStoreSummary *, FILE *);

	/* create/save/load an individual message info */
	CamelFolderInfo * (*folder_info_new)(CamelStoreSummary *, const char *full);
	CamelFolderInfo * (*folder_info_load)(CamelStoreSummary *, FILE *);
	int		  (*folder_info_save)(CamelStoreSummary *, FILE *, CamelFolderInfo *);
	void		  (*folder_info_free)(CamelStoreSummary *, CamelFolderInfo *);

	/* virtualise access methods */
	const char *(*folder_info_string)(CamelStoreSummary *, const CamelFolderInfo *, int);
	void (*folder_info_set_string)(CamelStoreSummary *, CamelFolderInfo *, int, const char *);
};

CamelType			 camel_store_summary_get_type	(void);
CamelStoreSummary      *camel_store_summary_new	(void);

void camel_store_summary_set_filename(CamelStoreSummary *, const char *);
void camel_store_summary_set_uri_prefix(CamelStoreSummary *, const char *);

/* load/save the summary in its entirety */
int camel_store_summary_load(CamelStoreSummary *);
int camel_store_summary_save(CamelStoreSummary *);

/* only load the header */
int camel_store_summary_header_load(CamelStoreSummary *);

/* set the dirty bit on the summary */
void camel_store_summary_touch(CamelStoreSummary *s);

/* add a new raw summary item */
void camel_store_summary_add(CamelStoreSummary *, CamelFolderInfo *info);

/* build/add raw summary items */
CamelFolderInfo *camel_store_summary_add_from_full(CamelStoreSummary *, const char *);

/* Just build raw summary items */
CamelFolderInfo *camel_store_summary_info_new(CamelStoreSummary *s);
CamelFolderInfo *camel_store_summary_info_new_from_full(CamelStoreSummary *s, const char *);

void camel_store_summary_info_ref(CamelStoreSummary *, CamelFolderInfo *);
void camel_store_summary_info_free(CamelStoreSummary *, CamelFolderInfo *);

/* removes a summary item */
void camel_store_summary_remove(CamelStoreSummary *s, CamelFolderInfo *info);
void camel_store_summary_remove_full(CamelStoreSummary *s, const char *full);
void camel_store_summary_remove_index(CamelStoreSummary *s, int);

/* remove all items */
void camel_store_summary_clear(CamelStoreSummary *s);

/* lookup functions */
int camel_store_summary_count(CamelStoreSummary *);
CamelFolderInfo *camel_store_summary_index(CamelStoreSummary *, int);
CamelFolderInfo *camel_store_summary_full(CamelStoreSummary *, const char *uid);
GPtrArray *camel_store_summary_array(CamelStoreSummary *s);
void camel_store_summary_array_free(CamelStoreSummary *s, GPtrArray *array);

const char *camel_folder_info_string(CamelStoreSummary *, const CamelFolderInfo *, int type);
void camel_folder_info_set_string(CamelStoreSummary *, CamelFolderInfo *, int type, const char *value);

/* helper macro's */
#define camel_folder_info_full(s, i) (camel_folder_info_string((CamelStoreSummary *)s, (const CamelFolderInfo *)i, CAMEL_STORE_SUMMARY_FULL))
#define camel_folder_info_uri(s, i) (camel_folder_info_string((CamelStoreSummary *)s, (const CamelFolderInfo *)i, CAMEL_STORE_SUMMARY_URI))
#define camel_folder_info_name(s, i) (camel_folder_info_string((CamelStoreSummary *)s, (const CamelFolderInfo *)i, CAMEL_STORE_SUMMARY_NAME))

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ! _CAMEL_STORE_SUMMARY_H */
