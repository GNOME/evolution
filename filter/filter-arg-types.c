/*
 *  Copyright (C) 2000 Helix Code Inc.
 *
 *  Authors: Michael Zucchi <notzed@helixcode.com>
 *
 *  Implementations of the filter-arg types.
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

#include "filter-arg-types.h"

/* ********************************************************************** */
/*                               Address                                  */
/* ********************************************************************** */

static void filter_arg_address_class_init (FilterArgAddressClass *class);
static void filter_arg_address_init       (FilterArgAddress      *gspaper);

static FilterArg *parent_class;

guint
filter_arg_address_get_type (void)
{
	static guint type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"FilterArgAddress",
			sizeof (FilterArgAddress),
			sizeof (FilterArgAddressClass),
			(GtkClassInitFunc) filter_arg_address_class_init,
			(GtkObjectInitFunc) filter_arg_address_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};
		
		type = gtk_type_unique (filter_arg_get_type (), &type_info);
	}
	
	return type;
}

static void
arg_address_write_html(FilterArg *argin, GtkHTML *html, GtkHTMLStreamHandle *stream)
{
	FilterArgAddress *arg = (FilterArgAddress *)argin;
	/* empty */
}

static void
arg_address_write_text(FilterArg *argin, GString *string)
{
	FilterArgAddress *arg = (FilterArgAddress *)argin;
	GList *l;
	struct filter_arg_address *a;

	l = argin->values;
	if (l == NULL) {
		g_string_append(string, "email address");
	}
	while (l) {
		a = l->data;
		g_string_append(string, a->name);
		if (l->next) {
			g_string_append(string, ", ");
		}
		l = g_list_next(l);
	}
}

static void
arg_address_edit_values(FilterArg *arg)
{
	printf("edit it!\n");
}

static xmlNodePtr
arg_address_values_get_xml(FilterArg *argin)
{
	xmlNodePtr value;
	FilterArgAddress *arg = (FilterArgAddress *)argin;
	GList *l;
	struct filter_arg_address *a;

	/* hmm, perhaps this overhead should be in FilterArg, and this function just returns the base node?? */
	value = xmlNewNode(NULL, "optionvalue");
	xmlSetProp(value, "name", argin->name);

	l = argin->values;
	while (l) {
		xmlNodePtr cur;

		a = l->data;

		cur = xmlNewChild(value, NULL, "address", NULL);
		if (a->name)
			xmlSetProp(cur, "name", a->name);
		if (a->email)
			xmlSetProp(cur, "email", a->email);
		l = g_list_next(l);
	}

	return value;
}

static void
arg_address_values_add_xml(FilterArg *arg, xmlNodePtr node)
{
	xmlNodePtr n;

	n = node->childs;
	while (n) {
		if (!strcmp(n->name, "address")) {
			filter_arg_address_add(arg, xmlGetProp(n, "name"), xmlGetProp(n, "email"));
		} else {
			g_warning("Loading address from xml, wrong node encountered: %s\n", n->name);
		}
		n = n->next;
	}
}

/* the search string is just the raw email address */
static char *
arg_address_get_value_as_string(FilterArg *argin, void *data)
{
	FilterArgAddress *arg = (FilterArgAddress *)argin;
	struct filter_arg_address *a = (struct filter_arg_address *)data;

	if (a->email == NULL
	    || a->email[0] == '\0') {
		if (a->name == NULL
		    || a->name[0] == '\0')
			return "";
		return a->name;
	} else
		return a->email;
}

static void
arg_address_free_value(FilterArg *arg, struct filter_arg_address *a)
{
	g_free(a->name);
	g_free(a->email);
	g_free(a);
}

static void
filter_arg_address_class_init (FilterArgAddressClass *class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) class;
	if (parent_class == NULL)
		parent_class = gtk_type_class (gtk_object_get_type ());

	class->parent_class.write_html = arg_address_write_html;
	class->parent_class.write_text = arg_address_write_text;
	class->parent_class.edit_values = arg_address_edit_values;
	class->parent_class.free_value = arg_address_free_value;

	class->parent_class.values_get_xml = arg_address_values_get_xml;
	class->parent_class.values_add_xml = arg_address_values_add_xml;

	class->parent_class.get_value_as_string = arg_address_get_value_as_string;
}

static void
filter_arg_address_init (FilterArgAddress *arg)
{
	arg->arg.values = NULL;
}

/**
 * filter_arg_address_new:
 *
 * Create a new FilterArgAddress widget.
 * 
 * Return value: A new FilterArgAddress widget.
 **/
FilterArg *
filter_arg_address_new (char *name)
{
	FilterArg *a = FILTER_ARG ( gtk_type_new (filter_arg_address_get_type ()));
	a->name = g_strdup(name);
	return a;
}

