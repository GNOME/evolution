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

/* currently, this is just a straigt s/imap/nntp from the IMAP file*/


#ifndef _CAMEL_NNTP_STORE_SUMMARY_H
#define _CAMEL_NNTP_STORE_SUMMARY_H

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <camel/camel-object.h>
#include <camel/camel-store-summary.h>

#define CAMEL_NNTP_STORE_SUMMARY(obj)         CAMEL_CHECK_CAST (obj, camel_nntp_store_summary_get_type (), CamelNNTPStoreSummary)
#define CAMEL_NNTP_STORE_SUMMARY_CLASS(klass) CAMEL_CHECK_CLASS_CAST (klass, camel_nntp_store_summary_get_type (), CamelNNTPStoreSummaryClass)
#define CAMEL_IS_NNTP_STORE_SUMMARY(obj)      CAMEL_CHECK_TYPE (obj, camel_nntp_store_summary_get_type ())

typedef struct _CamelNNTPStoreSummary      CamelNNTPStoreSummary;
typedef struct _CamelNNTPStoreSummaryClass CamelNNTPStoreSummaryClass;

typedef struct _CamelNNTPStoreInfo CamelNNTPStoreInfo;

enum {
	CAMEL_NNTP_STORE_INFO_FULL_NAME = CAMEL_STORE_INFO_LAST,
	CAMEL_NNTP_STORE_INFO_LAST,
};

struct _CamelNNTPStoreInfo {
	CamelStoreInfo info;
	char *full_name;
	guint32 first;		/* from LIST or NEWGROUPS return */
	guint32 last;
};

#define NNTP_DATE_SIZE 14

struct _CamelNNTPStoreSummary {
	CamelStoreSummary summary;

	struct _CamelNNTPStoreSummaryPrivate *priv;

	/* header info */
	guint32 version;	/* version of base part of file */
	char last_newslist[NNTP_DATE_SIZE];
};

struct _CamelNNTPStoreSummaryClass {
	CamelStoreSummaryClass summary_class;
};

CamelType			 camel_nntp_store_summary_get_type	(void);
CamelNNTPStoreSummary      *camel_nntp_store_summary_new	(void);

/* TODO: this api needs some more work, needs to support lists */
/*CamelNNTPStoreNamespace *camel_nntp_store_summary_namespace_new(CamelNNTPStoreSummary *s, const char *full_name, char dir_sep);*/
/*void camel_nntp_store_summary_namespace_set(CamelNNTPStoreSummary *s, CamelNNTPStoreNamespace *ns);*/
/*CamelNNTPStoreNamespace *camel_nntp_store_summary_namespace_find_path(CamelNNTPStoreSummary *s, const char *path);*/
/*CamelNNTPStoreNamespace *camel_nntp_store_summary_namespace_find_full(CamelNNTPStoreSummary *s, const char *full_name);*/

/* helper macro's */
#define camel_nntp_store_info_full_name(s, i) (camel_store_info_string((CamelStoreSummary *)s, (const CamelStoreInfo *)i, CAMEL_NNTP_STORE_INFO_FULL_NAME))

/* converts to/from utf8 canonical nasmes */
char *camel_nntp_store_summary_full_to_path(CamelNNTPStoreSummary *s, const char *full_name, char dir_sep);

char *camel_nntp_store_summary_path_to_full(CamelNNTPStoreSummary *s, const char *path, char dir_sep);
char *camel_nntp_store_summary_dotted_to_full(CamelNNTPStoreSummary *s, const char *dotted, char dir_sep);

CamelNNTPStoreInfo *camel_nntp_store_summary_full_name(CamelNNTPStoreSummary *s, const char *full_name);
CamelNNTPStoreInfo *camel_nntp_store_summary_add_from_full(CamelNNTPStoreSummary *s, const char *full_name, char dir_sep);

/* a convenience lookup function. always use this if path known */
char *camel_nntp_store_summary_full_from_path(CamelNNTPStoreSummary *s, const char *path);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ! _CAMEL_NNTP_STORE_SUMMARY_H */
