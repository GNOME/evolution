/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 * camel-signed--multipart.h : class for a signed-multipart 
 *
 * Authors: Michael Zucchi <notzed@ximian.com>
 *
 * Copyright 2002 Ximian, Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

/* Should this be a subclass of multipart?
   No, because we dont have different parts?
   I'm not really sure yet ... ? */

#ifndef CAMEL_MULTIPART_SIGNED_H
#define CAMEL_MULTIPART_SIGNED_H 1

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <camel/camel-multipart.h>

#define CAMEL_MULTIPART_SIGNED_TYPE     (camel_multipart_signed_get_type ())
#define CAMEL_MULTIPART_SIGNED(obj)     (CAMEL_CHECK_CAST((obj), CAMEL_MULTIPART_SIGNED_TYPE, CamelMultipartSigned))
#define CAMEL_MULTIPART_SIGNED_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), CAMEL_MULTIPART_SIGNED_TYPE, CamelMultipartSignedClass))
#define CAMEL_IS_MULTIPART_SIGNED(o)    (CAMEL_CHECK_TYPE((o), CAMEL_MULTIPART_SIGNED_TYPE))

/*
enum {
	CAMEL_MULTIPART_EMPTY,
	CAMEL_MULTIPART_CONST,
	CAMEL_MULTIPART_SIGN,
	CAMEL_MULTIPART_ENCR,
};
*/

/* 'handy' enums for getting the internal parts of the multipart */
enum {
	CAMEL_MULTIPART_SIGNED_CONTENT,
	CAMEL_MULTIPART_SIGNED_SIGNATURE,
};

typedef struct _CamelMultipartSigned CamelMultipartSigned;

struct _CamelMultipartSigned
{
	CamelMultipart parent_object;

	/* these are the client visible parts, decoded forms of our data wrapper content */
	CamelMimePart *content;
	CamelMimePart *signature;

	/* the raw content which must go over the wire, if we have generated it */
	/* perhaps this should jsut set data_wrapper->stream and update start1/end1 accordingly, as it is done
	   for other parts, or visa versa? */
	CamelStream *contentraw;

	/*int state;*/

	/* just cache some info we use */
	char *protocol;
	char *micalg;

	/* offset pointers of start of boundary in content object */
	off_t start1, end1;
	off_t start2, end2;
};

typedef struct {
	CamelMultipartClass parent_class;
} CamelMultipartSignedClass;

/* Standard Camel function */
CamelType camel_multipart_signed_get_type (void);

/* public methods */
CamelMultipartSigned *camel_multipart_signed_new           (void);

CamelStream *camel_multipart_signed_get_content_stream(CamelMultipartSigned *mps, CamelException *ex);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_MULTIPART_SIGNED_H */


