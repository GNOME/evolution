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

#ifndef _CAMEL_MIME_FILTER_INDEX_H
#define _CAMEL_MIME_FILTER_INDEX_H

#include <camel/camel-mime-filter.h>
#include <libibex/ibex.h>

#define CAMEL_MIME_FILTER_INDEX(obj)         GTK_CHECK_CAST (obj, camel_mime_filter_index_get_type (), CamelMimeFilterIndex)
#define CAMEL_MIME_FILTER_INDEX_CLASS(klass) GTK_CHECK_CLASS_CAST (klass, camel_mime_filter_index_get_type (), CamelMimeFilterIndexClass)
#define IS_CAMEL_MIME_FILTER_INDEX(obj)      GTK_CHECK_TYPE (obj, camel_mime_filter_index_get_type ())

typedef struct _CamelMimeFilterIndexClass CamelMimeFilterIndexClass;

struct _CamelMimeFilterIndex {
	CamelMimeFilter parent;

	struct _CamelMimeFilterIndexPrivate *priv;

	ibex *index;
	char *name;
};

struct _CamelMimeFilterIndexClass {
	CamelMimeFilterClass parent_class;
};

guint		camel_mime_filter_index_get_type	(void);
CamelMimeFilterIndex      *camel_mime_filter_index_new	(void);

CamelMimeFilterIndex      *camel_mime_filter_index_new_ibex (ibex *);

/* Set the match name for any indexed words */
void camel_mime_filter_index_set_name (CamelMimeFilterIndex *, char *);
void camel_mime_filter_index_set_ibex (CamelMimeFilterIndex *mf, ibex *index);

#endif /* ! _CAMEL_MIME_FILTER_INDEX_H */
