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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <sys/types.h>
#include <regex.h>

#include <gtk/gtk.h>
#include <libgnome/gnome-i18n.h>

#include "filter-input.h"
#include "e-util/e-sexp.h"
#include "widgets/misc/e-error.h"

#define d(x)

static gboolean validate (FilterElement *fe);
static int input_eq (FilterElement *fe, FilterElement *cm);
static void xml_create (FilterElement *fe, xmlNodePtr node);
static xmlNodePtr xml_encode (FilterElement *fe);
static int xml_decode (FilterElement *fe, xmlNodePtr node);
static GtkWidget *get_widget (FilterElement *fe);
static void build_code (FilterElement *fe, GString *out, struct _FilterPart *ff);
static void format_sexp (FilterElement *, GString *);

static void filter_input_class_init (FilterInputClass *klass);
static void filter_input_init (FilterInput *fi);
static void filter_input_finalise (GObject *obj);


static FilterElementClass *parent_class = NULL;


GType
filter_input_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (FilterInputClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) filter_input_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (FilterInput),
			0,    /* n_preallocs */
			(GInstanceInitFunc) filter_input_init,
		};
		
		type = g_type_register_static (FILTER_TYPE_ELEMENT, "FilterInput", &info, 0);
	}
	
	return type;
}

static void
filter_input_class_init (FilterInputClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	FilterElementClass *fe_class = FILTER_ELEMENT_CLASS (klass);
	
	parent_class = g_type_class_ref (FILTER_TYPE_ELEMENT);
	
	object_class->finalize = filter_input_finalise;
	
	/* override methods */
	fe_class->validate = validate;
	fe_class->eq = input_eq;
	fe_class->xml_create = xml_create;
	fe_class->xml_encode = xml_encode;
	fe_class->xml_decode = xml_decode;
	fe_class->get_widget = get_widget;
	fe_class->build_code = build_code;
	fe_class->format_sexp = format_sexp;
}

static void
filter_input_init (FilterInput *fi)
{
	fi->values = g_list_prepend (NULL, g_strdup (""));
}

static void
filter_input_finalise (GObject *obj)
{
	FilterInput *fi = (FilterInput *) obj;
	
	xmlFree (fi->type);
	g_list_foreach (fi->values, (GFunc)g_free, NULL);
	g_list_free (fi->values);
	
        G_OBJECT_CLASS (parent_class)->finalize (obj);
}

/**
 * filter_input_new:
 *
 * Create a new FilterInput object.
 * 
 * Return value: A new #FilterInput object.
 **/
FilterInput *
filter_input_new (void)
{
	return (FilterInput *) g_object_new (FILTER_TYPE_INPUT, NULL, NULL);
}

FilterInput *
filter_input_new_type_name (const char *type)
{
	FilterInput *fi;
	
	fi = filter_input_new ();
	fi->type = xmlStrdup (type);
	
	d(printf("new type %s = %p\n", type, fi));
	
	return fi;
}

void
filter_input_set_value (FilterInput *fi, const char *value)
{
	GList *l;
	
	d(printf("set_value '%s'\n", value));

	l = fi->values;
	while (l) {
		g_free (l->data);
		l = g_list_next (l);
	}
	g_list_free (fi->values);
	
	fi->values = g_list_append (NULL, g_strdup (value));
}

static gboolean
validate (FilterElement *fe)
{
	FilterInput *fi = (FilterInput *)fe;
	gboolean valid = TRUE;
	
	if (fi->values && !strcmp (fi->type, "regex")) {
		const char *pattern;
		regex_t regexpat;
		int regerr;
		
		pattern = fi->values->data;
		
		if ((regerr = regcomp (&regexpat, pattern, REG_EXTENDED | REG_NEWLINE | REG_ICASE))) {
			size_t reglen;
			char *regmsg;
			
			/* regerror gets called twice to get the full error string 
			   length to do proper posix error reporting */
			reglen = regerror (regerr, &regexpat, 0, 0);
			regmsg = g_malloc0 (reglen + 1);
			regerror (regerr, &regexpat, regmsg, reglen);
			
			/* FIXME: FilterElement should probably have a
			   GtkWidget member pointing to the value gotten with
			   ::get_widget() so that we can get the parent window
			   here. */
			e_error_run(NULL, "filter:bad-regexp", pattern, regmsg, NULL);
			g_free (regmsg);
			
			valid = FALSE;
		}
		
		regfree (&regexpat);
	}
	
	return valid;
}

