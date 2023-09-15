/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Not Zed <notzed@lostzed.mmc.com.au>
 *      Jepartrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "e-filter-file.h"
#include "e-filter-input.h"
#include "e-filter-part.h"
#include "e-rule-context.h"

G_DEFINE_TYPE (
	EFilterPart,
	e_filter_part,
	G_TYPE_OBJECT)

static void
filter_part_finalize (GObject *object)
{
	EFilterPart *part = E_FILTER_PART (object);

	g_list_foreach (part->elements, (GFunc) g_object_unref, NULL);
	g_list_free (part->elements);

	g_free (part->name);
	g_free (part->title);
	g_free (part->code);
	g_free (part->code_gen_func);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_filter_part_parent_class)->finalize (object);
}

static void
e_filter_part_class_init (EFilterPartClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = filter_part_finalize;
}

static void
e_filter_part_init (EFilterPart *part)
{
}

/**
 * e_filter_part_new:
 *
 * Create a new EFilterPart object.
 *
 * Return value: A new #EFilterPart object.
 **/
EFilterPart *
e_filter_part_new (void)
{
	return g_object_new (E_TYPE_FILTER_PART, NULL);
}

gboolean
e_filter_part_validate (EFilterPart *part,
                        EAlert **alert)
{
	GList *link;

	g_return_val_if_fail (E_IS_FILTER_PART (part), FALSE);

	/* The part is valid if all of its elements are valid. */
	for (link = part->elements; link != NULL; link = g_list_next (link)) {
		EFilterElement *element = link->data;

		if (!e_filter_element_validate (element, alert))
			return FALSE;
	}

	return TRUE;
}

gint
e_filter_part_eq (EFilterPart *part_a,
                  EFilterPart *part_b)
{
	GList *link_a, *link_b;

	g_return_val_if_fail (E_IS_FILTER_PART (part_a), FALSE);
	g_return_val_if_fail (E_IS_FILTER_PART (part_b), FALSE);

	if (g_strcmp0 (part_a->name, part_b->name) != 0)
		return FALSE;

	if (g_strcmp0 (part_a->title, part_b->title) != 0)
		return FALSE;

	if (g_strcmp0 (part_a->code, part_b->code) != 0)
		return FALSE;

	if (g_strcmp0 (part_a->code_gen_func, part_b->code_gen_func) != 0)
		return FALSE;

	link_a = part_a->elements;
	link_b = part_b->elements;

	while (link_a != NULL && link_b != NULL) {
		EFilterElement *element_a = link_a->data;
		EFilterElement *element_b = link_b->data;

		if (!e_filter_element_eq (element_a, element_b))
			return FALSE;

		link_a = g_list_next (link_a);
		link_b = g_list_next (link_b);
	}

	if (link_a != NULL || link_b != NULL)
		return FALSE;

	return TRUE;
}

gint
e_filter_part_xml_create (EFilterPart *part,
                          xmlNodePtr node,
                          ERuleContext *context)
{
	xmlNodePtr n;
	gchar *type, *str;
	EFilterElement *el;

	g_return_val_if_fail (E_IS_FILTER_PART (part), 0);
	g_return_val_if_fail (node != NULL, 0);
	g_return_val_if_fail (E_IS_RULE_CONTEXT (context), 0);

	str = (gchar *) xmlGetProp (node, (xmlChar *)"name");
	part->name = g_strdup (str);
	if (str)
		xmlFree (str);

	n = node->children;
	while (n) {
		if (!strcmp ((gchar *) n->name, "input")) {
			type = (gchar *) xmlGetProp (n, (xmlChar *)"type");
			if (type != NULL
			    && (el = e_rule_context_new_element (context, type)) != NULL) {
				e_filter_element_xml_create (el, n);
				xmlFree (type);
				part->elements = g_list_append (part->elements, el);
			} else {
				g_warning ("Invalid xml format, missing/unknown input type");
			}
		} else if (!strcmp ((gchar *) n->name, "title") ||
			   !strcmp ((gchar *) n->name, "_title")) {
			if (!part->title) {
				str = (gchar *) xmlNodeGetContent (n);
				part->title = g_strdup (str);
				if (str)
					xmlFree (str);
			}
		} else if (!strcmp ((gchar *) n->name, "code")) {
			if (part->code || part->code_gen_func) {
				g_warning ("Element 'code' defined twice in part '%s'", part->name);
			} else {
				xmlChar *fn;

				/* if element 'code' has attribute 'func', then
				 * the content of the element is ignored and only
				 * the 'func' is used to generate actual rule code;
				 * The function prototype is:
				 * void code_gen_func (EFilterPart *part, GString *out);
				 * @part is the part for which generate the code
				 * @out is GString where to add the code
				*/
				fn = xmlGetProp (n, (const xmlChar *) "func");
				if (fn && *fn) {
					part->code_gen_func = g_strdup ((const gchar *) fn);
				} else {
					str = (gchar *) xmlNodeGetContent (n);
					part->code = g_strdup (str);
					if (str)
						xmlFree (str);
				}

				if (fn)
					xmlFree (fn);
			}
		} else if (n->type == XML_ELEMENT_NODE) {
			g_warning ("Unknown part element in xml: %s\n", n->name);
		}
		n = n->next;
	}

	return 0;
}

