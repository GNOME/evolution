/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright (C) 2000 Ximian Inc.
 *
 *  Authors: Michael Zucchi <notzed@ximian.com>
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
 */

/* Abstract class for non-copying filters */

#ifndef _CAMEL_MIME_FILTER_H
#define _CAMEL_MIME_FILTER_H

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <sys/types.h>
#include <camel/camel-object.h>

#define CAMEL_MIME_FILTER_TYPE         (camel_mime_filter_get_type ())
#define CAMEL_MIME_FILTER(obj)         CAMEL_CHECK_CAST (obj, camel_mime_filter_get_type (), CamelMimeFilter)
#define CAMEL_MIME_FILTER_CLASS(klass) CAMEL_CHECK_CLASS_CAST (klass, camel_mime_filter_get_type (), CamelMimeFilterClass)
#define CAMEL_IS_MIME_FILTER(obj)      CAMEL_CHECK_TYPE (obj, camel_mime_filter_get_type ())

typedef struct _CamelMimeFilterClass CamelMimeFilterClass;

struct _CamelMimeFilter {
	CamelObject parent;

	struct _CamelMimeFilterPrivate *priv;

	char *outreal;		/* real malloc'd buffer */
	char *outbuf;		/* first 'writable' position allowed (outreal + outpre) */
	char *outptr;
	size_t outsize;
	size_t outpre;		/* prespace of this buffer */

	char *backbuf;
	size_t backsize;
	size_t backlen;		/* significant data there */
};

struct _CamelMimeFilterClass {
	CamelObjectClass parent_class;

	/* virtual functions */
	void (*filter)(CamelMimeFilter *f,
		       char *in, size_t len, size_t prespace,
		       char **out, size_t *outlen, size_t *outprespace);
	void (*complete)(CamelMimeFilter *f,
			 char *in, size_t len, size_t prespace,
			 char **out, size_t *outlen, size_t *outprespace);
	void (*reset)(CamelMimeFilter *f);
};

CamelType	      camel_mime_filter_get_type	(void);
CamelMimeFilter      *camel_mime_filter_new	(void);

void camel_mime_filter_filter(CamelMimeFilter *f,
			      char *in, size_t len, size_t prespace,
			      char **out, size_t *outlen, size_t *outprespace);

void camel_mime_filter_complete(CamelMimeFilter *f,
				char *in, size_t len, size_t prespace,
				char **out, size_t *outlen, size_t *outprespace);

void camel_mime_filter_reset(CamelMimeFilter *f);

/* sets/returns number of bytes backed up on the input */
void camel_mime_filter_backup(CamelMimeFilter *f, const char *data, size_t length);

/* ensure this much size available for filter output */
void camel_mime_filter_set_size(CamelMimeFilter *f, size_t size, int keep);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ! _CAMEL_MIME_FILTER_H */
