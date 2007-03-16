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

#ifndef _FILTER_ELEMENT_H
#define _FILTER_ELEMENT_H

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtkwidget.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

#define FILTER_TYPE_ELEMENT            (filter_element_get_type ())
#define FILTER_ELEMENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FILTER_TYPE_ELEMENT, FilterElement))
#define FILTER_ELEMENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FILTER_TYPE_ELEMENT, FilterElementClass))
#define IS_FILTER_ELEMENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FILTER_TYPE_ELEMENT))
#define IS_FILTER_ELEMENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FILTER_TYPE_ELEMENT))
#define FILTER_ELEMENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FILTER_TYPE_ELEMENT, FilterElementClass))

typedef struct _FilterElement FilterElement;
typedef struct _FilterElementClass FilterElementClass;

typedef FilterElement *(*FilterElementFunc)(void *data);

struct _FilterElement {
	GObject parent_object;
	
	char *name;
	gpointer data;
};

struct _FilterPart;

struct _FilterElementClass {
	GObjectClass parent_class;
	
	/* virtual methods */
	gboolean (*validate) (FilterElement *fe);
	int (*eq) (FilterElement *fe, FilterElement *cm);
	
	void (*xml_create) (FilterElement *, xmlNodePtr);
	xmlNodePtr (*xml_encode) (FilterElement *);
	int (*xml_decode) (FilterElement *, xmlNodePtr);
	
	FilterElement *(*clone) (FilterElement *fe);
	void (*copy_value)(FilterElement *fe, FilterElement *se);

	GtkWidget *(*get_widget) (FilterElement *);
	void (*build_code) (FilterElement *, GString *, struct _FilterPart *ff);
	void (*format_sexp) (FilterElement *, GString *);
	
	/* signals */
};

GType		filter_element_get_type	(void);
FilterElement	*filter_element_new	(void);

void            filter_element_set_data (FilterElement *fe, gpointer data);

/* methods */
gboolean        filter_element_validate         (FilterElement *fe);
int		filter_element_eq		(FilterElement *fe, FilterElement *cm);

void		filter_element_xml_create	(FilterElement *fe, xmlNodePtr node);

xmlNodePtr	filter_element_xml_encode	(FilterElement *fe);
int		filter_element_xml_decode	(FilterElement *fe, xmlNodePtr node);
FilterElement	*filter_element_clone		(FilterElement *fe);
void		filter_element_copy_value	(FilterElement *de, FilterElement *se);

GtkWidget	*filter_element_get_widget	(FilterElement *fe);
void		filter_element_build_code	(FilterElement *fe, GString *out, struct _FilterPart *ff);
void		filter_element_format_sexp	(FilterElement *fe, GString *out);

#endif /* ! _FILTER_ELEMENT_H */
