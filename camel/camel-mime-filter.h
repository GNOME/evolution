/*
 *  Copyright (C) 2000 Helix Code Inc.
 *
 *  Authors: Michael Zucchi <notzed@helixcode.com>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License
 *  as published by the Free Software Foundation; either version 2 of
 *  the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* Abstract class for non-copying filters */

#ifndef _CAMEL_MIME_FILTER_H
#define _CAMEL_MIME_FILTER_H

#include <camel/camel-object.h>
#include <sys/types.h>

#define CAMEL_MIME_FILTER_TYPE         (camel_mime_filter_get_type ())
#define CAMEL_MIME_FILTER(obj)         CAMEL_CHECK_CAST (obj, camel_mime_filter_get_type (), CamelMimeFilter)
#define CAMEL_MIME_FILTER_CLASS(klass) CAMEL_CHECK_CLASS_CAST (klass, camel_mime_filter_get_type (), CamelMimeFilterClass)
#define IS_CAMEL_MIME_FILTER(obj)      CAMEL_CHECK_TYPE (obj, camel_mime_filter_get_type ())

typedef struct _CamelMimeFilterClass CamelMimeFilterClass;

struct _CamelMimeFilter {
	CamelObject parent;

	struct _CamelMimeFilterPrivate *priv;

	char *outreal;		/* real malloc'd buffer */
	char *outbuf;		/* first 'writable' position allowed (outreal + outpre) */
	char *outptr;
	int outsize;
	int outpre;		/* prespace of this buffer */

	char *backbuf;
	int backsize;
	int backlen;		/* significant data there */
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

#endif /* ! _CAMEL_MIME_FILTER_H */
