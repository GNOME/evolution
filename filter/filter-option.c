/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Not Zed <notzed@lostzed.mmc.com.au>
 *      Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gmodule.h>

#include "filter-option.h"
#include "filter-part.h"
#include <libedataserver/e-sexp.h>

#define d(x)

static gint option_eq (FilterElement *fe, FilterElement *cm);
static void xml_create (FilterElement *fe, xmlNodePtr node);
static xmlNodePtr xml_encode (FilterElement *fe);
static gint xml_decode (FilterElement *fe, xmlNodePtr node);
static FilterElement *clone (FilterElement *fe);
static GtkWidget *get_widget (FilterElement *fe);
static void build_code (FilterElement *fe, GString *out, struct _FilterPart *ff);
static void format_sexp (FilterElement *, GString *);
static GSList *get_dynamic_options (FilterOption *fo);

static void filter_option_class_init (FilterOptionClass *klass);
static void filter_option_init (FilterOption *fo);
static void filter_option_finalise (GObject *obj);

static FilterElementClass *parent_class;

GType
filter_option_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (FilterOptionClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) filter_option_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (FilterOption),
			0,    /* n_preallocs */
			(GInstanceInitFunc) filter_option_init,
		};

		type = g_type_register_static (FILTER_TYPE_ELEMENT, "FilterOption", &info, 0);
	}

	return type;
}

static void
filter_option_class_init (FilterOptionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	FilterElementClass *fe_class = FILTER_ELEMENT_CLASS (klass);

	parent_class = g_type_class_ref (FILTER_TYPE_ELEMENT);

	object_class->finalize = filter_option_finalise;

	/* override methods */
	fe_class->eq = option_eq;
	fe_class->xml_create = xml_create;
	fe_class->xml_encode = xml_encode;
	fe_class->xml_decode = xml_decode;
	fe_class->clone = clone;
	fe_class->get_widget = get_widget;
	fe_class->build_code = build_code;
	fe_class->format_sexp = format_sexp;
}

static void
filter_option_init (FilterOption *fo)
{
	fo->type = "option";
	fo->dynamic_func = NULL;
}

static void
free_option (struct _filter_option *o, gpointer data)
{
	g_free (o->title);
	g_free (o->value);
	g_free (o->code);
	g_free (o);
}

static void
filter_option_finalise (GObject *obj)
{
	FilterOption *fo = (FilterOption *) obj;

	g_list_foreach (fo->options, (GFunc)free_option, NULL);
	g_list_free (fo->options);
	g_free (fo->dynamic_func);

        G_OBJECT_CLASS (parent_class)->finalize (obj);
}

/**
 * filter_option_new:
 *
 * Create a new FilterOption object.
 *
 * Return value: A new #FilterOption object.
 **/
FilterOption *
filter_option_new (void)
{
	return (FilterOption *) g_object_new (FILTER_TYPE_OPTION, NULL, NULL);
}

static struct _filter_option *
find_option (FilterOption *fo, const gchar *name)
{
	GList *l = fo->options;
	struct _filter_option *op;

	while (l) {
		op = l->data;
		if (!strcmp (name, op->value)) {
			return op;
		}
		l = g_list_next (l);
	}

	return NULL;
}

void
filter_option_set_current (FilterOption *option, const gchar *name)
{
	g_return_if_fail (IS_FILTER_OPTION(option));

	option->current = find_option (option, name);
}

/* used by implementers to add additional options */
struct _filter_option *
filter_option_add(FilterOption *fo, const gchar *value, const gchar *title, const gchar *code, gboolean is_dynamic)
{
	struct _filter_option *op;

	g_return_val_if_fail (IS_FILTER_OPTION(fo), NULL);
	g_return_val_if_fail(find_option(fo, value) == NULL, NULL);

	op = g_malloc(sizeof(*op));
	op->title = g_strdup(title);
	op->value = g_strdup(value);
	op->code = g_strdup(code);
	op->is_dynamic = is_dynamic;

	fo->options = g_list_append(fo->options, op);
	if (fo->current == NULL)
		fo->current = op;

	return op;
}

const gchar *
filter_option_get_current (FilterOption *option)
{
	g_return_val_if_fail (IS_FILTER_OPTION (option), NULL);

	if (!option->current)
		return NULL;

	return option->current->value;
}

