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


#ifndef __CAMEL_IMAP_ENGINE_H__
#define __CAMEL_IMAP_ENGINE_H__

#include <stdarg.h>

#include <glib.h>

#include <e-util/e-msgport.h>

#include <camel/camel-stream.h>
#include <camel/camel-folder.h>
#include <camel/camel-session.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define CAMEL_TYPE_IMAP_ENGINE            (camel_imap_engine_get_type ())
#define CAMEL_IMAP_ENGINE(obj)            (CAMEL_CHECK_CAST ((obj), CAMEL_TYPE_IMAP_ENGINE, CamelIMAPEngine))
#define CAMEL_IMAP_ENGINE_CLASS(klass)    (CAMEL_CHECK_CLASS_CAST ((klass), CAMEL_TYPE_IMAP_ENGINE, CamelIMAPEngineClass))
#define CAMEL_IS_IMAP_ENGINE(obj)         (CAMEL_CHECK_TYPE ((obj), CAMEL_TYPE_IMAP_ENGINE))
#define CAMEL_IS_IMAP_ENGINE_CLASS(klass) (CAMEL_CHECK_CLASS_TYPE ((klass), CAMEL_TYPE_IMAP_ENGINE))
#define CAMEL_IMAP_ENGINE_GET_CLASS(obj)  (CAMEL_CHECK_GET_CLASS ((obj), CAMEL_TYPE_IMAP_ENGINE, CamelIMAPEngineClass))

typedef struct _CamelIMAPEngine CamelIMAPEngine;
typedef struct _CamelIMAPEngineClass CamelIMAPEngineClass;

struct _camel_imap_token_t;
struct _CamelIMAPCommand;
struct _CamelIMAPFolder;
struct _CamelIMAPStream;

typedef enum {
	CAMEL_IMAP_ENGINE_DISCONNECTED,
	CAMEL_IMAP_ENGINE_CONNECTED,
	CAMEL_IMAP_ENGINE_PREAUTH,
	CAMEL_IMAP_ENGINE_AUTHENTICATED,
	CAMEL_IMAP_ENGINE_SELECTED,
} camel_imap_engine_t;

typedef enum {
	CAMEL_IMAP_LEVEL_UNKNOWN,
	CAMEL_IMAP_LEVEL_IMAP4,
	CAMEL_IMAP_LEVEL_IMAP4REV1
} camel_imap_level_t;

enum {
	CAMEL_IMAP_CAPABILITY_IMAP4           = (1 << 0),
	CAMEL_IMAP_CAPABILITY_IMAP4REV1       = (1 << 1),
	CAMEL_IMAP_CAPABILITY_STATUS          = (1 << 2),
	CAMEL_IMAP_CAPABILITY_NAMESPACE       = (1 << 3),
	CAMEL_IMAP_CAPABILITY_UIDPLUS         = (1 << 4),
	CAMEL_IMAP_CAPABILITY_LITERALPLUS     = (1 << 5),
	CAMEL_IMAP_CAPABILITY_LOGINDISABLED   = (1 << 6),
	CAMEL_IMAP_CAPABILITY_STARTTLS        = (1 << 7),
	CAMEL_IMAP_CAPABILITY_useful_lsub     = (1 << 8),
	CAMEL_IMAP_CAPABILITY_utf8_search     = (1 << 9),
};

typedef enum {
	CAMEL_IMAP_RESP_CODE_ALERT,
	CAMEL_IMAP_RESP_CODE_BADCHARSET,
	CAMEL_IMAP_RESP_CODE_CAPABILITY,
	CAMEL_IMAP_RESP_CODE_PARSE,
	CAMEL_IMAP_RESP_CODE_PERM_FLAGS,
	CAMEL_IMAP_RESP_CODE_READONLY,
	CAMEL_IMAP_RESP_CODE_READWRITE,
	CAMEL_IMAP_RESP_CODE_TRYCREATE,
	CAMEL_IMAP_RESP_CODE_UIDNEXT,
	CAMEL_IMAP_RESP_CODE_UIDVALIDITY,
	CAMEL_IMAP_RESP_CODE_UNSEEN,
	CAMEL_IMAP_RESP_CODE_NEWNAME,
	CAMEL_IMAP_RESP_CODE_APPENDUID,
	CAMEL_IMAP_RESP_CODE_COPYUID,
	CAMEL_IMAP_RESP_CODE_UNKNOWN,
} camel_imap_resp_code_t;

