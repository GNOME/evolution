/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  Camel
 *  Copyright (C) 1999-2004 Jeffrey Stedfast
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
 */


#ifndef __CAMEL_IMAP4_ENGINE_H__
#define __CAMEL_IMAP4_ENGINE_H__

#include <stdarg.h>

#include <glib.h>

#include <libedataserver/e-msgport.h>

#include <camel/camel-stream.h>
#include <camel/camel-folder.h>
#include <camel/camel-session.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define CAMEL_TYPE_IMAP4_ENGINE            (camel_imap4_engine_get_type ())
#define CAMEL_IMAP4_ENGINE(obj)            (CAMEL_CHECK_CAST ((obj), CAMEL_TYPE_IMAP4_ENGINE, CamelIMAP4Engine))
#define CAMEL_IMAP4_ENGINE_CLASS(klass)    (CAMEL_CHECK_CLASS_CAST ((klass), CAMEL_TYPE_IMAP4_ENGINE, CamelIMAP4EngineClass))
#define CAMEL_IS_IMAP4_ENGINE(obj)         (CAMEL_CHECK_TYPE ((obj), CAMEL_TYPE_IMAP4_ENGINE))
#define CAMEL_IS_IMAP4_ENGINE_CLASS(klass) (CAMEL_CHECK_CLASS_TYPE ((klass), CAMEL_TYPE_IMAP4_ENGINE))
#define CAMEL_IMAP4_ENGINE_GET_CLASS(obj)  (CAMEL_CHECK_GET_CLASS ((obj), CAMEL_TYPE_IMAP4_ENGINE, CamelIMAP4EngineClass))

typedef struct _CamelIMAP4Engine CamelIMAP4Engine;
typedef struct _CamelIMAP4EngineClass CamelIMAP4EngineClass;

struct _camel_imap4_token_t;
struct _CamelIMAP4Command;
struct _CamelIMAP4Folder;
struct _CamelIMAP4Stream;

typedef enum {
	CAMEL_IMAP4_ENGINE_DISCONNECTED,
	CAMEL_IMAP4_ENGINE_CONNECTED,
	CAMEL_IMAP4_ENGINE_PREAUTH,
	CAMEL_IMAP4_ENGINE_AUTHENTICATED,
	CAMEL_IMAP4_ENGINE_SELECTED,
} camel_imap4_engine_t;

typedef enum {
	CAMEL_IMAP4_LEVEL_UNKNOWN,
	CAMEL_IMAP4_LEVEL_IMAP4,
	CAMEL_IMAP4_LEVEL_IMAP4REV1
} camel_imap4_level_t;

enum {
	CAMEL_IMAP4_CAPABILITY_IMAP4            = (1 << 0),
	CAMEL_IMAP4_CAPABILITY_IMAP4REV1        = (1 << 1),
	CAMEL_IMAP4_CAPABILITY_STATUS           = (1 << 2),
	CAMEL_IMAP4_CAPABILITY_NAMESPACE        = (1 << 3),
	CAMEL_IMAP4_CAPABILITY_UIDPLUS          = (1 << 4),
	CAMEL_IMAP4_CAPABILITY_LITERALPLUS      = (1 << 5),
	CAMEL_IMAP4_CAPABILITY_LOGINDISABLED    = (1 << 6),
	CAMEL_IMAP4_CAPABILITY_STARTTLS         = (1 << 7),
	CAMEL_IMAP4_CAPABILITY_IDLE             = (1 << 8),
	CAMEL_IMAP4_CAPABILITY_QUOTA            = (1 << 9),
	CAMEL_IMAP4_CAPABILITY_ACL              = (1 << 10),
	CAMEL_IMAP4_CAPABILITY_MULTIAPPEND      = (1 << 11),
	CAMEL_IMAP4_CAPABILITY_useful_lsub      = (1 << 12),
	CAMEL_IMAP4_CAPABILITY_utf8_search      = (1 << 13),
};

typedef enum {
	CAMEL_IMAP4_RESP_CODE_ALERT,
	CAMEL_IMAP4_RESP_CODE_BADCHARSET,
	CAMEL_IMAP4_RESP_CODE_CAPABILITY,
	CAMEL_IMAP4_RESP_CODE_PARSE,
	CAMEL_IMAP4_RESP_CODE_PERM_FLAGS,
	CAMEL_IMAP4_RESP_CODE_READONLY,
	CAMEL_IMAP4_RESP_CODE_READWRITE,
	CAMEL_IMAP4_RESP_CODE_TRYCREATE,
	CAMEL_IMAP4_RESP_CODE_UIDNEXT,
	CAMEL_IMAP4_RESP_CODE_UIDVALIDITY,
	CAMEL_IMAP4_RESP_CODE_UNSEEN,
	CAMEL_IMAP4_RESP_CODE_NEWNAME,
	CAMEL_IMAP4_RESP_CODE_APPENDUID,
	CAMEL_IMAP4_RESP_CODE_COPYUID,
	CAMEL_IMAP4_RESP_CODE_UNKNOWN,
} camel_imap4_resp_code_t;