void
filter_option_remove_all (FilterOption *fo)
{
	g_return_if_fail (IS_FILTER_OPTION (fo));

	g_list_foreach (fo->options, (GFunc)free_option, NULL);
	g_list_free (fo->options);
	fo->options = NULL;

	fo->current = NULL;
}

static gint
option_eq(FilterElement *fe, FilterElement *cm)
{
	FilterOption *fo = (FilterOption *)fe, *co = (FilterOption *)cm;

	return FILTER_ELEMENT_CLASS (parent_class)->eq (fe, cm)
		&& ((fo->current && co->current && strcmp(fo->current->value, co->current->value) == 0)
		    || (fo->current == NULL && co->current == NULL));
}

static void
xml_create (FilterElement *fe, xmlNodePtr node)
{
	FilterOption *fo = (FilterOption *)fe;
	xmlNodePtr n, work;

	/* parent implementation */
        FILTER_ELEMENT_CLASS (parent_class)->xml_create (fe, node);

	n = node->children;
	while (n) {
		if (!strcmp ((gchar *)n->name, "option")) {
			gchar *tmp, *value, *title = NULL, *code = NULL;

			value = (gchar *)xmlGetProp (n, (const guchar *)"value");
			work = n->children;
			while (work) {
				if (!strcmp ((gchar *)work->name, "title") || !strcmp ((gchar *)work->name, "_title")) {
					if (!title) {
						if (!(tmp = (gchar *)xmlNodeGetContent (work)))
							tmp = (gchar *)xmlStrdup ((const guchar *)"");

						title = g_strdup (tmp);
						xmlFree (tmp);
					}
				} else if (!strcmp ((gchar *)work->name, "code")) {
					if (!code) {
						if (!(tmp = (gchar *)xmlNodeGetContent (work)))
							tmp = (gchar *)xmlStrdup ((const guchar *)"");

						code = g_strdup (tmp);
						xmlFree (tmp);
					}
				}
				work = work->next;
			}

			filter_option_add (fo, value, title, code, FALSE);
			xmlFree (value);
			g_free (title);
			g_free (code);
		} else if (g_str_equal ((gchar *)n->name, "dynamic")) {
			if (fo->dynamic_func) {
				g_warning ("Only one 'dynamic' node is acceptable in the optionlist '%s'", fe->name);
			} else {
				/* Expecting only one <dynamic func="cb" /> in the option list,
				   The 'cb' should be of this prototype:
				   GSList *cb (void);
				   returning GSList of struct _filter_option, all newly allocated, because it'll
				   be freed with g_free and g_slist_free. 'is_dynamic' member is ignored here.
				*/
				xmlChar *fn;

				fn = xmlGetProp (n, (const guchar *)"func");
				if (fn && *fn) {
					GSList *items, *i;
					struct _filter_option *op;

					fo->dynamic_func = g_strdup ((const gchar *)fn);

					/* get options now, to have them available when reading saved rules */
					items = get_dynamic_options (fo);
					for (i = items; i; i = i->next) {
						op = i->data;

						if (op) {
							filter_option_add (fo, op->value, op->title, op->code, TRUE);
							free_option (op, NULL);
						}
					}

					g_slist_free (items);
				} else {
					g_warning ("Missing 'func' attribute within '%s' node in optionlist '%s'", n->name, fe->name);
				}

				xmlFree (fn);
			}
		} else if (n->type == XML_ELEMENT_NODE) {
			g_warning ("Unknown xml node within optionlist: %s\n", n->name);
		}
		n = n->next;
	}
}

static xmlNodePtr
xml_encode (FilterElement *fe)
{
	xmlNodePtr value;
	FilterOption *fo = (FilterOption *)fe;

	d(printf ("Encoding option as xml\n"));
	value = xmlNewNode (NULL, (const guchar *)"value");
	xmlSetProp (value, (const guchar *)"name", (guchar *)fe->name);
	xmlSetProp (value, (const guchar *)"type", (guchar *)fo->type);
	if (fo->current)
		xmlSetProp (value, (const guchar *)"value", (guchar *)fo->current->value);

	return value;
}

static gint
xml_decode (FilterElement *fe, xmlNodePtr node)
{
	FilterOption *fo = (FilterOption *)fe;
	gchar *value;

	d(printf ("Decoding option from xml\n"));
	xmlFree (fe->name);
	fe->name = (gchar *)xmlGetProp (node, (const guchar *)"name");
	value = (gchar *)xmlGetProp (node, (const guchar *)"value");
	if (value) {
		fo->current = find_option (fo, value);
		xmlFree (value);
	} else {
		fo->current = NULL;
	}
	return 0;
}

