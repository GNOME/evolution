/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@helixcode.com>
 *
 *  Copyright 2000 Helix Code, Inc. (www.helixcode.com)
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


#ifndef _FILTER_URL_H
#define _FILTER_URL_H

#include <gtk/gtk.h>
#include "filter-element.h"

#define FILTER_URL(obj)	        GTK_CHECK_CAST (obj, filter_url_get_type (), FilterUrl)
#define FILTER_URL_CLASS(klass)	GTK_CHECK_CLASS_CAST (klass, filter_url_get_type (), FilterUrlClass)
#define IS_FILTER_URL(obj)      GTK_CHECK_TYPE (obj, filter_url_get_type ())

typedef struct _FilterUrl	FilterUrl;
typedef struct _FilterUrlClass	FilterUrlClass;

struct _FilterUrl {
	FilterElement parent;
	struct _FilterUrlPrivate *priv;
	
	gchar *url;
};

struct _FilterUrlClass {
	FilterElementClass parent_class;

	/* virtual methods */

	/* signals */
};

guint		filter_url_get_type (void);
FilterUrl       *filter_url_new     (void);

/* methods */

#endif /* ! _FILTER_URL_H */

