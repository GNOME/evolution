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

#include <stdlib.h>
#include <string.h>

#include <gtk/gtkbox.h>
#include <gtk/gtkhbox.h>
#include <libgnome/gnome-i18n.h>

#include "filter-part.h"
#include "rule-context.h"

#define d(x)

static void filter_part_class_init (FilterPartClass *klass);
static void filter_part_init (FilterPart *fp);
static void filter_part_finalise (GObject *obj);


static GObjectClass *parent_class = NULL;


GType
filter_part_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (FilterPartClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) filter_part_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (FilterPart),
			0,    /* n_preallocs */
			(GInstanceInitFunc) filter_part_init,
		};
		
		type = g_type_register_static (G_TYPE_OBJECT, "FilterPart", &info, 0);
	}
	
	return type;
}

static void
filter_part_class_init (FilterPartClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (G_TYPE_OBJECT);
	
	object_class->finalize = filter_part_finalise;
}

static void
filter_part_init (FilterPart *fp)
{
	;
}

static void
filter_part_finalise (GObject *obj)
{
	FilterPart *fp = (FilterPart *) obj;
	GList *l;
	
	l = fp->elements;
	while (l) {
		g_object_unref (l->data);
		l = l->next;
	}
	
	g_list_free (fp->elements);
	g_free (fp->name);
	g_free (fp->title);
	g_free (fp->code);
	
        G_OBJECT_CLASS (parent_class)->finalize (obj);
}

/**
 * filter_part_new:
 *
 * Create a new FilterPart object.
 * 
 * Return value: A new #FilterPart object.
 **/
FilterPart *
filter_part_new (void)
{
	return (FilterPart *) g_object_new (FILTER_TYPE_PART, NULL, NULL);
}

gboolean
filter_part_validate (FilterPart *fp)
{
	gboolean correct = TRUE;
	GList *l;
	
	l = fp->elements;
	while (l && correct) {
		FilterElement *fe = l->data;
		
	        correct = filter_element_validate (fe);
		
		l = l->next;
	}
	
	return correct;
}

int
filter_part_eq (FilterPart *fp, FilterPart *fc)
{
	int truth;
	GList *al, *bl;
	
	truth = ((fp->name && fc->name && strcmp(fp->name, fc->name) == 0)
		 || (fp->name == NULL && fc->name == NULL))
		&& ((fp->title && fc->title && strcmp(fp->title, fc->title) == 0)
		    || (fp->title == NULL && fc->title == NULL))
		&& ((fp->code && fc->code && strcmp(fp->code, fc->code) == 0)
		    || (fp->code == NULL && fc->code == NULL));
	
	al = fp->elements;
	bl = fc->elements;
	while (truth && al && bl) {
		FilterElement *a = al->data, *b = bl->data;
		
		truth = filter_element_eq(a, b);
		
		al = al->next;
		bl = bl->next;
	}
	
	return truth && al == NULL && bl == NULL;
}

int
filter_part_xml_create (FilterPart *ff, xmlNodePtr node, RuleContext *rc)
{
	xmlNodePtr n;
	char *type, *str;
	FilterElement *el;
	
	str = xmlGetProp (node, "name");
	ff->name = g_strdup (str);
	if (str)
		xmlFree (str);
	
	n = node->children;
	while (n) {
		if (!strcmp (n->name, "input")) {
			type = xmlGetProp (n, "type");
			d(printf ("creating new element type input '%s'\n", type));
			if (type != NULL
			    && (el = rule_context_new_element(rc, type)) != NULL) {
				filter_element_xml_create (el, n);
				xmlFree (type);
				d(printf ("adding element part %p %s\n", ff, el, el->name));
				ff->elements = g_list_append (ff->elements, el);
			} else {
				g_warning ("Invalid xml format, missing/unknown input type");
			}
		} else if (!strcmp (n->name, "title")) {
			if (!ff->title) {
				str = xmlNodeGetContent (n);
				ff->title = g_strdup (str);
				if (str)
					xmlFree (str);
			}
		} else if (!strcmp (n->name, "code")) {
			if (!ff->code) {
				str = xmlNodeGetContent (n);
				ff->code = g_strdup (str);
				if (str)
					xmlFree (str);
			}
		} else if (n->type == XML_ELEMENT_NODE) {
			g_warning ("Unknown part element in xml: %s\n", n->name);
		}
		n = n->next;
	}
	
	return 0;
}