static void
combobox_changed (GtkWidget *widget, FilterElement *fe)
{
	FilterOption *fo = (FilterOption *)fe;

	fo->current = (struct _filter_option *) g_list_nth_data (fo->options, gtk_combo_box_get_active (GTK_COMBO_BOX (widget)));
}

static GSList *
get_dynamic_options (FilterOption *fo)
{
	GModule *module;
	GSList *(*get_func)(void);
	GSList *res = NULL;

	if (!fo || !fo->dynamic_func)
		return res;

	module = g_module_open (NULL, G_MODULE_BIND_LAZY);

	if (g_module_symbol (module, fo->dynamic_func, (gpointer) &get_func)) {
		res = get_func ();
	} else {
		g_warning ("optionlist dynamic fill function '%s' not found", fo->dynamic_func);
	}

	g_module_close (module);

	return res;
}

static GtkWidget *
get_widget (FilterElement *fe)
{
	FilterOption *fo = (FilterOption *)fe;
	GtkWidget *combobox;
	GList *l;
	struct _filter_option *op;
	gint index = 0, current = 0;

	if (fo->dynamic_func) {
		/* it is dynamically filled, thus remove all dynamics and put there the fresh ones */
		GSList *items, *i;
		GList *old_ops;
		struct _filter_option *old_cur;

		old_ops = fo->options;
		old_cur = fo->current;
		l = old_ops;

		/* start with an empty list */
		fo->current = NULL;
		fo->options = NULL;

		for (l = fo->options; l; l = l->next) {
			op = l->data;

			if (op->is_dynamic) {
				break;
			} else {
				filter_option_add (fo, op->value, op->title, op->code, FALSE);
			}
		}

		items = get_dynamic_options (fo);
		for (i = items; i; i = i->next) {
			op = i->data;

			if (op) {
				filter_option_add (fo, op->value, op->title, op->code, TRUE);
				free_option (op, NULL);
			}
		}

		g_slist_free (items);

		/* maybe some static left after those dynamic, add them too */
		for (; l; l = l->next) {
			op = l->data;

			if (!op->is_dynamic)
				filter_option_add (fo, op->value, op->title, op->code, FALSE);
		}

		if (old_cur)
			filter_option_set_current (fo, old_cur->value);

		/* free old list */
		g_list_foreach (old_ops, (GFunc)free_option, NULL);
		g_list_free (old_ops);
	}

	combobox = gtk_combo_box_new_text ();
	l = fo->options;
	while (l) {
		op = l->data;
		gtk_combo_box_append_text (GTK_COMBO_BOX (combobox), _(op->title));

		if (op == fo->current)
			current = index;

		l = g_list_next (l);
		index++;
	}

	g_signal_connect (combobox, "changed", G_CALLBACK (combobox_changed), fe);
	gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), current);

	return combobox;
}

static void
build_code (FilterElement *fe, GString *out, struct _FilterPart *ff)
{
	FilterOption *fo = (FilterOption *)fe;

	d(printf ("building option code %p, current = %p\n", fo, fo->current));

	if (fo->current && fo->current->code)
		filter_part_expand_code (ff, fo->current->code, out);
}

static void
format_sexp (FilterElement *fe, GString *out)
{
	FilterOption *fo = (FilterOption *)fe;

	if (fo->current)
		e_sexp_encode_string (out, fo->current->value);
}

static FilterElement *
clone (FilterElement *fe)
{
	FilterOption *fo = (FilterOption *)fe, *new;
	GList *l;
	struct _filter_option *op, *newop;

	d(printf ("cloning option\n"));

        new = FILTER_OPTION (FILTER_ELEMENT_CLASS (parent_class)->clone (fe));
	l = fo->options;
	while (l) {
		op = l->data;
		newop = filter_option_add (new, op->value, op->title, op->code, op->is_dynamic);
		if (fo->current == op)
			new->current = newop;
		l = l->next;
	}

	new->dynamic_func = g_strdup (fo->dynamic_func);

	d(printf ("cloning option code %p, current = %p\n", new, new->current));

	return (FilterElement *) new;
}
