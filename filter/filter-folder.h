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


#ifndef _FILTER_FOLDER_H
#define _FILTER_FOLDER_H

#include "filter-element.h"

#define FILTER_TYPE_FOLDER            (filter_folder_get_type ())
#define FILTER_FOLDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FILTER_TYPE_FOLDER, FilterFolder))
#define FILTER_FOLDER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FILTER_TYPE_FOLDER, FilterFolderClass))
#define IS_FILTER_FOLDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FILTER_TYPE_FOLDER))
#define IS_FILTER_FOLDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FILTER_TYPE_FOLDER))
#define FILTER_FOLDER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FILTER_TYPE_FOLDER, FilterFolderClass))

typedef struct _FilterFolder FilterFolder;
typedef struct _FilterFolderClass FilterFolderClass;

struct _FilterFolder {
	FilterElement parent_object;
	
	char *uri;
};

struct _FilterFolderClass {
	FilterElementClass parent_class;
	
	/* virtual methods */
	
	/* signals */
};

GType filter_folder_get_type (void);
FilterFolder *filter_folder_new (void);

/* methods */
void filter_folder_set_value (FilterFolder *ff, const char *uri);

#endif /* ! _FILTER_FOLDER_H */
