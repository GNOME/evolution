/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright (C) 2000-2002 Ximian Inc.
 *
 *  Authors: Not Zed <notzed@lostzed.mmc.com.au>
 *           Jeffrey Stedfast <fejj@ximian.com>
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

#ifndef _EM_FILTER_FOLDER_ELEMENT_H
#define _EM_FILTER_FOLDER_ELEMENT_H

#include "filter/filter-element.h"

#define EM_FILTER_FOLDER_ELEMENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), em_filter_folder_element_get_type(), EMFilterFolderElement))
#define EM_FILTER_FOLDER_ELEMENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), em_filter_folder_element_get_type(), EMFilterFolderElementClass))
#define EM_IS_FILTER_FOLDER_ELEMENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), em_filter_folder_element_get_type()))
#define EM_IS_FILTER_FOLDER_ELEMENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), em_filter_folder_element_get_type()))
#define EM_FILTER_FOLDER_ELEMENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), em_filter_folder_element_get_type(), EMFilterFolderElementClass))

typedef struct _EMFilterFolderElement EMFilterFolderElement;
typedef struct _EMFilterFolderElementClass EMFilterFolderElementClass;

struct _EMFilterFolderElement {
	FilterElement parent_object;
	
	char *uri;
};

struct _EMFilterFolderElementClass {
	FilterElementClass parent_class;
};

GType em_filter_folder_element_get_type (void);
EMFilterFolderElement *em_filter_folder_element_new (void);

/* methods */
void em_filter_folder_element_set_value (EMFilterFolderElement *ff, const char *uri);

#endif /* ! _EM_FILTER_FOLDER_ELEMENT_H */
