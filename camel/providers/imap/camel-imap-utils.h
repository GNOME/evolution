/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@helixcode.com>
 *
 *  Copyright 2000 Helix Code, Inc. (www.helixcode.com)
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

#ifndef CAMEL_IMAP_UTILS_H
#define CAMEL_IMAP_UTILS_H 1

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <glib.h>

char *imap_next_word (const char *buf);

#define IMAP_LIST_FLAG_NOINFERIORS	(1 << 0)
#define IMAP_LIST_FLAG_NOSELECT		(1 << 1)
#define IMAP_LIST_FLAG_MARKED		(1 << 2)
#define IMAP_LIST_FLAG_UNMARKED		(1 << 3)
gboolean imap_parse_list_response (const char *buf, int *flags, char *sep, char **folder);

char *imap_create_flag_list (guint32 flags);
guint32 imap_parse_flag_list (const char *flag_list);

char *imap_parse_nstring (char **str_p, int *len);
char *imap_parse_astring (char **str_p, int *len);

char *imap_quote_string (const char *str);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_IMAP_UTILS_H */
