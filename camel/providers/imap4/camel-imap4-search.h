/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@novell.com>
 *
 *  Copyright 2004 Novell, Inc. (www.novell.com)
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


#ifndef __CAMEL_IMAP4_SEARCH_H__
#define __CAMEL_IMAP4_SEARCH_H__

#include <camel/camel-folder-search.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define CAMEL_IMAP4_SEARCH_TYPE         (camel_imap4_search_get_type ())
#define CAMEL_IMAP4_SEARCH(obj)         CAMEL_CHECK_CAST (obj, camel_imap4_search_get_type (), CamelIMAP4Search)
#define CAMEL_IMAP4_SEARCH_CLASS(klass) CAMEL_CHECK_CLASS_CAST (klass, camel_imap4_search_get_type (), CamelIMAP4SearchClass)
#define CAMEL_IS_IMAP4_SEARCH(obj)      CAMEL_CHECK_TYPE (obj, camel_imap4_search_get_type ())

typedef struct _CamelIMAP4Search CamelIMAP4Search;
typedef struct _CamelIMAP4SearchClass CamelIMAP4SearchClass;

struct _CamelIMAP4Engine;

struct _CamelIMAP4Search {
	CamelFolderSearch parent_object;
	
	struct _CamelIMAP4Engine *engine;
};

struct _CamelIMAP4SearchClass {
	CamelFolderSearchClass parent_class;
	
};


CamelType camel_imap4_search_get_type (void);

CamelFolderSearch *camel_imap4_search_new (struct _CamelIMAP4Engine *engine, const char *cachedir);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __CAMEL_IMAP4_SEARCH_H__ */
