/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
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

#ifndef _CAMEL_DIGEST_FOLDER_H
#define _CAMEL_DIGEST_FOLDER_H

#include <glib.h>
#include <camel/camel-store.h>
#include <camel/camel-folder.h>
#include <camel/camel-mime-message.h>

#define CAMEL_DIGEST_FOLDER(obj)         CAMEL_CHECK_CAST (obj, camel_digest_folder_get_type (), CamelDigestFolder)
#define CAMEL_DIGEST_FOLDER_CLASS(klass) CAMEL_CHECK_CLASS_CAST (klass, camel_digest_folder_get_type (), CamelDigestFolderClass)
#define CAMEL_IS_DIGEST_FOLDER(obj)      CAMEL_CHECK_TYPE (obj, camel_digest_folder_get_type ())

typedef struct _CamelDigestFolderClass CamelDigestFolderClass;

struct _CamelDigestFolder {
	CamelFolder parent;
	
	struct _CamelDigestFolderPrivate *priv;
};

struct _CamelDigestFolderClass {
	CamelFolderClass parent_class;
	
};

CamelType    camel_digest_folder_get_type (void);

CamelFolder *camel_digest_folder_new      (CamelStore *parent_store, CamelMimeMessage *message);

#endif /* ! _CAMEL_DIGEST_FOLDER_H */
