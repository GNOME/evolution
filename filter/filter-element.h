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

#ifndef _FILTER_ELEMENT_H
#define _FILTER_ELEMENT_H

#include <gtk/gtk.h>
#include <gnome-xml/parser.h>

#define FILTER_ELEMENT(obj)	GTK_CHECK_CAST (obj, filter_element_get_type (), FilterElement)
#define FILTER_ELEMENT_CLASS(klass)	GTK_CHECK_CLASS_CAST (klass, filter_element_get_type (), FilterElementClass)
#define IS_FILTER_ELEMENT(obj)      GTK_CHECK_TYPE (obj, filter_element_get_type ())

typedef struct _FilterElement	FilterElement;
typedef struct _FilterElementClass	FilterElementClass;

struct _FilterElement {
	GtkObject parent;
	struct _FilterElementPrivate *priv;

	char *name;
};

struct _FilterPart;

struct _FilterElementClass {
	GtkObjectClass parent_class;

	/* virtual methods */
	void (*xml_create)(FilterElement *, xmlNodePtr);
	xmlNodePtr (*xml_encode)(FilterElement *);
	int (*xml_decode)(FilterElement *, xmlNodePtr);

	FilterElement *(*clone)(FilterElement *fe);

	GtkWidget *(*get_widget)(FilterElement *);
	void (*build_code)(FilterElement *, GString *, struct _FilterPart *ff);
	void (*format_sexp)(FilterElement *, GString *);

	/* signals */
};

guint		filter_element_get_type	(void);
FilterElement	*filter_element_new	(void);

FilterElement	*filter_element_new_type_name	(const char *type);

/* methods */
void		filter_element_xml_create	(FilterElement *fe, xmlNodePtr node);

xmlNodePtr	filter_element_xml_encode	(FilterElement *fe);
int		filter_element_xml_decode	(FilterElement *fe, xmlNodePtr node);
FilterElement	*filter_element_clone		(FilterElement *fe);

GtkWidget	*filter_element_get_widget	(FilterElement *fe);
void		filter_element_build_code	(FilterElement *fe, GString *out, struct _FilterPart *ff);
void		filter_element_format_sexp	(FilterElement *fe, GString *out);

#endif /* ! _FILTER_ELEMENT_H */

