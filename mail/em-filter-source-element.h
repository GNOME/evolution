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

#ifndef _EM_FILTER_SOURCE_ELEMENT_H
#define _EM_FILTER_SOURCE_ELEMENT_H

#include "filter/filter-element.h"

#define EM_FILTER_SOURCE_ELEMENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), em_filter_source_element_get_type(), EMFilterSourceElement))
#define EM_FILTER_SOURCE_ELEMENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), em_filter_source_element_get_type(), EMFilterSourceElementClass))
#define EM_IS_FILTER_SOURCE_ELEMENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), em_filter_source_element_get_type()))
#define EM_IS_FILTER_SOURCE_ELEMENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), em_filter_source_element_get_type()))
#define EM_FILTER_SOURCE_ELEMENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), em_filter_source_element_get_type(), EMFilterSourceElementClass))

typedef struct _EMFilterSourceElement EMFilterSourceElement;
typedef struct _EMFilterSourceElementClass EMFilterSourceElementClass;

struct _EMFilterSourceElement {
	FilterElement parent_object;
	struct _EMFilterSourceElementPrivate *priv;
};

struct _EMFilterSourceElementClass {
	FilterElementClass parent_class;
};

GType em_filter_source_element_get_type (void);
EMFilterSourceElement *em_filter_source_element_new (void);

void em_filter_source_element_set_current (EMFilterSourceElement *src, const char *url);

#endif /* _EM_FILTER_SOURCE_ELEMENT_H */
