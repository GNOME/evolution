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

#ifndef _CAMEL_MIME_FILTER_SAVE_H
#define _CAMEL_MIME_FILTER_SAVE_H

#include <camel/camel-mime-filter.h>

#define CAMEL_MIME_FILTER_SAVE(obj)         GTK_CHECK_CAST (obj, camel_mime_filter_save_get_type (), CamelMimeFilterSave)
#define CAMEL_MIME_FILTER_SAVE_CLASS(klass) GTK_CHECK_CLASS_CAST (klass, camel_mime_filter_save_get_type (), CamelMimeFilterSaveClass)
#define IS_CAMEL_MIME_FILTER_SAVE(obj)      GTK_CHECK_TYPE (obj, camel_mime_filter_save_get_type ())

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
