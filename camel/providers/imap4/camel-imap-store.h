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


#ifndef __CAMEL_IMAP_STORE_H__
#define __CAMEL_IMAP_STORE_H__

#include <camel/camel-store.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define CAMEL_TYPE_IMAP_STORE            (camel_imap_store_get_type ())
#define CAMEL_IMAP_STORE(obj)            (CAMEL_CHECK_CAST ((obj), CAMEL_TYPE_IMAP_STORE, CamelIMAPStore))
#define CAMEL_IMAP_STORE_CLASS(klass)    (CAMEL_CHECK_CLASS_CAST ((klass), CAMEL_TYPE_IMAP_STORE, CamelIMAPStoreClass))
#define CAMEL_IS_IMAP_STORE(obj)         (CAMEL_CHECK_TYPE ((obj), CAMEL_TYPE_IMAP_STORE))
#define CAMEL_IS_IMAP_STORE_CLASS(klass) (CAMEL_CHECK_CLASS_TYPE ((klass), CAMEL_TYPE_IMAP_STORE))
#define CAMEL_IMAP_STORE_GET_CLASS(obj)  (CAMEL_CHECK_GET_CLASS ((obj), CAMEL_TYPE_IMAP_STORE, CamelIMAPStoreClass))

typedef struct _CamelIMAPStore CamelIMAPStore;
typedef struct _CamelIMAPStoreClass CamelIMAPStoreClass;

struct _CamelIMAPEngine;

struct _CamelIMAPStore {
	CamelStore parent_object;
	
	struct _CamelIMAPEngine *engine;
};

struct _CamelIMAPStoreClass {
	CamelStoreClass parent_class;
	
};


CamelType camel_imap_store_get_type (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __CAMEL_IMAP_STORE_H__ */