static int
list_eq (GList *al, GList *bl)
{
	int truth = TRUE;
	
	while (truth && al && bl) {
		truth = strcmp ((char *) al->data, (char *) bl->data) == 0;
		al = al->next;
		bl = bl->next;
	}
	
	return truth && al == NULL && bl == NULL;
}

static int
input_eq (FilterElement *fe, FilterElement *cm)
{
	FilterInput *fi = (FilterInput *)fe, *ci = (FilterInput *)cm;
	
	return FILTER_ELEMENT_CLASS (parent_class)->eq (fe, cm)
		&& strcmp (fi->type, ci->type) == 0
		&& list_eq (fi->values, ci->values);
}

static void
xml_create (FilterElement *fe, xmlNodePtr node)
{
	/* parent implementation */
        FILTER_ELEMENT_CLASS (parent_class)->xml_create (fe, node);
}

static xmlNodePtr
xml_encode (FilterElement *fe)
{
	xmlNodePtr value;
	GList *l;
	FilterInput *fi = (FilterInput *)fe;
	char *type;
	
	type = fi->type ? fi->type : "string";
	
	d(printf ("Encoding %s as xml\n", type));
	
	value = xmlNewNode (NULL, "value");
	xmlSetProp (value, "name", fe->name);
	xmlSetProp (value, "type", type);
	l = fi->values;
	while (l) {
		xmlNodePtr cur;
		char *str = l->data;
		
                cur = xmlNewChild (value, NULL, type, NULL);
		xmlNodeSetContent (cur, str);
		l = l->next;
	}
	
	return value;
}

static int
xml_decode (FilterElement *fe, xmlNodePtr node)
{
	FilterInput *fi = (FilterInput *)fe;
	char *name, *str, *type;
	xmlNodePtr n;
	GList *l;
	
	l = fi->values;
	while (l) {
		g_free (l->data);
		l = l->next;
	}
	g_list_free (fi->values);
	fi->values = NULL;

	name = xmlGetProp (node, "name");
	type = xmlGetProp (node, "type");
	
	d(printf("Decoding %s from xml %p\n", type, fe));
	d(printf ("Name = %s\n", name));
	xmlFree (fe->name);
	fe->name = name;
	xmlFree (fi->type);
	fi->type = type;
	n = node->children;
	while (n) {
		if (!strcmp (n->name, type)) {
			if (!(str = xmlNodeGetContent (n)))
				str = xmlStrdup ("");
			
			d(printf ("  '%s'\n", str));
			fi->values = g_list_append (fi->values, g_strdup (str));
			xmlFree (str);
		} else if (n->type == XML_ELEMENT_NODE) {
			g_warning ("Unknown node type '%s' encountered decoding a %s\n", n->name, type);
		}
		n = n->next;
	}
	
	return 0;
}

static void
entry_changed (GtkEntry *entry, FilterElement *fe)
{
	FilterInput *fi = (FilterInput *) fe;
	const char *new;
	GList *l;
	
	new = gtk_entry_get_text (entry);
	
	d(printf("entry_changed '%s'\n", new));

	/* NOTE: entry only supports a single value ... */
	l = fi->values;
	while (l) {
		g_free (l->data);
		l = l->next;
	}
	
	g_list_free (fi->values);
	
	fi->values = g_list_append (NULL, g_strdup (new));
}

static GtkWidget *
get_widget (FilterElement *fe)
{
	GtkWidget *entry;
	FilterInput *fi = (FilterInput *)fe;
	
	entry = gtk_entry_new ();
	if (fi->values && fi->values->data)
		gtk_entry_set_text (GTK_ENTRY (entry), (const char *) fi->values->data);
	
	g_signal_connect (entry, "changed", G_CALLBACK (entry_changed), fe);
	
	return entry;
}

static void
build_code (FilterElement *fe, GString *out, struct _FilterPart *ff)
{
	;
}

static void
format_sexp (FilterElement *fe, GString *out)
{
	FilterInput *fi = (FilterInput *) fe;
	GList *l;

	d(printf("format_sexp, first elem=%p\n", fi->values));
	
	l = fi->values;
	while (l) {
		e_sexp_encode_string (out, l->data);
		l = l->next;
	}
}
