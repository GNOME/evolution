/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2000 Ximian, Inc. (www.ximian.com)
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
 *
 */

#ifndef CAMEL_IMAP_UTILS_H
#define CAMEL_IMAP_UTILS_H 1

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <sys/types.h>

#include "camel-folder-summary.h"
#include "camel-imap-types.h"

const char *imap_next_word (const char *buf);

#define IMAP_LIST_FLAG_NOINFERIORS	(1 << 0)
#define IMAP_LIST_FLAG_NOSELECT		(1 << 1)
#define IMAP_LIST_FLAG_MARKED		(1 << 2)
#define IMAP_LIST_FLAG_UNMARKED		(1 << 3)

gboolean imap_parse_list_response  (CamelImapStore *store, const char *buf, int *flags,
				    char *sep, char **folder);

char   **imap_parse_folder_name    (CamelImapStore *store, const char *folder_name);

char    *imap_create_flag_list     (guint32 flags);
guint32  imap_parse_flag_list      (char **flag_list);


enum { IMAP_STRING, IMAP_NSTRING, IMAP_ASTRING };

char    *imap_parse_string_generic (char **str_p, size_t *len, int type);

#define imap_parse_string(str_p, len_p) \
	imap_parse_string_generic (str_p, len_p, IMAP_STRING)
#define imap_parse_nstring(str_p, len_p) \
	imap_parse_string_generic (str_p, len_p, IMAP_NSTRING)
#define imap_parse_astring(str_p, len_p) \
	imap_parse_string_generic (str_p, len_p, IMAP_ASTRING)

void     imap_parse_body           (char **body_p, CamelFolder *folder,
				    CamelMessageContentInfo *ci);

char    *imap_quote_string         (const char *str);

void     imap_skip_list            (char **str_p);

char    *imap_uid_array_to_set     (CamelFolderSummary *summary, GPtrArray *uids, int uid, ssize_t maxlen, int *lastuid);
GPtrArray *imap_uid_set_to_array   (CamelFolderSummary *summary, const char *uids);
void     imap_uid_array_free       (GPtrArray *arr);

char *imap_concat (CamelImapStore *imap_store, const char *prefix, const char *suffix);
char *imap_namespace_concat (CamelImapStore *store, const char *name);

char *imap_mailbox_encode (const unsigned char *in, size_t inlen);
char *imap_mailbox_decode (const unsigned char *in, size_t inlen);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_IMAP_UTILS_H */