xmlNodePtr
e_filter_part_xml_encode (EFilterPart *part)
{
	xmlNodePtr node;
	GList *link;

	g_return_val_if_fail (E_IS_FILTER_PART (part), NULL);

	node = xmlNewNode (NULL, (xmlChar *)"part");
	xmlSetProp (node, (xmlChar *)"name", (xmlChar *) part->name);

	for (link = part->elements; link != NULL; link = g_list_next (link)) {
		EFilterElement *element = link->data;
		xmlNodePtr value;

		value = e_filter_element_xml_encode (element);
		xmlAddChild (node, value);
	}

	return node;
}

gint
e_filter_part_xml_decode (EFilterPart *part,
                          xmlNodePtr node)
{
	xmlNodePtr child;

	g_return_val_if_fail (E_IS_FILTER_PART (part), -1);
	g_return_val_if_fail (node != NULL, -1);

	for (child = node->children; child != NULL; child = child->next) {
		EFilterElement *element;
		xmlChar *name;

		if (strcmp ((gchar *) child->name, "value") != 0)
			continue;

		name = xmlGetProp (child, (xmlChar *) "name");
		element = e_filter_part_find_element (part, (gchar *) name);
		xmlFree (name);

		if (element != NULL)
			e_filter_element_xml_decode (element, child);
	}

	return 0;
}

EFilterPart *
e_filter_part_clone (EFilterPart *part)
{
	EFilterPart *clone;
	GList *link;

	g_return_val_if_fail (E_IS_FILTER_PART (part), NULL);

	clone = g_object_new (G_OBJECT_TYPE (part), NULL, NULL);
	clone->name = g_strdup (part->name);
	clone->title = g_strdup (part->title);
	clone->code = g_strdup (part->code);
	clone->code_gen_func = g_strdup (part->code_gen_func);

	for (link = part->elements; link != NULL; link = g_list_next (link)) {
		EFilterElement *element = link->data;
		EFilterElement *clone_element;

		clone_element = e_filter_element_clone (element);
		clone->elements = g_list_append (clone->elements, clone_element);
	}

	return clone;
}

/* only copies values of matching parts in the right order */
void
e_filter_part_copy_values (EFilterPart *dst_part,
                           EFilterPart *src_part)
{
	GList *dst_link, *src_link;

	g_return_if_fail (E_IS_FILTER_PART (dst_part));
	g_return_if_fail (E_IS_FILTER_PART (src_part));

	/* NOTE: we go backwards, it just works better that way */

	/* for each source type, search the dest type for
	 * a matching type in the same order */
	src_link = g_list_last (src_part->elements);
	dst_link = g_list_last (dst_part->elements);

	while (src_link != NULL && dst_link != NULL) {
		EFilterElement *src_element = src_link->data;
		GList *link = dst_link;

		while (link != NULL) {
			EFilterElement *dst_element = link->data;
			GType dst_type = G_OBJECT_TYPE (dst_element);
			GType src_type = G_OBJECT_TYPE (src_element);

			if (dst_type == src_type) {
				e_filter_element_copy_value (
					dst_element, src_element);
				dst_link = g_list_previous (link);
				break;
			}

			link = g_list_previous (link);
		}

		src_link = g_list_previous (src_link);
	}
}

EFilterElement *
e_filter_part_find_element (EFilterPart *part,
                            const gchar *name)
{
	GList *link;

	g_return_val_if_fail (E_IS_FILTER_PART (part), NULL);

	if (name == NULL)
		return NULL;

	for (link = part->elements; link != NULL; link = g_list_next (link)) {
		EFilterElement *element = link->data;

		if (g_strcmp0 (element->name, name) == 0)
			return element;
	}

	return NULL;
}

GtkWidget *
e_filter_part_get_widget (EFilterPart *part)
{
	GtkWidget *hbox;
	GList *link;

	g_return_val_if_fail (E_IS_FILTER_PART (part), NULL);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 3);

	for (link = part->elements; link != NULL; link = g_list_next (link)) {
		EFilterElement *element = link->data;
		GtkWidget *widget;

		widget = e_filter_element_get_widget (element);
		if (widget != NULL) {
			gboolean expand_fill = E_IS_FILTER_FILE (element) || E_IS_FILTER_INPUT (element);
			gtk_box_pack_start (
				GTK_BOX (hbox), widget, expand_fill, expand_fill, 3);
		}
	}

	gtk_widget_show_all (hbox);

	return hbox;
}