typedef struct _CamelIMAP4RespCode {
	camel_imap4_resp_code_t code;
	union {
		guint32 flags;
		char *parse;
		guint32 uidnext;
		guint32 uidvalidity;
		guint32 unseen;
		char *newname[2];
		struct {
			guint32 uidvalidity;
			guint32 uid;
		} appenduid;
		struct {
			guint32 uidvalidity;
			char *srcset;
			char *destset;
		} copyuid;
	} v;
} CamelIMAP4RespCode;

enum {
	CAMEL_IMAP4_UNTAGGED_ERROR = -1,
	CAMEL_IMAP4_UNTAGGED_OK,
	CAMEL_IMAP4_UNTAGGED_NO,
	CAMEL_IMAP4_UNTAGGED_BAD,
	CAMEL_IMAP4_UNTAGGED_PREAUTH,
	CAMEL_IMAP4_UNTAGGED_HANDLED,
};

typedef struct _CamelIMAP4Namespace {
	struct _CamelIMAP4Namespace *next;
	char *path;
	char sep;
} CamelIMAP4Namespace;

typedef struct _CamelIMAP4NamespaceList {
	CamelIMAP4Namespace *personal;
	CamelIMAP4Namespace *other;
	CamelIMAP4Namespace *shared;
} CamelIMAP4NamespaceList;

enum {
	CAMEL_IMAP4_ENGINE_MAXLEN_LINE,
	CAMEL_IMAP4_ENGINE_MAXLEN_TOKEN
};

typedef gboolean (* CamelIMAP4ReconnectFunc) (CamelIMAP4Engine *engine, CamelException *ex);

struct _CamelIMAP4Engine {
	CamelObject parent_object;
	
	CamelIMAP4ReconnectFunc reconnect;
	gboolean reconnecting;
	
	CamelSession *session;
	CamelService *service;
	CamelURL *url;
	
	camel_imap4_engine_t state;
	camel_imap4_level_t level;
	guint32 capa;
	
	guint32 maxlen:31;
	guint32 maxlentype:1;
	
	CamelIMAP4NamespaceList namespaces;
	GHashTable *authtypes;                    /* supported authtypes */
	
	struct _CamelIMAP4Stream *istream;
	CamelStream *ostream;
	
	unsigned char tagprefix;             /* 'A'..'Z' */
	unsigned int tag;                    /* next command tag */
	int nextid;
	
	struct _CamelIMAP4Folder *folder;    /* currently selected folder */
	
	EDList queue;                          /* queue of waiting commands */
	struct _CamelIMAP4Command *current;
};

struct _CamelIMAP4EngineClass {
	CamelObjectClass parent_class;
	
	unsigned char tagprefix;
};


CamelType camel_imap4_engine_get_type (void);

CamelIMAP4Engine *camel_imap4_engine_new (CamelService *service, CamelIMAP4ReconnectFunc reconnect);

/* returns 0 on success or -1 on error */
int camel_imap4_engine_take_stream (CamelIMAP4Engine *engine, CamelStream *stream, CamelException *ex);

int camel_imap4_engine_capability (CamelIMAP4Engine *engine, CamelException *ex);
int camel_imap4_engine_namespace (CamelIMAP4Engine *engine, CamelException *ex);

int camel_imap4_engine_select_folder (CamelIMAP4Engine *engine, CamelFolder *folder, CamelException *ex);

struct _CamelIMAP4Command *camel_imap4_engine_queue (CamelIMAP4Engine *engine, CamelFolder *folder,
						     const char *format, ...);
struct _CamelIMAP4Command *camel_imap4_engine_prequeue (CamelIMAP4Engine *engine, CamelFolder *folder,
							const char *format, ...);

void camel_imap4_engine_dequeue (CamelIMAP4Engine *engine, struct _CamelIMAP4Command *ic);

int camel_imap4_engine_iterate (CamelIMAP4Engine *engine);


/* untagged response utility functions */
int camel_imap4_engine_handle_untagged_1 (CamelIMAP4Engine *engine, struct _camel_imap4_token_t *token, CamelException *ex);
void camel_imap4_engine_handle_untagged (CamelIMAP4Engine *engine, CamelException *ex);

/* stream wrapper utility functions */
int camel_imap4_engine_next_token (CamelIMAP4Engine *engine, struct _camel_imap4_token_t *token, CamelException *ex);
int camel_imap4_engine_line (CamelIMAP4Engine *engine, unsigned char **line, size_t *len, CamelException *ex);
int camel_imap4_engine_literal (CamelIMAP4Engine *engine, unsigned char **literal, size_t *len, CamelException *ex);
int camel_imap4_engine_nstring (CamelIMAP4Engine *engine, unsigned char **nstring, CamelException *ex);
int camel_imap4_engine_eat_line (CamelIMAP4Engine *engine, CamelException *ex);


/* response code stuff */
int camel_imap4_engine_parse_resp_code (CamelIMAP4Engine *engine, CamelException *ex);
void camel_imap4_resp_code_free (CamelIMAP4RespCode *rcode);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __CAMEL_IMAP4_ENGINE_H__ */