void
filter_arg_address_add(FilterArg *arg, char *name, char *email)
{
	struct filter_arg_address *a;

	a = g_malloc0(sizeof(*a));

	a->name = g_strdup(name);
	a->email = g_strdup(email);

	filter_arg_add(arg, a);
}

void
filter_arg_address_remove(FilterArg *arg, char *name, char *email)
{

}

/* ********************************************************************** */
/*                               Folder                                   */
/* ********************************************************************** */


static void filter_arg_folder_class_init (FilterArgFolderClass *class);
static void filter_arg_folder_init       (FilterArgFolder      *gspaper);

static FilterArg *parent_class;

guint
filter_arg_folder_get_type (void)
{
	static guint type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"FilterArgFolder",
			sizeof (FilterArgFolder),
			sizeof (FilterArgFolderClass),
			(GtkClassInitFunc) filter_arg_folder_class_init,
			(GtkObjectInitFunc) filter_arg_folder_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};
		
		type = gtk_type_unique (filter_arg_get_type (), &type_info);
	}
	
	return type;
}

static void
arg_folder_write_html(FilterArg *argin, GtkHTML *html, GtkHTMLStreamHandle *stream)
{
	FilterArgFolder *arg = (FilterArgFolder *)argin;
	/* empty */
}

static void
arg_folder_write_text(FilterArg *argin, GString *string)
{
	FilterArgFolder *arg = (FilterArgFolder *)argin;
	GList *l;
	char *a;

	l = argin->values;
	if (l == NULL) {
		g_string_append(string, "folder");
	}
	while (l) {
		a = l->data;
		g_string_append(string, a);
		if (l->next) {
			g_string_append(string, ", ");
		}
		l = g_list_next(l);
	}
}

static void
arg_folder_edit_values(FilterArg *arg)
{
	printf("edit it!\n");
}

static xmlNodePtr
arg_folder_values_get_xml(FilterArg *argin)
{
	xmlNodePtr value;
	FilterArgFolder *arg = (FilterArgFolder *)argin;
	GList *l;
	char *a;

	value = xmlNewNode(NULL, "optionvalue");
	xmlSetProp(value, "name", argin->name);

	l = argin->values;
	while (l) {
		xmlNodePtr cur;

		a = l->data;

		cur = xmlNewChild(value, NULL, "folder", NULL);
		if (a)
			xmlSetProp(cur, "folder", a);
		l = g_list_next(l);
	}

	return value;
}

static void
arg_folder_values_add_xml(FilterArg *arg, xmlNodePtr node)
{
	xmlNodePtr n;

	n = node->childs;
	while (n) {
		if (!strcmp(n->name, "folder")) {
			filter_arg_folder_add(arg, xmlGetProp(n, "folder"));
		} else {
			g_warning("Loading folders from xml, wrong node encountered: %s\n", n->name);
		}
		n = n->next;
	}
}

static char *
arg_folder_get_value_as_string(FilterArg *argin, void *data)
{
	FilterArgFolder *arg = (FilterArgFolder *)argin;
	char *a = (char *)data;

	return a;
}

static void
arg_folder_free_value(FilterArg *arg, void *a)
{
	g_free(a);
}

static void
filter_arg_folder_class_init (FilterArgFolderClass *class)
{
	GtkObjectClass *object_class;
	
	object_class = (GtkObjectClass *) class;
	if (parent_class == NULL)
		parent_class = gtk_type_class (gtk_object_get_type ());

	class->parent_class.write_html = arg_folder_write_html;
	class->parent_class.write_text = arg_folder_write_text;
	class->parent_class.edit_values = arg_folder_edit_values;
	class->parent_class.free_value = arg_folder_free_value;

	class->parent_class.values_get_xml = arg_folder_values_get_xml;
	class->parent_class.values_add_xml = arg_folder_values_add_xml;
}

static void
filter_arg_folder_init (FilterArgFolder *arg)
{
	arg->arg.values = NULL;
}

/**
 * filter_arg_folder_new:
 *
 * Create a new FilterArgFolder widget.
 * 
 * Return value: A new FilterArgFolder widget.
 **/
FilterArg *
filter_arg_folder_new (char *name)
{
	FilterArg *a = FILTER_ARG ( gtk_type_new (filter_arg_folder_get_type ()));
	a->name = g_strdup(name);
	return a;
}


void
filter_arg_folder_add(FilterArg *arg, char *name)
{
	filter_arg_add(arg, g_strdup(name));
}

void
filter_arg_folder_remove(FilterArg *arg, char *name)
{
	/* do it */
}
