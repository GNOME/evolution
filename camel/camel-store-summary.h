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

#include <glib.h>

#include <camel/camel-mime-parser.h>
#include <camel/camel-object.h>
#include <camel/camel-url.h>

#define CAMEL_STORE_SUMMARY(obj)         CAMEL_CHECK_CAST (obj, camel_store_summary_get_type (), CamelStoreSummary)
#define CAMEL_STORE_SUMMARY_CLASS(klass) CAMEL_CHECK_CLASS_CAST (klass, camel_store_summary_get_type (), CamelStoreSummaryClass)
#define CAMEL_IS_STORE_SUMMARY(obj)      CAMEL_CHECK_TYPE (obj, camel_store_summary_get_type ())

typedef struct _CamelStoreSummary      CamelStoreSummary;
typedef struct _CamelStoreSummaryClass CamelStoreSummaryClass;

typedef struct _CamelStoreInfo CamelStoreInfo;

enum _CamelStoreInfoFlags {
	CAMEL_STORE_INFO_FOLDER_NOSELECT = 1<<0,
	CAMEL_STORE_INFO_FOLDER_READONLY = 1<<1,
	CAMEL_STORE_INFO_FOLDER_SUBSCRIBED = 1<<2,
	CAMEL_STORE_INFO_FOLDER_FLAGGED = 1<<3,
};

#define CAMEL_STORE_INFO_FOLDER_UNKNOWN (~0)

enum {
	CAMEL_STORE_INFO_PATH = 0,
	CAMEL_STORE_INFO_NAME,
	CAMEL_STORE_INFO_URI,
	CAMEL_STORE_INFO_LAST,
};

struct _CamelStoreInfo {
	guint32 refcount;
	char *uri;
	char *path;
	guint32 flags;
	guint32 unread;
	guint32 total;
};

enum _CamelStoreSummaryFlags {
	CAMEL_STORE_SUMMARY_DIRTY = 1<<0,
	CAMEL_STORE_SUMMARY_FRAGMENT = 1<<1, /* path name is stored in fragment rather than path */
};

struct _CamelStoreSummary {
	CamelObject parent;

	struct _CamelStoreSummaryPrivate *priv;

	/* header info */
	guint32 version;	/* version of base part of file */
	guint32 flags;		/* flags */
	guint32 count;		/* how many were saved/loaded */
	time_t time;		/* timestamp for this summary (for implementors to use) */
	struct _CamelURL *uri_base;	/* url of base part of summary */

	/* sizes of memory objects */
	guint32 store_info_size;

	/* memory allocators (setup automatically) */
	struct _EMemChunk *store_info_chunks;

	char *summary_path;

	GPtrArray *folders;	/* CamelStoreInfo's */
	GHashTable *folders_path; /* CamelStoreInfo's by path name */
};

struct _CamelStoreSummaryClass {
	CamelObjectClass parent_class;

	/* load/save the global info */
	int (*summary_header_load)(CamelStoreSummary *, FILE *);
	int (*summary_header_save)(CamelStoreSummary *, FILE *);

	/* create/save/load an individual message info */
	CamelStoreInfo * (*store_info_new)(CamelStoreSummary *, const char *path);
	CamelStoreInfo * (*store_info_load)(CamelStoreSummary *, FILE *);
	int		  (*store_info_save)(CamelStoreSummary *, FILE *, CamelStoreInfo *);
	void		  (*store_info_free)(CamelStoreSummary *, CamelStoreInfo *);

	/* virtualise access methods */
	const char *(*store_info_string)(CamelStoreSummary *, const CamelStoreInfo *, int);
	void (*store_info_set_string)(CamelStoreSummary *, CamelStoreInfo *, int, const char *);
};

CamelType			 camel_store_summary_get_type	(void);
CamelStoreSummary      *camel_store_summary_new	(void);

void camel_store_summary_set_filename(CamelStoreSummary *, const char *);
void camel_store_summary_set_uri_base(CamelStoreSummary *s, CamelURL *base);

/* load/save the summary in its entirety */
int camel_store_summary_load(CamelStoreSummary *);
int camel_store_summary_save(CamelStoreSummary *);

/* only load the header */
int camel_store_summary_header_load(CamelStoreSummary *);

/* set the dirty bit on the summary */
void camel_store_summary_touch(CamelStoreSummary *s);

/* add a new raw summary item */
void camel_store_summary_add(CamelStoreSummary *, CamelStoreInfo *info);

/* build/add raw summary items */
CamelStoreInfo *camel_store_summary_add_from_path(CamelStoreSummary *, const char *);

/* Just build raw summary items */
CamelStoreInfo *camel_store_summary_info_new(CamelStoreSummary *s);
CamelStoreInfo *camel_store_summary_info_new_from_path(CamelStoreSummary *s, const char *);

void camel_store_summary_info_ref(CamelStoreSummary *, CamelStoreInfo *);
void camel_store_summary_info_free(CamelStoreSummary *, CamelStoreInfo *);

/* removes a summary item */
void camel_store_summary_remove(CamelStoreSummary *s, CamelStoreInfo *info);
void camel_store_summary_remove_path(CamelStoreSummary *s, const char *path);
void camel_store_summary_remove_index(CamelStoreSummary *s, int);

/* remove all items */
void camel_store_summary_clear(CamelStoreSummary *s);

/* lookup functions */
int camel_store_summary_count(CamelStoreSummary *);
CamelStoreInfo *camel_store_summary_index(CamelStoreSummary *, int);
CamelStoreInfo *camel_store_summary_path(CamelStoreSummary *, const char *uid);
GPtrArray *camel_store_summary_array(CamelStoreSummary *s);
void camel_store_summary_array_free(CamelStoreSummary *s, GPtrArray *array);

const char *camel_store_info_string(CamelStoreSummary *, const CamelStoreInfo *, int type);
void camel_store_info_set_string(CamelStoreSummary *, CamelStoreInfo *, int type, const char *value);

/* helper macro's */
#define camel_store_info_path(s, i) (camel_store_info_string((CamelStoreSummary *)s, (const CamelStoreInfo *)i, CAMEL_STORE_INFO_PATH))
#define camel_store_info_uri(s, i) (camel_store_info_string((CamelStoreSummary *)s, (const CamelStoreInfo *)i, CAMEL_STORE_INFO_URI))
#define camel_store_info_name(s, i) (camel_store_info_string((CamelStoreSummary *)s, (const CamelStoreInfo *)i, CAMEL_STORE_INFO_NAME))

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ! _CAMEL_STORE_SUMMARY_H */