xmlNodePtr
filter_part_xml_encode (FilterPart *fp)
{
	GList *l;
	FilterElement *fe;
	xmlNodePtr part, value;
	
	g_return_val_if_fail (fp != NULL, NULL);
	
	part = xmlNewNode (NULL, "part");
	xmlSetProp (part, "name", fp->name);
	l = fp->elements;
	while (l) {
		fe = l->data;
		value = filter_element_xml_encode (fe);
		xmlAddChild (part, value);
		l = g_list_next (l);
	}
	
	return part;
}


int
filter_part_xml_decode (FilterPart *fp, xmlNodePtr node)
{
	FilterElement *fe;
	xmlNodePtr n;
	char *name;
	
	g_return_val_if_fail (fp != NULL, -1);
	g_return_val_if_fail (node != NULL, -1);
	
	n = node->children;
	while (n) {
		if (!strcmp (n->name, "value")) {
			name = xmlGetProp (n, "name");
			d(printf ("finding element part %p %s = %p\n", name, name, fe));
			fe = filter_part_find_element (fp, name);
			d(printf ("finding element part %p %s = %p\n", name, name, fe));
			xmlFree (name);
			if (fe)
				filter_element_xml_decode (fe, n);
		}
		n = n->next;
	}
	
	return 0;
}

FilterPart *
filter_part_clone (FilterPart *fp)
{
	FilterPart *new;
	GList *l;
	FilterElement *fe, *ne;
	
	new = (FilterPart *) g_object_new (G_OBJECT_TYPE (fp), NULL, NULL);
	new->name = g_strdup (fp->name);
	new->title = g_strdup (fp->title);
	new->code = g_strdup (fp->code);
	l = fp->elements;
	while (l) {
		fe = l->data;
		ne = filter_element_clone (fe);
		new->elements = g_list_append (new->elements, ne);
		l = g_list_next (l);
	}
	
	return new;
}

/* only copies values of matching parts in the right order */
void
filter_part_copy_values (FilterPart *dst, FilterPart *src)
{
	GList *dstl, *srcl, *dstt;
	FilterElement *de, *se;
	
	/* NOTE: we go backwards, it just works better that way */
	
	/* for each source type, search the dest type for
	   a matching type in the same order */
	srcl = g_list_last (src->elements);
	dstl = g_list_last (dst->elements);
	while (srcl && dstl) {
		se = srcl->data;
		dstt = dstl;
		while (dstt) {
			de = dstt->data;
			if (FILTER_PART_GET_CLASS (de) == FILTER_PART_GET_CLASS (se)) {
				filter_element_copy_value (de, se);
				dstl = dstt->prev;
				break;
			}
			dstt = dstt->prev;
		}
		
		srcl = srcl->prev;
	}
}

FilterElement *
filter_part_find_element (FilterPart *ff, const char *name)
{
	GList *l = ff->elements;
	FilterElement *fe;
	
	if (name == NULL)
		return NULL;
	
	while (l) {
		fe = l->data;
		if (fe->name && !strcmp (fe->name, name))
			return fe;
		l = g_list_next (l);
	}
	
	return NULL;
}


GtkWidget *
filter_part_get_widget (FilterPart *ff)
{
	GtkWidget *hbox;
	GList *l = ff->elements;
	FilterElement *fe;
	GtkWidget *w;
	
	hbox = gtk_hbox_new (FALSE, 3);
	
	while (l) {
		fe = l->data;
		w = filter_element_get_widget (fe);
		if (w)
			gtk_box_pack_start (GTK_BOX (hbox), w, FALSE, FALSE, 3);
		
		l = g_list_next (l);
	}
	
	gtk_widget_show_all (hbox);
	
	return hbox;
}