typedef struct _CamelIMAPRespCode {
	camel_imap_resp_code_t code;
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
} CamelIMAPRespCode;

enum {
	CAMEL_IMAP_UNTAGGED_ERROR = -1,
	CAMEL_IMAP_UNTAGGED_OK,
	CAMEL_IMAP_UNTAGGED_NO,
	CAMEL_IMAP_UNTAGGED_BAD,
	CAMEL_IMAP_UNTAGGED_PREAUTH,
	CAMEL_IMAP_UNTAGGED_HANDLED,
};

typedef struct _CamelIMAPNamespace {
	struct _CamelIMAPNamespace *next;
	char *path;
	char sep;
} CamelIMAPNamespace;

typedef struct _CamelIMAPNamespaceList {
	CamelIMAPNamespace *personal;
	CamelIMAPNamespace *other;
	CamelIMAPNamespace *shared;
} CamelIMAPNamespaceList;

enum {
	CAMEL_IMAP_ENGINE_MAXLEN_LINE,
	CAMEL_IMAP_ENGINE_MAXLEN_TOKEN
};

struct _CamelIMAPEngine {
	CamelObject parent_object;
	
	CamelSession *session;
	CamelURL *url;
	
	camel_imap_engine_t state;
	camel_imap_level_t level;
	guint32 capa;
	
	guint32 maxlen:31;
	guint32 maxlentype:1;
	
	CamelIMAPNamespaceList namespaces;
	GHashTable *authtypes;                    /* supported authtypes */
	
	struct _CamelIMAPStream *istream;
	CamelStream *ostream;
	
	unsigned char tagprefix;             /* 'A'..'Z' */
	unsigned int tag;                    /* next command tag */
	int nextid;
	
	struct _CamelIMAPFolder *folder;    /* currently selected folder */
	
	EDList queue;                          /* queue of waiting commands */
	struct _CamelIMAPCommand *current;
};

struct _CamelIMAPEngineClass {
	CamelObjectClass parent_class;
	
	unsigned char tagprefix;
};


CamelType camel_imap_engine_get_type (void);

CamelIMAPEngine *camel_imap_engine_new (CamelSession *session, CamelURL *url);

/* returns 0 on success or -1 on error */
int camel_imap_engine_take_stream (CamelIMAPEngine *engine, CamelStream *stream, CamelException *ex);

int camel_imap_engine_capability (CamelIMAPEngine *engine, CamelException *ex);
int camel_imap_engine_namespace (CamelIMAPEngine *engine, CamelException *ex);

int camel_imap_engine_select_folder (CamelIMAPEngine *engine, CamelFolder *folder, CamelException *ex);

struct _CamelIMAPCommand *camel_imap_engine_queue (CamelIMAPEngine *engine, CamelFolder *folder,
						   const char *format, ...);
void camel_imap_engine_prequeue (CamelIMAPEngine *engine, struct _CamelIMAPCommand *ic);

void camel_imap_engine_dequeue (CamelIMAPEngine *engine, struct _CamelIMAPCommand *ic);

int camel_imap_engine_iterate (CamelIMAPEngine *engine);


/* untagged response utility functions */
int camel_imap_engine_handle_untagged_1 (CamelIMAPEngine *engine, struct _camel_imap_token_t *token, CamelException *ex);
void camel_imap_engine_handle_untagged (CamelIMAPEngine *engine, CamelException *ex);

/* stream wrapper utility functions */
int camel_imap_engine_next_token (CamelIMAPEngine *engine, struct _camel_imap_token_t *token, CamelException *ex);
int camel_imap_engine_line (CamelIMAPEngine *engine, unsigned char **line, size_t *len, CamelException *ex);
int camel_imap_engine_eat_line (CamelIMAPEngine *engine, CamelException *ex);


/* response code stuff */
int camel_imap_engine_parse_resp_code (CamelIMAPEngine *engine, CamelException *ex);
void camel_imap_resp_code_free (CamelIMAPRespCode *rcode);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __CAMEL_IMAP_ENGINE_H__ */
