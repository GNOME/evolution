/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#ifndef _FILTER_FILE_H
#define _FILTER_FILE_H

#include "filter-element.h"

#define FILTER_FILE(obj)         GTK_CHECK_CAST (obj, filter_file_get_type (), FilterFile)
#define FILTER_FILE_CLASS(klass) GTK_CHECK_CLASS_CAST (klass, filter_file_get_type (), FilterFileClass)
#define IS_FILTER_FILE(obj)      GTK_CHECK_TYPE (obj, filter_file_get_type ())

typedef struct _FilterFile	FilterFile;
typedef struct _FilterFileClass	FilterFileClass;

struct _FilterFile {
	FilterElement parent;
	struct _FilterFilePrivate *priv;
	
	char *type;
	char *path;
};

struct _FilterFileClass {
	FilterElementClass parent_class;
	
	/* virtual methods */
	
	/* signals */
};

GtkType filter_file_get_type (void);

FilterFile *filter_file_new (void);

FilterFile *filter_file_new_type_name (const char *type);

/* methods */
void filter_file_set_path (FilterFile *file, const char *path);

#endif /* ! _FILTER_FILE_H */
