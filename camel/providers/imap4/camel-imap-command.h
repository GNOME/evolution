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


#ifndef __CAMEL_IMAP_COMMAND_H__
#define __CAMEL_IMAP_COMMAND_H__

#include <stdarg.h>

#include <glib.h>

#include <e-util/e-msgport.h>

#include <camel/camel-stream.h>
#include <camel/camel-exception.h>
#include <camel/camel-data-wrapper.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

struct _CamelIMAPEngine;
struct _CamelIMAPFolder;
struct _camel_imap_token_t;

typedef struct _CamelIMAPCommand CamelIMAPCommand;
typedef struct _CamelIMAPLiteral CamelIMAPLiteral;

typedef int (* CamelIMAPPlusCallback) (struct _CamelIMAPEngine *engine,
				       CamelIMAPCommand *ic,
				       const unsigned char *linebuf,
				       size_t linelen, CamelException *ex);

typedef int (* CamelIMAPUntaggedCallback) (struct _CamelIMAPEngine *engine,
					   CamelIMAPCommand *ic,
					   guint32 index,
					   struct _camel_imap_token_t *token,
					   CamelException *ex);

enum {
	CAMEL_IMAP_LITERAL_STRING,
	CAMEL_IMAP_LITERAL_STREAM,
	CAMEL_IMAP_LITERAL_WRAPPER,
};

struct _CamelIMAPLiteral {
	int type;
	union {
		char *string;
		CamelStream *stream;
		CamelDataWrapper *wrapper;
	} literal;
};

typedef struct _CamelIMAPCommandPart {
	struct _CamelIMAPCommandPart *next;
	unsigned char *buffer;
	size_t buflen;
	
	CamelIMAPLiteral *literal;
} CamelIMAPCommandPart;

enum {
	CAMEL_IMAP_COMMAND_QUEUED,
	CAMEL_IMAP_COMMAND_ACTIVE,
	CAMEL_IMAP_COMMAND_COMPLETE,
	CAMEL_IMAP_COMMAND_ERROR,
};

enum {
	CAMEL_IMAP_RESULT_NONE,
	CAMEL_IMAP_RESULT_OK,
	CAMEL_IMAP_RESULT_NO,
	CAMEL_IMAP_RESULT_BAD,
};

struct _CamelIMAPCommand {
	EDListNode node;
	
	struct _CamelIMAPEngine *engine;
	
	unsigned int ref_count:26;
	unsigned int status:3;
	unsigned int result:3;
	int id;
	
	char *tag;
	
	GPtrArray *resp_codes;
	
	struct _CamelIMAPFolder *folder;
	CamelException ex;
	
	/* command parts - logical breaks in the overall command based on literals */
	CamelIMAPCommandPart *parts;
	
	/* current part */
	CamelIMAPCommandPart *part;
	
	/* untagged handlers */
	GHashTable *untagged;
	
	/* '+' callback/data */
	CamelIMAPPlusCallback plus;
	void *user_data;
};

CamelIMAPCommand *camel_imap_command_new (struct _CamelIMAPEngine *engine, struct _CamelIMAPFolder *folder,
					  const char *format, ...);
CamelIMAPCommand *camel_imap_command_newv (struct _CamelIMAPEngine *engine, struct _CamelIMAPFolder *folder,
					   const char *format, va_list args);

void camel_imap_command_register_untagged (CamelIMAPCommand *ic, const char *atom, CamelIMAPUntaggedCallback untagged);

void camel_imap_command_ref (CamelIMAPCommand *ic);
void camel_imap_command_unref (CamelIMAPCommand *ic);

/* returns 1 when complete, 0 if there is more to do, or -1 on error */
int camel_imap_command_step (CamelIMAPCommand *ic);

void camel_imap_command_reset (CamelIMAPCommand *ic);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __CAMEL_IMAP_COMMAND_H__ */
