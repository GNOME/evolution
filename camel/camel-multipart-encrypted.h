/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002 Ximian, Inc. (www.ximian.com)
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


#ifndef __CAMEL_MULTIPART_ENCRYPTED_H__
#define __CAMEL_MULTIPART_ENCRYPTED_H__

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <camel/camel-multipart.h>

#define CAMEL_MULTIPART_ENCRYPTED_TYPE     (camel_multipart_encrypted_get_type ())
#define CAMEL_MULTIPART_ENCRYPTED(obj)     (CAMEL_CHECK_CAST((obj), CAMEL_MULTIPART_ENCRYPTED_TYPE, CamelMultipartEncrypted))
#define CAMEL_MULTIPART_ENCRYPTED_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), CAMEL_MULTIPART_ENCRYPTED_TYPE, CamelMultipartEncryptedClass))
#define CAMEL_IS_MULTIPART_ENCRYPTED(o)    (CAMEL_CHECK_TYPE((o), CAMEL_MULTIPART_ENCRYPTED_TYPE))

typedef struct _CamelMultipartEncrypted CamelMultipartEncrypted;
typedef struct _CamelMultipartEncryptedClass CamelMultipartEncryptedClass;

/* 'handy' enums for getting the internal parts of the multipart */
enum {
	CAMEL_MULTIPART_ENCRYPTED_VERSION,
	CAMEL_MULTIPART_ENCRYPTED_CONTENT,
};

struct _CamelMultipartEncrypted {
	CamelMultipart parent_object;
	
	CamelMimePart *version;
	CamelMimePart *content;
	CamelMimePart *decrypted;
	
	char *protocol;
};

struct _CamelMultipartEncryptedClass {
	CamelMultipartClass parent_class;
	
};

CamelType camel_multipart_encrypted_get_type (void);

CamelMultipartEncrypted *camel_multipart_encrypted_new (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __CAMEL_MULTIPART_ENCRYPTED_H__ */
