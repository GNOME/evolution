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



#ifndef CAMEL_IMAP_STREAM_H
#define CAMEL_IMAP_STREAM_H


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <camel/camel-stream.h>
#include <camel/providers/imap/camel-imap-folder.h>
#include <camel/providers/imap/camel-imap-store.h>
#include <sys/types.h>

#define CAMEL_IMAP_STREAM_TYPE     (camel_imap_stream_get_type ())
#define CAMEL_IMAP_STREAM(obj)     (GTK_CHECK_CAST((obj), CAMEL_IMAP_STREAM_TYPE, CamelImapStream))
#define CAMEL_IMAP_STREAM_CLASS(k) (GTK_CHECK_CLASS_CAST ((k), CAMEL_IMAP_STREAM_TYPE, CamelImapStreamClass))
#define CAMEL_IS_IMAP_STREAM(o)    (GTK_CHECK_TYPE((o), CAMEL_IMAP_STREAM_TYPE))

typedef struct _CamelImapStream CamelImapStream;
typedef struct _CamelImapStreamClass CamelImapStreamClass;

struct _CamelImapStream {
	CamelStream parent_object;

	CamelImapFolder *folder;
	char *command;
	char *cache;
	char *cache_ptr;
};

struct _CamelImapStreamClass {
	CamelStreamClass parent_class;

	/* Virtual methods */
};

/* Standard Gtk function */
GtkType camel_imap_stream_get_type (void);

/* public methods */
CamelStream *camel_imap_stream_new (CamelImapFolder *folder, char *command);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_IMAP_STREAM_H */
