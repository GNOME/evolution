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


#ifndef __CAMEL_IMAP4_COMMAND_H__
#define __CAMEL_IMAP4_COMMAND_H__

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

struct _CamelIMAP4Engine;
struct _CamelIMAP4Folder;
struct _camel_imap4_token_t;

typedef struct _CamelIMAP4Command CamelIMAP4Command;
typedef struct _CamelIMAP4Literal CamelIMAP4Literal;

typedef int (* CamelIMAP4PlusCallback) (struct _CamelIMAP4Engine *engine,
					CamelIMAP4Command *ic,
					const unsigned char *linebuf,
					size_t linelen, CamelException *ex);

typedef int (* CamelIMAP4UntaggedCallback) (struct _CamelIMAP4Engine *engine,
					    CamelIMAP4Command *ic,
					    guint32 index,
					    struct _camel_imap4_token_t *token,
					    CamelException *ex);

enum {
	CAMEL_IMAP4_LITERAL_STRING,
	CAMEL_IMAP4_LITERAL_STREAM,
	CAMEL_IMAP4_LITERAL_WRAPPER,
};

struct _CamelIMAP4Literal {
	int type;
	union {
		char *string;
		CamelStream *stream;
		CamelDataWrapper *wrapper;
	} literal;
};

typedef struct _CamelIMAP4CommandPart {
	struct _CamelIMAP4CommandPart *next;
	unsigned char *buffer;
	size_t buflen;
	
	CamelIMAP4Literal *literal;
} CamelIMAP4CommandPart;

enum {
	CAMEL_IMAP4_COMMAND_QUEUED,
	CAMEL_IMAP4_COMMAND_ACTIVE,
	CAMEL_IMAP4_COMMAND_COMPLETE,
	CAMEL_IMAP4_COMMAND_ERROR,
};

enum {
	CAMEL_IMAP4_RESULT_NONE,
	CAMEL_IMAP4_RESULT_OK,
	CAMEL_IMAP4_RESULT_NO,
	CAMEL_IMAP4_RESULT_BAD,
};

struct _CamelIMAP4Command {
	EDListNode node;
	
	struct _CamelIMAP4Engine *engine;
	
	unsigned int ref_count:26;
	unsigned int status:3;
	unsigned int result:3;
	int id;
	
	char *tag;
	
	GPtrArray *resp_codes;
	
	struct _CamelIMAP4Folder *folder;
	CamelException ex;
	
	/* command parts - logical breaks in the overall command based on literals */
	CamelIMAP4CommandPart *parts;
	
	/* current part */
	CamelIMAP4CommandPart *part;
	
	/* untagged handlers */
	GHashTable *untagged;
	
	/* '+' callback/data */
	CamelIMAP4PlusCallback plus;
	void *user_data;
};

CamelIMAP4Command *camel_imap4_command_new (struct _CamelIMAP4Engine *engine, struct _CamelIMAP4Folder *folder,
					    const char *format, ...);
CamelIMAP4Command *camel_imap4_command_newv (struct _CamelIMAP4Engine *engine, struct _CamelIMAP4Folder *folder,
					     const char *format, va_list args);

void camel_imap4_command_register_untagged (CamelIMAP4Command *ic, const char *atom, CamelIMAP4UntaggedCallback untagged);

void camel_imap4_command_ref (CamelIMAP4Command *ic);
void camel_imap4_command_unref (CamelIMAP4Command *ic);

/* returns 1 when complete, 0 if there is more to do, or -1 on error */
int camel_imap4_command_step (CamelIMAP4Command *ic);

void camel_imap4_command_reset (CamelIMAP4Command *ic);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __CAMEL_IMAP4_COMMAND_H__ */