/**
 * filter_part_build_code:
 * @ff: 
 * @out: 
 * 
 * Outputs the code of a part.
 **/
void
filter_part_build_code (FilterPart *ff, GString *out)
{
	GList *l = ff->elements;
	FilterElement *fe;
	
	if (ff->code)
		filter_part_expand_code (ff, ff->code, out);
	
	while (l) {
		fe = l->data;
		filter_element_build_code (fe, out, ff);
		l = l->next;
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
filter_part_build_code_list (GList *l, GString *out)
{
	FilterPart *fp;
	
	while (l) {
		fp = l->data;
		filter_part_build_code (fp, out);
		g_string_append (out, "\n  ");
		l = l->next;
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
FilterPart *
filter_part_find_list (GList *l, const char *name)
{
	FilterPart *part;
	
	d(printf ("Find part named %s\n", name));
	
	while (l) {
		part = l->data;
		if (!strcmp (part->name, name)) {
			d(printf ("Found!\n"));
			return part;
		}
		l = l->next;
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
FilterPart *
filter_part_next_list (GList *l, FilterPart *last)
{
	GList *node = l;
	
	if (last != NULL) {
		node = g_list_find (node, last);
		if (node == NULL)
			node = l;
		else
			node = node->next;
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
void
filter_part_expand_code (FilterPart *ff, const char *source, GString *out)
{
	const char *newstart, *start, *end;
	char *name = alloca (32);
	int len, namelen = 32;
	FilterElement *fe;
	
	start = source;
	while (start && (newstart = strstr (start, "${"))
		&& (end = strstr (newstart+2, "}")) ) {
		len = end - newstart - 2;
		if (len + 1 > namelen) {
			namelen = (len + 1) * 2;
			name = g_alloca (namelen);
		}
		memcpy (name, newstart+2, len);
		name[len] = 0;
		fe = filter_part_find_element (ff, name);
		d(printf("expand code: looking up variab le '%s' = %p\n", ff, name, fe));
		if (fe) {
			g_string_append_printf (out, "%.*s", newstart-start, start);
			filter_element_format_sexp (fe, out);
#if 0
		} else if ((val = g_hash_table_lookup (ff->globals, name))) {
			g_string_append_printf (out, "%.*s", newstart-start, start);
			e_sexp_encode_string (out, val);
#endif
		} else {
			g_string_append_printf (out, "%.*s", end-start+1, start);
		}
		start = end + 1;
	}
	g_string_append (out, start);
}

#if 0
int main(int argc, char **argv)
{
	GtkWidget *dialog, *w;
	xmlDocPtr system;
	xmlNodePtr node;
	FilterPart *ff;
	GString *code;
	
	gnome_init ("test", "0.0", argc, argv);
	
	system = xmlParseFile ("form.xml");
	if (system == NULL) {
		printf("i/o error\n");
		return 1;
	}
	
	ff = filter_part_new ();
	filter_part_xml_create (ff, system->root);
	
	w = filter_part_get_widget (ff);
	
	dialog = gtk_dialog_new ();
	gtk_dialog_add_buttons ((GtkDialog *) dialog, GTK_BUTTONS_OK, NULL);
	gtk_dialog_set_has_separator ((GtkDialog *) dialog, FALSE);
	gtk_window_set_title ((GtkWindow *) dialog, _("Test"));
	gtk_window_set_policy ((GtkWindow *) dialog, FALSE, TRUE, FALSE);
	gtk_box_pack_start ((GtkBox *) dialog->vbox, w, TRUE, TRUE, 0);
	
	gtk_dialog_run ((GtkDialog *) dialog);
	gtk_widget_destroy (dialog);
	
	code = g_string_new ("");
	filter_part_build_code (ff, code);
	printf("code is:\n%s\n", code->str);
	
	return 0;
}
#endif
