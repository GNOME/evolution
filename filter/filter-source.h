/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * filter-source.h
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Developed by Jon Trowbridge <trow@ximian.com>
 */

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA.
 */

#ifndef __FILTER_SOURCE_H__
#define __FILTER_SOURCE_H__

#include "filter-element.h"

#define FILTER_SOURCE(obj)	GTK_CHECK_CAST (obj, filter_source_get_type (), FilterSource)
#define FILTER_SOURCE_CLASS(klass)	GTK_CHECK_CLASS_CAST (klass, filter_source_get_type (), FilterSourceClass)
#define IS_FILTER_SOURCE(obj)      GTK_CHECK_TYPE (obj, filter_source_get_type ())

typedef struct _FilterSource	FilterSource;
typedef struct _FilterSourceClass	FilterSourceClass;
struct _FilterSourcePrivate;

struct _FilterSource {
	FilterElement parent;
	struct _FilterSourcePrivate *priv;
};

struct _FilterSourceClass {
	FilterElementClass parent_class;
};

GtkType       filter_source_get_type (void);
FilterSource *filter_source_new      (void);

void filter_source_set_current (FilterSource *src, const gchar *url);

#endif /* __FILTER_SOURCE_H__ */

