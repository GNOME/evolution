/*
 *  Copyright (C) 2000 Helix Code Inc.
 *
 *  Authors: Michael Zucchi <notzed@helixcode.com>
 *
 *  Abstract class to hold filter arguments.
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

#ifndef _FILTER_ARG_H
#define _FILTER_ARG_H

#include <gtk/gtk.h>
#include <gtkhtml/gtkhtml.h>
#include <gnome-xml/tree.h>

#define FILTER_ARG(obj)         GTK_CHECK_CAST (obj, filter_arg_get_type (), FilterArg)
#define FILTER_ARG_CLASS(klass) GTK_CHECK_CLASS_CAST (klass, filter_arg_get_type (), FilterArgClass)
#define IS_FILTER_ARG(obj)      GTK_CHECK_TYPE (obj, filter_arg_get_type ())

typedef struct _FilterArg      FilterArg;
typedef struct _FilterArgClass FilterArgClass;

struct _FilterArg {
	GtkObject object;

	struct _FilterArgPrivate *priv;

	char *name;
	GList *values;
};

struct _FilterArgClass {
	GtkObjectClass parent_class;

	/* make a copy of yourself */
	struct _FilterArg * (*clone)(FilterArg *arg);

	/* virtual methods */
	void (*write_html)(FilterArg *arg, GtkHTML *html, GtkHTMLStreamHandle *stream);
	void (*write_text)(FilterArg *arg, GString *string);
	void (*free_value)(FilterArg *arg, void *v);

	void (*edit_values)(FilterArg *arg);
	int (*edit_value)(FilterArg *arg, int index);

	void (*values_add_xml)(FilterArg *arg, xmlNodePtr node);
	xmlNodePtr (*values_get_xml)(FilterArg *arg);

	char * (*get_value_as_string)(FilterArg *arg, void *data);

	/* signals */
	void (*changed)(FilterArg *arg);
};

guint		filter_arg_get_type	(void);
FilterArg      *filter_arg_new	(char *name);
FilterArg      *filter_arg_clone(FilterArg *arg);
void		filter_arg_copy (FilterArg *dst, FilterArg *src);
void		filter_arg_value_add(FilterArg *a, void *v);

void		filter_arg_edit_values(FilterArg *arg);
int	        filter_arg_edit_value(FilterArg *arg, int index);

void		filter_arg_remove(FilterArg *arg, void *v);
void		filter_arg_add(FilterArg *arg, void *v);

xmlNodePtr	filter_arg_values_get_xml(FilterArg *arg);
void		filter_arg_values_add_xml(FilterArg *arg, xmlNodePtr node);
int		filter_arg_get_count(FilterArg *arg);
void 	       *filter_arg_get_value(FilterArg *arg, int index);
char 	       *filter_arg_get_value_as_string(FilterArg *arg, int index);

#endif /* ! _FILTER_ARG_H */

