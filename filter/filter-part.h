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

#ifndef _FILTER_PART_H
#define _FILTER_PART_H

#include <gtk/gtk.h>
#include "filter-input.h"

#define FILTER_PART(obj)	GTK_CHECK_CAST (obj, filter_part_get_type (), FilterPart)
#define FILTER_PART_CLASS(klass)	GTK_CHECK_CLASS_CAST (klass, filter_part_get_type (), FilterPartClass)
#define IS_FILTER_PART(obj)      GTK_CHECK_TYPE (obj, filter_part_get_type ())

typedef struct _FilterPart	FilterPart;
typedef struct _FilterPartClass	FilterPartClass;

struct _FilterPart {
	GtkObject parent;
	struct _FilterPartPrivate *priv;

	char *name;
	char *title;
	char *code;
	GList *elements;
};

struct _FilterPartClass {
	GtkObjectClass parent_class;

	/* virtual methods */

	/* signals */
};

guint		filter_part_get_type	(void);
FilterPart	*filter_part_new	(void);

/* methods */
int		filter_part_xml_create	(FilterPart *ff, xmlNodePtr node);

xmlNodePtr	filter_part_xml_encode	(FilterPart *fe);
int		filter_part_xml_decode	(FilterPart *fe, xmlNodePtr node);

FilterPart	*filter_part_clone	(FilterPart *fp);

FilterElement	*filter_part_find_element(FilterPart *ff, const char *name);

GtkWidget	*filter_part_get_widget		(FilterPart *ff);
void		filter_part_build_code		(FilterPart *ff, GString *out);
void		filter_part_expand_code		(FilterPart *ff, const char *str, GString *out);

/* static functions */
void		filter_part_build_code_list	(GList *l, GString *out);
FilterPart	*filter_part_find_list		(GList *l, const char *name);
FilterPart	*filter_part_next_list		(GList *l, FilterPart *last);

#endif /* ! _FILTER_PART_H */

