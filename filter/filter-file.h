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
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __FILTER_FILE_H__
#define __FILTER_FILE_H__

G_BEGIN_DECLS

#include "filter-element.h"

#define FILTER_TYPE_FILE            (filter_file_get_type ())
#define FILTER_FILE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FILTER_TYPE_FILE, FilterFile))
#define FILTER_FILE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FILTER_TYPE_FILE, FilterFileClass))
#define IS_FILTER_FILE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FILTER_TYPE_FILE))
#define IS_FILTER_FILE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FILTER_TYPE_FILE))
#define FILTER_FILE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FILTER_TYPE_FILE, FilterFileClass))

typedef struct _FilterFile FilterFile;
typedef struct _FilterFileClass FilterFileClass;

struct _FilterFile {
	FilterElement parent_object;

	gchar *type;
	gchar *path;
};

struct _FilterFileClass {
	FilterElementClass parent_class;

	/* virtual methods */

	/* signals */
};

GType filter_file_get_type (void);

FilterFile *filter_file_new (void);

FilterFile *filter_file_new_type_name (const gchar *type);

/* methods */
void filter_file_set_path (FilterFile *file, const gchar *path);

G_END_DECLS

#endif /* ! __FILTER_FILE_H__ */
