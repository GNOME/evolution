/*
 *  Copyright (C) 2000 Ximian Inc.
 *
 *  Authors: Not Zed <notzed@lostzed.mmc.com.au>
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

#ifndef _FILTER_FOLDER_H
#define _FILTER_FOLDER_H

#include "filter-element.h"

#define FILTER_FOLDER(obj)	GTK_CHECK_CAST (obj, filter_folder_get_type (), FilterFolder)
#define FILTER_FOLDER_CLASS(klass)	GTK_CHECK_CLASS_CAST (klass, filter_folder_get_type (), FilterFolderClass)
#define IS_FILTER_FOLDER(obj)      GTK_CHECK_TYPE (obj, filter_folder_get_type ())

typedef struct _FilterFolder	FilterFolder;
typedef struct _FilterFolderClass	FilterFolderClass;

struct _FilterFolder {
	FilterElement parent;
	struct _FilterFolderPrivate *priv;

	char *uri;
};

struct _FilterFolderClass {
	FilterElementClass parent_class;

	/* virtual methods */

	/* signals */
};

guint		filter_folder_get_type	(void);
FilterFolder	*filter_folder_new	(void);

/* methods */
void            filter_folder_set_value(FilterFolder *ff, const char *uri);

#endif /* ! _FILTER_FOLDER_H */

