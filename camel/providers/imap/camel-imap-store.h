/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-imap-store.h : class for an imap store */

/* 
 * Authors: Jeffrey Stedfast <fejj@helixcode.com>
 *
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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
#endif /* __cplusplus }*/

#include "camel-remote-store.h"

#define CAMEL_IMAP_STORE_TYPE     (camel_imap_store_get_type ())
#define CAMEL_IMAP_STORE(obj)     (CAMEL_CHECK_CAST((obj), CAMEL_IMAP_STORE_TYPE, CamelImapStore))
#define CAMEL_IMAP_STORE_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), CAMEL_IMAP_STORE_TYPE, CamelImapStoreClass))
#define IS_CAMEL_IMAP_STORE(o)    (CAMEL_CHECK_TYPE((o), CAMEL_IMAP_STORE_TYPE))

typedef enum {
	IMAP_LEVEL_UNKNOWN,
	IMAP_LEVEL_IMAP4,
	IMAP_LEVEL_IMAP4REV1
} CamelImapServerLevel;


typedef struct {
	CamelRemoteStore parent_object;	
	
	CamelFolder *current_folder;
	CamelStream *istream, *ostream;
	
	guint32 command;
	
	CamelImapServerLevel server_level;
	gboolean has_status_capability;
	
	gchar *dir_sep;
	
	guint timeout_id;
} CamelImapStore;


typedef struct {
	CamelRemoteStoreClass parent_class;

} CamelImapStoreClass;


/* public methods */
void camel_imap_store_open (CamelImapStore *store, CamelException *ex);
void camel_imap_store_close (CamelImapStore *store, gboolean expunge, CamelException *ex);

/* support functions */

enum {
	CAMEL_IMAP_OK = 0,
	CAMEL_IMAP_NO,
	CAMEL_IMAP_BAD,
	CAMEL_IMAP_PLUS,
	CAMEL_IMAP_FAIL
};

gint camel_imap_command (CamelImapStore *store, CamelFolder *folder,
			 CamelException *ex, char *fmt, ...);

gint camel_imap_command_extended (CamelImapStore *store, CamelFolder *folder,
				  char **ret, CamelException *ex, char *fmt, ...);

gint camel_imap_fetch_command (CamelImapStore *store, CamelFolder *folder,
			       char **ret, CamelException *ex, char *fmt, ...);

/* multi-transactional commands... */
gint camel_imap_command_preliminary (CamelImapStore *store,
				     char **cmdid,
				     CamelException *ex,
				     char *fmt, ...);

gint camel_imap_command_continuation (CamelImapStore *store,
				      char **ret,
				      char *cmdid,
				      char *cmdbuf,
				      CamelException *ex);

gint camel_imap_command_continuation_with_stream (CamelImapStore *store,
						  char **ret,
						  char *cmdid,
						  CamelStream *cstream,
						  CamelException *ex);

/* Standard Camel function */
CamelType camel_imap_store_get_type (void);

const gchar *camel_imap_store_get_toplevel_dir (CamelImapStore *store);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_IMAP_STORE_H */
