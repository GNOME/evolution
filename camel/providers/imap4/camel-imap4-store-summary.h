/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@novell.com>
 *
 *  Copyright 2004 Novell, Inc. (www.novell.com)
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
 *
 */


#ifndef __CAMEL_IMAP_STORE_SUMMARY_H__
#define __CAMEL_IMAP_STORE_SUMMARY_H__

#include <camel/camel-store-summary.h>
#include "camel-imap4-engine.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define CAMEL_IMAP4_STORE_SUMMARY(obj)         CAMEL_CHECK_CAST (obj, camel_imap4_store_summary_get_type (), CamelIMAP4StoreSummary)
#define CAMEL_IMAP4_STORE_SUMMARY_CLASS(klass) CAMEL_CHECK_CLASS_CAST (klass, camel_imap4_store_summary_get_type (), CamelIMAP4StoreSummaryClass)
#define CAMEL_IS_IMAP4_STORE_SUMMARY(obj)      CAMEL_CHECK_TYPE (obj, camel_imap4_store_summary_get_type ())

typedef struct _CamelIMAP4StoreSummary      CamelIMAP4StoreSummary;
typedef struct _CamelIMAP4StoreSummaryClass CamelIMAP4StoreSummaryClass;

typedef struct _CamelIMAP4StoreInfo CamelIMAP4StoreInfo;

enum {
	CAMEL_IMAP4_STORE_INFO_FULL_NAME = CAMEL_STORE_INFO_LAST,
	CAMEL_IMAP4_STORE_INFO_LAST,
};


struct _CamelFolderInfo;


struct _CamelIMAP4StoreInfo {
	CamelStoreInfo info;
};

struct _CamelIMAP4StoreSummary {
	CamelStoreSummary summary;
	
	struct _CamelIMAP4StoreSummaryPrivate *priv;
	
	/* header info */
	guint32 version;
	
	CamelIMAP4NamespaceList *namespaces;
	guint32 capa;
};

struct _CamelIMAP4StoreSummaryClass {
	CamelStoreSummaryClass summary_class;
};


CamelType camel_imap4_store_summary_get_type (void);

CamelIMAP4StoreSummary *camel_imap4_store_summary_new (void);

void camel_imap4_store_summary_set_capabilities (CamelIMAP4StoreSummary *s, guint32 capa);
void camel_imap4_store_summary_set_namespaces (CamelIMAP4StoreSummary *s, const CamelIMAP4NamespaceList *ns);

/* add the info to the cache if we don't already have it, otherwise do nothing */
void camel_imap4_store_summary_note_info (CamelIMAP4StoreSummary *s, struct _CamelFolderInfo *fi);

void camel_imap4_store_summary_unnote_info (CamelIMAP4StoreSummary *s, struct _CamelFolderInfo *fi);

struct _CamelFolderInfo *camel_imap4_store_summary_get_folder_info (CamelIMAP4StoreSummary *s, const char *top, guint32 flags);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __CAMEL_IMAP4_STORE_SUMMARY_H__ */
