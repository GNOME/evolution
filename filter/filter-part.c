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

#include <gtk/gtk.h>
#include <gnome.h>

#include <gnome-xml/xmlmemory.h>

#include "filter-part.h"
#include "filter-element.h"

#define d(x)

static void filter_part_class_init	(FilterPartClass *class);
static void filter_part_init	(FilterPart *gspaper);
static void filter_part_finalise	(GtkObject *obj);

#define _PRIVATE(x) (((FilterPart *)(x))->priv)

struct _FilterPartPrivate {
};

static GtkObjectClass *parent_class;

enum {
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

guint
filter_part_get_type (void)
{
	static guint type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"FilterPart",
			sizeof(FilterPart),
			sizeof(FilterPartClass),
			(GtkClassInitFunc)filter_part_class_init,
			(GtkObjectInitFunc)filter_part_init,
			(GtkArgSetFunc)NULL,
			(GtkArgGetFunc)NULL
		};
		
		type = gtk_type_unique(gtk_object_get_type (), &type_info);
	}
	
	return type;
}

static void
filter_part_class_init (FilterPartClass *class)
{
	GtkObjectClass *object_class;
	
	object_class = (GtkObjectClass *)class;
	parent_class = gtk_type_class(gtk_object_get_type ());

	object_class->finalize = filter_part_finalise;
	/* override methods */

	/* signals */

	gtk_object_class_add_signals(object_class, signals, LAST_SIGNAL);
}

static void
filter_part_init (FilterPart *o)
{
	o->priv = g_malloc0(sizeof(*o->priv));
}

static void
filter_part_finalise(GtkObject *obj)
{
	FilterPart *o = (FilterPart *)obj;

	o = o;

        ((GtkObjectClass *)(parent_class))->finalize(obj);
}

/**
 * filter_part_new:
 *
 * Create a new FilterPart object.
 * 
 * Return value: A new #FilterPart object.
 **/
FilterPart *
filter_part_new(void)
{
	FilterPart *o = (FilterPart *)gtk_type_new(filter_part_get_type ());
	return o;
}



int		filter_part_xml_create	(FilterPart *ff, xmlNodePtr node)
{
	xmlNodePtr n;
	char *type;
	FilterElement *el;

	ff->name = xmlGetProp(node, "name");
	n = node->childs;
	while (n) {
		if (!strcmp(n->name, "input")) {
			type = xmlGetProp(n, "type");
			d(printf("creating new element type input '%s'\n", type));
			if (type != NULL
			    && (el = filter_element_new_type_name(type)) != NULL) {
				filter_element_xml_create(el, n);
				xmlFree(type);
				d(printf("adding element part %p %s\n", el, el->name));
				ff->elements = g_list_append(ff->elements, el);
			} else {
				g_warning("Invalid xml format, missing/unknown input type");
			}
		} else if (!strcmp(n->name, "title")) {
			if (!ff->title)
				ff->title = xmlNodeGetContent(n);
		} else if (!strcmp(n->name, "code")) {
			if (!ff->code)
				ff->code = xmlNodeGetContent(n);
		} else {
			g_warning("Unknwon part element in xml: %s\n", n->name);
		}
		n = n->next;
	}
	return 0;
}

xmlNodePtr	filter_part_xml_encode	(FilterPart *fp)
{
	GList *l;
	FilterElement *fe;
	xmlNodePtr part, value;

	g_return_val_if_fail(fp != NULL, NULL);

	part = xmlNewNode(NULL, "part");
	xmlSetProp(part, "name", fp->name);
	l = fp->elements;
	while (l) {
		fe = l->data;
		value = filter_element_xml_encode(fe);
		xmlAddChild(part, value);
		l = g_list_next(l);
	}
	return part;
}

int		filter_part_xml_decode	(FilterPart *fp, xmlNodePtr node)
{
	FilterElement *fe;
	xmlNodePtr n;
	char *name;

	g_return_val_if_fail(fp != NULL, -1);
	g_return_val_if_fail(node != NULL, -1);

	n = node->childs;
	while (n) {
		if (!strcmp(n->name, "value")) {
			name = xmlGetProp(n, "name");
			d(printf("finding element part %p %s = %p\n", name, name, fe));
			fe = filter_part_find_element(fp, name);
			d(printf("finding element part %p %s = %p\n", name, name, fe));
			xmlFree(name);
			if (fe) {
				filter_element_xml_decode(fe, n);
			}
		}
		n = n->next;
	}
	return 0;
}

FilterPart	*filter_part_clone	(FilterPart *fp)
{
	FilterPart *new;
	GList *l;
	FilterElement *fe, *ne;

	new = (FilterPart *)gtk_type_new( ((GtkObject *)fp)->klass->type );
	new->name = g_strdup(fp->name);
	new->title = g_strdup(fp->title);
	new->code = g_strdup(fp->code);
	l = fp->elements;
	while (l) {
		fe = l->data;
		ne = filter_element_clone(fe);
		new->elements = g_list_append(new->elements, ne);
		l = g_list_next(l);
	}
	return new;
}

