/*
 *  Copyright (C) 2000 Helix Code Inc.
 *
 *  Authors: Not Zed <notzed@lostzed.mmc.com.au>
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

#ifndef _FILTER_FOLDER_H
#define _FILTER_FOLDER_H

#include <gtk/gtk.h>
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
	char *name;		/* name of folder for display? */
};

struct _FilterFolderClass {
	FilterElementClass parent_class;

	/* virtual methods */

	/* signals */
};

guint		filter_folder_get_type	(void);
FilterFolder	*filter_folder_new	(void);

/* methods */

#endif /* ! _FILTER_FOLDER_H */

