/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-imap-store.h : class for an imap store */

/* 
 * Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 2000 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of version 2 of the GNU General Public 
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */


#ifndef CAMEL_IMAP_STORE_H
#define CAMEL_IMAP_STORE_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include "camel-imap-types.h"
#include <camel/camel-disco-store.h>

#ifdef ENABLE_THREADS
#include <libedataserver/e-msgport.h>

typedef struct _CamelImapMsg CamelImapMsg;

struct _CamelImapMsg {
	EMsg msg;

	void (*receive)(CamelImapStore *store, struct _CamelImapMsg *m);
	void (*free)(CamelImapStore *store, struct _CamelImapMsg *m);
};

CamelImapMsg *camel_imap_msg_new(void (*receive)(CamelImapStore *store, struct _CamelImapMsg *m),
				 void (*free)(CamelImapStore *store, struct _CamelImapMsg *m),
				 size_t size);
void camel_imap_msg_queue(CamelImapStore *store, CamelImapMsg *msg);

#endif

#define CAMEL_IMAP_STORE_TYPE     (camel_imap_store_get_type ())
#define CAMEL_IMAP_STORE(obj)     (CAMEL_CHECK_CAST((obj), CAMEL_IMAP_STORE_TYPE, CamelImapStore))
#define CAMEL_IMAP_STORE_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), CAMEL_IMAP_STORE_TYPE, CamelImapStoreClass))
#define CAMEL_IS_IMAP_STORE(o)    (CAMEL_CHECK_TYPE((o), CAMEL_IMAP_STORE_TYPE))

enum {
	CAMEL_IMAP_STORE_ARG_FIRST  = CAMEL_DISCO_STORE_ARG_FIRST + 100,
	CAMEL_IMAP_STORE_ARG_NAMESPACE,
	CAMEL_IMAP_STORE_ARG_OVERRIDE_NAMESPACE,
	CAMEL_IMAP_STORE_ARG_CHECK_ALL,
	CAMEL_IMAP_STORE_ARG_FILTER_INBOX,
	CAMEL_IMAP_STORE_ARG_FILTER_JUNK,
	CAMEL_IMAP_STORE_ARG_FILTER_JUNK_INBOX,
};

#define CAMEL_IMAP_STORE_NAMESPACE           (CAMEL_IMAP_STORE_ARG_NAMESPACE | CAMEL_ARG_STR)
#define CAMEL_IMAP_STORE_OVERRIDE_NAMESPACE  (CAMEL_IMAP_STORE_ARG_OVERRIDE_NAMESPACE | CAMEL_ARG_INT)
#define CAMEL_IMAP_STORE_CHECK_ALL           (CAMEL_IMAP_STORE_ARG_CHECK_ALL | CAMEL_ARG_INT)
#define CAMEL_IMAP_STORE_FILTER_INBOX        (CAMEL_IMAP_STORE_ARG_FILTER_INBOX | CAMEL_ARG_INT)
#define CAMEL_IMAP_STORE_FILTER_JUNK         (CAMEL_IMAP_STORE_ARG_FILTER_JUNK | CAMEL_ARG_BOO)
#define CAMEL_IMAP_STORE_FILTER_JUNK_INBOX   (CAMEL_IMAP_STORE_ARG_FILTER_JUNK_INBOX | CAMEL_ARG_BOO)

/* CamelFolderInfo flags */
#define CAMEL_IMAP_FOLDER_MARKED	     (1<<16)
#define CAMEL_IMAP_FOLDER_UNMARKED	     (1<<17)

typedef enum {
	IMAP_LEVEL_UNKNOWN,
	IMAP_LEVEL_IMAP4,
	IMAP_LEVEL_IMAP4REV1
} CamelImapServerLevel;

#define IMAP_CAPABILITY_IMAP4			(1 << 0)
#define IMAP_CAPABILITY_IMAP4REV1		(1 << 1)
#define IMAP_CAPABILITY_STATUS			(1 << 2)
#define IMAP_CAPABILITY_NAMESPACE		(1 << 3)
#define IMAP_CAPABILITY_UIDPLUS			(1 << 4)
#define IMAP_CAPABILITY_LITERALPLUS		(1 << 5)
#define IMAP_CAPABILITY_STARTTLS                (1 << 6)
#define IMAP_CAPABILITY_useful_lsub		(1 << 7)
#define IMAP_CAPABILITY_utf8_search		(1 << 8)

#define IMAP_PARAM_OVERRIDE_NAMESPACE		(1 << 0)
#define IMAP_PARAM_CHECK_ALL			(1 << 1)
#define IMAP_PARAM_FILTER_INBOX			(1 << 2)
#define IMAP_PARAM_FILTER_JUNK			(1 << 3)
#define IMAP_PARAM_FILTER_JUNK_INBOX		(1 << 4)

struct _CamelImapStore {
	CamelDiscoStore parent_object;	
	
	CamelStream *istream;
	CamelStream *ostream;

	struct _CamelImapStoreSummary *summary;
	
	/* Information about the command channel / connection status */
	guint connected:1;
	guint preauthed:1;
	char tag_prefix;
	guint32 command;
	CamelFolder *current_folder;
	
	/* Information about the server */
	CamelImapServerLevel server_level;
	guint32 capabilities, parameters;
	guint braindamaged:1;
	/* NB: namespace should be handled by summary->namespace */
	char *namespace, dir_sep, *base_url, *storage_path;
	GHashTable *authtypes;
	
	guint renaming:1;
};


typedef struct {
	CamelDiscoStoreClass parent_class;

} CamelImapStoreClass;


/* Standard Camel function */
CamelType camel_imap_store_get_type (void);


gboolean camel_imap_store_connected (CamelImapStore *store, CamelException *ex);

ssize_t camel_imap_store_readline (CamelImapStore *store, char **dest, CamelException *ex);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_IMAP_STORE_H */