FilterElement	*filter_part_find_element(FilterPart *ff, const char *name)
{
	GList *l = ff->elements;
	FilterElement *fe;

	if (name == NULL)
		return NULL;

	while (l) {
		fe = l->data;
		if (fe->name && !strcmp(fe->name, name))
			return fe;
		l = g_list_next(l);
	}

	return NULL;
}


GtkWidget	*filter_part_get_widget	(FilterPart *ff)
{
	GtkHBox *hbox;
	GList *l = ff->elements;
	FilterElement *fe;
	GtkWidget *w;

	hbox = (GtkHBox *)gtk_hbox_new(FALSE, 3);

	while (l) {
		fe = l->data;
		w = filter_element_get_widget(fe);
		if (w) {
			gtk_box_pack_start((GtkBox *)hbox, w, FALSE, FALSE, 3);
		}
		l = g_list_next(l);
	}
	gtk_widget_show_all((GtkWidget *)hbox);
	return (GtkWidget *)hbox;
}

/**
 * filter_part_build_code:
 * @ff: 
 * @out: 
 * 
 * Outputs the code of a part.
 **/
void		filter_part_build_code		(FilterPart *ff, GString *out)
{
	GList *l = ff->elements;
	FilterElement *fe;

	if (ff->code) {
		filter_part_expand_code(ff, ff->code, out);
	}
	while (l) {
		fe = l->data;
		filter_element_build_code(fe, out, ff);
		l = g_list_next(l);
	}	
}

/**
 * filter_part_build_code_list:
 * @l: 
 * @out: 
 * 
 * Construct a list of the filter parts code into
 * a single string.
 **/
void
filter_part_build_code_list(GList *l, GString *out)
{
	FilterPart *fp;

	while (l) {
		fp = l->data;
		filter_part_build_code(fp, out);
		g_string_append(out, "\n  ");
		l = g_list_next(l);
	}
}

/**
 * filter_part_find_list:
 * @l: 
 * @name: 
 * 
 * Find a filter part stored in a list.
 * 
 * Return value: 
 **/
FilterPart	*filter_part_find_list		(GList *l, const char *name)
{
	FilterPart *part;
	d(printf("Find part named %s\n", name));

	while (l) {
		part = l->data;
		if (!strcmp(part->name, name)) {
			d(printf("Found!\n"));
			return part;
		}
		l = g_list_next(l);
	}
	return NULL;
}

/**
 * filter_part_next_list:
 * @l: 
 * @last: The last item retrieved, or NULL to start
 * from the beginning of the list.
 * 
 * Iterate through a filter part list.
 * 
 * Return value: The next value in the list, or NULL if the
 * list is expired.
 **/
FilterPart	*filter_part_next_list		(GList *l, FilterPart *last)
{
	GList *node = l;

	if (last != NULL) {
		node = g_list_find(node, last);
		if (node == NULL)
			node = l;
		else
			node = g_list_next(node);
	}
	if (node)
		return node->data;
	return NULL;
}

/**
 * filter_part_expand_code:
 * @ff: 
 * @str: 
 * @out: 
 * 
 * Expands the variables in string @str based on the values of the part.
 **/
void		filter_part_expand_code		(FilterPart *ff, const char *source, GString *out)
{
	const char *newstart, *start, *end;
	char *name=alloca(32);
	int len, namelen=32;
	FilterElement *fe;

	start = source;
	while ( (newstart = strstr(start, "${"))
		&& (end = strstr(newstart+2, "}")) ) {
		len = end-newstart-2;
		if (len+1>namelen) {
			namelen = (len+1)*2;
			name = alloca(namelen);
		}
		memcpy(name, newstart+2, len);
		name[len] = 0;
		fe = filter_part_find_element(ff, name);
		d(printf("expand code: looking up variab le '%s' = %p\n", name, fe));
		if (fe) {
			g_string_sprintfa(out, "%.*s", newstart-start, start);
			filter_element_format_sexp(fe, out);
#if 0
		} else if ( (val = g_hash_table_lookup(ff->globals, name)) ) {
			g_string_sprintfa(out, "%.*s", newstart-start, start);
			e_sexp_encode_string(out, val);
#endif
		} else {
			g_string_sprintfa(out, "%.*s", end-start+1, start);
		}
		start = end+1;
	}
	g_string_append(out, start);
}

#if 0
int main(int argc, char **argv)
{
	xmlDocPtr system;
	FilterPart *ff;
	GtkWidget *w;
	GnomeDialog *gd;
	xmlNodePtr node;
	GString *code;

	gnome_init("test", "0.0", argc, argv);

	system = xmlParseFile("form.xml");
	if (system==NULL) {
		printf("i/o error\n");
		return 1;
	}

	ff = filter_part_new();
	filter_part_xml_create(ff, system->root);

	w = filter_part_get_widget(ff);

	gd = (GnomeDialog *)gnome_dialog_new(_("Test"), GNOME_STOCK_BUTTON_OK, NULL);
	gtk_box_pack_start((GtkBox *)gd->vbox, w, FALSE, TRUE, 0);
	gtk_widget_show((GtkWidget *)gd);

	gnome_dialog_run_and_close(gd);

	code = g_string_new("");
	filter_part_build_code(ff, code);
	printf("code is:\n%s\n", code->str);

	return 0;
}
#endif
