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

#ifndef _CAMEL_MIME_FILTER_SAVE_H
#define _CAMEL_MIME_FILTER_SAVE_H

#include <camel/camel-mime-filter.h>

#define CAMEL_MIME_FILTER_SAVE(obj)         CAMEL_CHECK_CAST (obj, camel_mime_filter_save_get_type (), CamelMimeFilterSave)
#define CAMEL_MIME_FILTER_SAVE_CLASS(klass) CAMEL_CHECK_CLASS_CAST (klass, camel_mime_filter_save_get_type (), CamelMimeFilterSaveClass)
#define CAMEL_IS_MIME_FILTER_SAVE(obj)      CAMEL_CHECK_TYPE (obj, camel_mime_filter_save_get_type ())

typedef struct _CamelMimeFilterSaveClass CamelMimeFilterSaveClass;

struct _CamelMimeFilterSave {
	CamelMimeFilter parent;

	struct _CamelMimeFilterSavePrivate *priv;

	char *filename;
	int fd;
};

struct _CamelMimeFilterSaveClass {
	CamelMimeFilterClass parent_class;
};

guint		camel_mime_filter_save_get_type	(void);
CamelMimeFilterSave      *camel_mime_filter_save_new	(void);

CamelMimeFilterSave *camel_mime_filter_save_new_name (const char *name, int flags, int mode);

#endif /* ! _CAMEL_MIME_FILTER_SAVE_H */
