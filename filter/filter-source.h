/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jon Trowbridge <trow@ximian.com>
 *           Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001-2002 Ximian, Inc. (www.ximian.com)
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


#ifndef __FILTER_SOURCE_H__
#define __FILTER_SOURCE_H__

#include "filter-element.h"

#define FILTER_TYPE_SOURCE            (filter_source_get_type ())
#define FILTER_SOURCE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FILTER_TYPE_SOURCE, FilterSource))
#define FILTER_SOURCE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FILTER_TYPE_SOURCE, FilterSourceClass))
#define IS_FILTER_SOURCE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FILTER_TYPE_SOURCE))
#define IS_FILTER_SOURCE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FILTER_TYPE_SOURCE))
#define FILTER_SOURCE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FILTER_TYPE_SOURCE, FilterSourceClass))

typedef struct _FilterSource FilterSource;
typedef struct _FilterSourceClass FilterSourceClass;

struct _FilterSource {
	FilterElement parent_object;
	struct _FilterSourcePrivate *priv;
};

struct _FilterSourceClass {
	FilterElementClass parent_class;
	
};


GType filter_source_get_type (void);
FilterSource *filter_source_new (void);

void filter_source_set_current (FilterSource *src, const char *url);

#endif /* __FILTER_SOURCE_H__ */
