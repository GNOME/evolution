/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002 Ximian Inc.
 *
 * Authors: Jeffrey Stedfast <fejj@ximian.com>
 *	    Michael Zucchi <notzed@ximian.com>
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

#ifndef _CAMEL_MIME_FILTER_CANON_H
#define _CAMEL_MIME_FILTER_CANON_H

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <camel/camel-mime-filter.h>

#define CAMEL_MIME_FILTER_CANON_TYPE         (camel_mime_filter_canon_get_type ())
#define CAMEL_MIME_FILTER_CANON(obj)         CAMEL_CHECK_CAST (obj, CAMEL_MIME_FILTER_CANON_TYPE, CamelMimeFilterCanon)
#define CAMEL_MIME_FILTER_CANON_CLASS(klass) CAMEL_CHECK_CLASS_CAST (klass, CAMEL_MIME_FILTER_CANON_TYPE, CamelMimeFilterCanonClass)
#define CAMEL_IS_MIME_FILTER_CANON(obj)      CAMEL_CHECK_TYPE (obj, CAMEL_MIME_FILTER_CANON_TYPE)

typedef struct _CamelMimeFilterCanon CamelMimeFilterCanon;
typedef struct _CamelMimeFilterCanonClass CamelMimeFilterCanonClass;

enum {
	CAMEL_MIME_FILTER_CANON_CRLF = (1<<0), /* canoncialise end of line to crlf, otherwise canonicalise to lf only */
	CAMEL_MIME_FILTER_CANON_FROM = (1<<1), /* escape "^From " using quoted-printable semantics into "=46rom " */
	CAMEL_MIME_FILTER_CANON_STRIP = (1<<2),	/* strip trailing space */
};

struct _CamelMimeFilterCanon {
	CamelMimeFilter parent;

	guint32 flags;
};

struct _CamelMimeFilterCanonClass {
	CamelMimeFilterClass parent_class;
};

CamelType camel_mime_filter_canon_get_type (void);

CamelMimeFilter *camel_mime_filter_canon_new(guint32 flags);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ! _CAMEL_MIME_FILTER_CANON_H */
