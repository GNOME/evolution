/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002 Ximian Inc.
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


#ifndef _CAMEL_IMAP_STORE_SUMMARY_H
#define _CAMEL_IMAP_STORE_SUMMARY_H

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <camel/camel-object.h>
#include <camel/camel-store-summary.h>

#define CAMEL_IMAP_STORE_SUMMARY(obj)         CAMEL_CHECK_CAST (obj, camel_imap_store_summary_get_type (), CamelImapStoreSummary)
#define CAMEL_IMAP_STORE_SUMMARY_CLASS(klass) CAMEL_CHECK_CLASS_CAST (klass, camel_imap_store_summary_get_type (), CamelImapStoreSummaryClass)
#define CAMEL_IS_IMAP_STORE_SUMMARY(obj)      CAMEL_CHECK_TYPE (obj, camel_imap_store_summary_get_type ())

typedef struct _CamelImapStoreSummary      CamelImapStoreSummary;
typedef struct _CamelImapStoreSummaryClass CamelImapStoreSummaryClass;

typedef struct _CamelImapStoreInfo CamelImapStoreInfo;

enum {
	CAMEL_IMAP_STORE_INFO_FULL_NAME = CAMEL_STORE_INFO_LAST,
	CAMEL_IMAP_STORE_INFO_LAST,
};

struct _CamelImapStoreInfo {
	CamelStoreInfo info;
	char *full_name;
};

typedef struct _CamelImapStoreNamespace CamelImapStoreNamespace;

struct _CamelImapStoreNamespace {
	char *path;		/* display path */
	char *full_name;	/* real name */
	char sep;		/* directory separator */
};

struct _CamelImapStoreSummary {
	CamelStoreSummary summary;

	struct _CamelImapStoreSummaryPrivate *priv;

	/* header info */
	guint32 version;	/* version of base part of file */
	guint32 capabilities;
	CamelImapStoreNamespace *namespace; /* eventually to be a list */
};

struct _CamelImapStoreSummaryClass {
	CamelStoreSummaryClass summary_class;
};

CamelType			 camel_imap_store_summary_get_type	(void);
CamelImapStoreSummary      *camel_imap_store_summary_new	(void);

/* TODO: this api needs some more work, needs to support lists */
CamelImapStoreNamespace *camel_imap_store_summary_namespace_new(CamelImapStoreSummary *s, const char *full_name, char dir_sep);
void camel_imap_store_summary_namespace_set(CamelImapStoreSummary *s, CamelImapStoreNamespace *ns);
CamelImapStoreNamespace *camel_imap_store_summary_namespace_find_path(CamelImapStoreSummary *s, const char *path);
CamelImapStoreNamespace *camel_imap_store_summary_namespace_find_full(CamelImapStoreSummary *s, const char *full_name);

/* converts to/from utf8 canonical nasmes */
char *camel_imap_store_summary_full_to_path(CamelImapStoreSummary *s, const char *full_name, char dir_sep);
char *camel_imap_store_summary_path_to_full(CamelImapStoreSummary *s, const char *path, char dir_sep);

CamelImapStoreInfo *camel_imap_store_summary_full_name(CamelImapStoreSummary *s, const char *full_name);
CamelImapStoreInfo *camel_imap_store_summary_add_from_full(CamelImapStoreSummary *s, const char *full_name, char dir_sep);

/* a convenience lookup function. always use this if path known */
char *camel_imap_store_summary_full_from_path(CamelImapStoreSummary *s, const char *path);

/* helper macro's */
#define camel_imap_store_info_full_name(s, i) (camel_store_info_string((CamelStoreSummary *)s, (const CamelStoreInfo *)i, CAMEL_IMAP_STORE_INFO_FULL_NAME))

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ! _CAMEL_IMAP_STORE_SUMMARY_H */