void
e_filter_part_describe (EFilterPart *part,
			GString *out)
{
	GList *link;

	g_return_if_fail (E_IS_FILTER_PART (part));
	g_return_if_fail (out != NULL);

	g_string_append (out, _(part->title));

	for (link = part->elements; link != NULL; link = g_list_next (link)) {
		EFilterElement *element = link->data;

		g_string_append_c (out, ' ');
		e_filter_element_describe (element, out);
	}
}

static void
filter_part_generate_code (EFilterPart *part,
			   GString *out)
{
	GModule *module;
	gpointer ptr = NULL;
	void (*code_gen_func) (EFilterPart *part, GString *out);

	if (!part || !part->code_gen_func)
		return;

	module = g_module_open (NULL, G_MODULE_BIND_LAZY);

	if (g_module_symbol (module, part->code_gen_func, &ptr)) {
		code_gen_func = ptr;
		code_gen_func (part, out);
	} else {
		g_warning (
			"part's dynamic code function '%s' not found",
			part->code_gen_func);
	}

	g_module_close (module);
}

/**
 * e_filter_part_build_code:
 * @part:
 * @out:
 *
 * Outputs the code of a part.
 **/
void
e_filter_part_build_code (EFilterPart *part,
                          GString *out)
{
	GList *link;

	g_return_if_fail (E_IS_FILTER_PART (part));
	g_return_if_fail (out != NULL);

	if (part->code_gen_func) {
		filter_part_generate_code (part, out);
	} else if (part->code != NULL) {
		e_filter_part_expand_code (part, part->code, out);
	}

	for (link = part->elements; link != NULL; link = g_list_next (link)) {
		EFilterElement *element = link->data;
		e_filter_element_build_code (element, out, part);
	}
}

/**
 * e_filter_part_build_code_list:
 * @list:
 * @out:
 *
 * Construct a list of the filter parts code into
 * a single string.
 **/
void
e_filter_part_build_code_list (GList *list,
                               GString *out)
{
	GList *link;

	g_return_if_fail (out != NULL);

	for (link = list; link != NULL; link = g_list_next (link)) {
		EFilterPart *part = link->data;

		e_filter_part_build_code (part, out);
		g_string_append (out, "\n  ");
	}
}

/**
 * e_filter_part_find_list:
 * @list:
 * @name:
 *
 * Find a filter part stored in a list.
 *
 * Return value:
 **/
EFilterPart *
e_filter_part_find_list (GList *list,
                         const gchar *name)
{
	GList *link;

	g_return_val_if_fail (name != NULL, NULL);

	for (link = list; link != NULL; link = g_list_next (link)) {
		EFilterPart *part = link->data;

		if (g_strcmp0 (part->name, name) == 0)
			return part;
	}

	return NULL;
}

/**
 * e_filter_part_next_list:
 * @list:
 * @last: The last item retrieved, or NULL to start
 * from the beginning of the list.
 *
 * Iterate through a filter part list.
 *
 * Return value: The next value in the list, or NULL if the
 * list is expired.
 **/
EFilterPart *
e_filter_part_next_list (GList *list,
                         EFilterPart *last)
{
	GList *link = list;

	if (last != NULL) {
		link = g_list_find (list, last);
		if (link == NULL)
			link = list;
		else
			link = link->next;
	}

	return (link != NULL) ? link->data : NULL;
}

/**
 * e_filter_part_expand_code:
 * @part:
 * @str:
 * @out:
 *
 * Expands the variables in string @str based on the values of the part.
 **/
void
e_filter_part_expand_code (EFilterPart *part,
                           const gchar *source,
                           GString *out)
{
	const gchar *newstart, *start, *end;
	gchar *name = g_alloca (32);
	gint len, namelen = 32;

	g_return_if_fail (E_IS_FILTER_PART (part));
	g_return_if_fail (source != NULL);
	g_return_if_fail (out != NULL);

	start = source;

	while (start && (newstart = strstr (start, "${"))
		&& (end = strstr (newstart + 2, "}"))) {
		EFilterElement *element;

		len = end - newstart - 2;
		if (len + 1 > namelen) {
			namelen = (len + 1) * 2;
			name = g_alloca (namelen);
		}
		memcpy (name, newstart + 2, len);
		name[len] = 0;

		element = e_filter_part_find_element (part, name);
		if (element != NULL) {
			g_string_append_printf (out, "%.*s", (gint)(newstart - start), start);
			e_filter_element_format_sexp (element, out);
#if 0
		} else if ((val = g_hash_table_lookup (part->globals, name))) {
			g_string_append_printf (out, "%.*s", newstart - start, start);
			camel_sexp_encode_string (out, val);
#endif
		} else {
			g_string_append_printf (out, "%.*s", (gint)(end - start + 1), start);
		}
		start = end + 1;
	}

	g_string_append (out, start);
}
