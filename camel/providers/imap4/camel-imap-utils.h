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


#ifndef __CAMEL_IMAP_UTILS_H__
#define __CAMEL_IMAP_UTILS_H__

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

/* IMAP flag merging */
typedef struct {
	guint32 changed;
	guint32 bits;
} flags_diff_t;

void camel_imap_flags_diff (flags_diff_t *diff, guint32 old, guint32 new);
guint32 camel_imap_flags_merge (flags_diff_t *diff, guint32 flags);
guint32 camel_imap_merge_flags (guint32 original, guint32 local, guint32 server);


struct _CamelIMAPEngine;
struct _CamelIMAPCommand;
struct _camel_imap_token_t;

void camel_imap_utils_set_unexpected_token_error (CamelException *ex, struct _CamelIMAPEngine *engine, struct _camel_imap_token_t *token);

int camel_imap_parse_flags_list (struct _CamelIMAPEngine *engine, guint32 *flags, CamelException *ex);

enum {
	CAMEL_IMAP_FOLDER_MARKED          = (1 << 0),
	CAMEL_IMAP_FOLDER_UNMARKED        = (1 << 1),
	CAMEL_IMAP_FOLDER_NOSELECT        = (1 << 2),
	CAMEL_IMAP_FOLDER_NOINFERIORS     = (1 << 3),
	CAMEL_IMAP_FOLDER_HAS_CHILDREN    = (1 << 4),
	CAMEL_IMAP_FOLDER_HAS_NO_CHILDREN = (1 << 5),
};

typedef struct {
	guint32 flags;
	char delim;
	char *name;
} camel_imap_list_t;

int camel_imap_untagged_list (struct _CamelIMAPEngine *engine, struct _CamelIMAPCommand *ic,
			       guint32 index, struct _camel_imap_token_t *token, CamelException *ex);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __CAMEL_IMAP_UTILS_H__ */
