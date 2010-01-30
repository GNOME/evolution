/*
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Not Zed <notzed@lostzed.mmc.com.au>
 *      Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _EM_FILTER_FOLDER_ELEMENT_H
#define _EM_FILTER_FOLDER_ELEMENT_H

#include "filter/e-filter-element.h"

#define EM_FILTER_FOLDER_ELEMENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), em_filter_folder_element_get_type(), EMFilterFolderElement))
#define EM_FILTER_FOLDER_ELEMENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), em_filter_folder_element_get_type(), EMFilterFolderElementClass))
#define EM_IS_FILTER_FOLDER_ELEMENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), em_filter_folder_element_get_type()))
#define EM_IS_FILTER_FOLDER_ELEMENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), em_filter_folder_element_get_type()))
#define EM_FILTER_FOLDER_ELEMENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), em_filter_folder_element_get_type(), EMFilterFolderElementClass))

typedef struct _EMFilterFolderElement EMFilterFolderElement;
typedef struct _EMFilterFolderElementClass EMFilterFolderElementClass;

struct _EMFilterFolderElement {
	EFilterElement parent_object;

	gchar *uri;
	gboolean store_camel_uri; /* true if uri should contain camel uri, otherwise contains evolution's uri with an Account ID */
};

struct _EMFilterFolderElementClass {
	EFilterElementClass parent_class;
};

GType em_filter_folder_element_get_type (void);
EMFilterFolderElement *em_filter_folder_element_new (void);

/* methods */
void em_filter_folder_element_set_value (EMFilterFolderElement *ff, const gchar *uri);

#endif /* _EM_FILTER_FOLDER_ELEMENT_H */
