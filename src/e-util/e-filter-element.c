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
 *      Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <string.h>
#include <stdlib.h>

#include "e-filter-element.h"
#include "e-filter-part.h"

typedef EFilterElement * (*EFilterElementFunc) (gpointer data);

struct _element_type {
	gchar *name;

	EFilterElementFunc create;
	gpointer data;
};

G_DEFINE_TYPE (
	EFilterElement,
	e_filter_element,
	G_TYPE_OBJECT)

static gboolean
filter_element_validate (EFilterElement *element,
                         EAlert **alert)
{
	return TRUE;
}

static gint
filter_element_eq (EFilterElement *element_a,
                   EFilterElement *element_b)
{
	return (g_strcmp0 (element_a->name, element_b->name) == 0);
}

static void
filter_element_xml_create (EFilterElement *element,
                           xmlNodePtr node)
{
	element->name = (gchar *) xmlGetProp (node, (xmlChar *) "name");
}

static EFilterElement *
filter_element_clone (EFilterElement *element)
{
	EFilterElement *clone;
	xmlNodePtr node;

	clone = g_object_new (G_OBJECT_TYPE (element), NULL);

	node = e_filter_element_xml_encode (element);
	e_filter_element_xml_decode (clone, node);
	xmlFreeNodeList (node);

	return clone;
}

/* This is somewhat hackish, implement all the base cases in here */
#include "e-filter-input.h"
#include "e-filter-option.h"
#include "e-filter-code.h"
#include "e-filter-color.h"
#include "e-filter-datespec.h"
#include "e-filter-int.h"
#include "e-filter-file.h"

static void
filter_element_copy_value (EFilterElement *dst_element,
                           EFilterElement *src_element)
{
	if (E_IS_FILTER_INPUT (src_element)) {
		EFilterInput *src_input;

		src_input = E_FILTER_INPUT (src_element);

		if (E_IS_FILTER_INPUT (dst_element)) {
			EFilterInput *dst_input;

			dst_input = E_FILTER_INPUT (dst_element);

			if (src_input->values)
				e_filter_input_set_value (
					dst_input,
					src_input->values->data);

		} else if (E_IS_FILTER_INT (dst_element)) {
			EFilterInt *dst_int;

			dst_int = E_FILTER_INT (dst_element);

			dst_int->val = atoi (src_input->values->data);
		}

	} else if (E_IS_FILTER_COLOR (src_element)) {
		EFilterColor *src_color;

		src_color = E_FILTER_COLOR (src_element);

		if (E_IS_FILTER_COLOR (dst_element)) {
			EFilterColor *dst_color;

			dst_color = E_FILTER_COLOR (dst_element);

			dst_color->color = src_color->color;
		}

	} else if (E_IS_FILTER_DATESPEC (src_element)) {
		EFilterDatespec *src_datespec;

		src_datespec = E_FILTER_DATESPEC (src_element);

		if (E_IS_FILTER_DATESPEC (dst_element)) {
			EFilterDatespec *dst_datespec;

			dst_datespec = E_FILTER_DATESPEC (dst_element);

			dst_datespec->type = src_datespec->type;
			dst_datespec->value = src_datespec->value;
		}

	} else if (E_IS_FILTER_INT (src_element)) {
		EFilterInt *src_int;

		src_int = E_FILTER_INT (src_element);

		if (E_IS_FILTER_INT (dst_element)) {
			EFilterInt *dst_int;

			dst_int = E_FILTER_INT (dst_element);

			dst_int->val = src_int->val;

		} else if (E_IS_FILTER_INPUT (dst_element)) {
			EFilterInput *dst_input;
			gchar *values;

			dst_input = E_FILTER_INPUT (dst_element);

			values = g_strdup_printf ("%d", src_int->val);
			e_filter_input_set_value (dst_input, values);
			g_free (values);
		}

	} else if (E_IS_FILTER_OPTION (src_element)) {
		EFilterOption *src_option;

		src_option = E_FILTER_OPTION (src_element);

		if (E_IS_FILTER_OPTION (dst_element)) {
			EFilterOption *dst_option;

			dst_option = E_FILTER_OPTION (dst_element);

			if (src_option->current)
				e_filter_option_set_current (
					dst_option,
					src_option->current->value);
		}
	}
}

static void
filter_element_finalize (GObject *object)
{
	EFilterElement *element = E_FILTER_ELEMENT (object);

	xmlFree (element->name);

	/* Chain up to parent's finalize () method. */
	G_OBJECT_CLASS (e_filter_element_parent_class)->finalize (object);
}

static void
e_filter_element_class_init (EFilterElementClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = filter_element_finalize;

	class->validate = filter_element_validate;
	class->eq = filter_element_eq;
	class->xml_create = filter_element_xml_create;
	class->clone = filter_element_clone;
	class->copy_value = filter_element_copy_value;
}

static void
e_filter_element_init (EFilterElement *element)
{
}

/**
 * filter_element_new:
 *
 * Create a new EFilterElement object.
 *
 * Return value: A new #EFilterElement object.
 **/
EFilterElement *
e_filter_element_new (void)
{
	return g_object_new (E_TYPE_FILTER_ELEMENT, NULL);
}

gboolean
e_filter_element_validate (EFilterElement *element,
                           EAlert **alert)
{
	EFilterElementClass *class;

	g_return_val_if_fail (E_IS_FILTER_ELEMENT (element), FALSE);

	class = E_FILTER_ELEMENT_GET_CLASS (element);
	g_return_val_if_fail (class != NULL, FALSE);
	g_return_val_if_fail (class->validate != NULL, FALSE);

	return class->validate (element, alert);
}

gint
e_filter_element_eq (EFilterElement *element_a,
                     EFilterElement *element_b)
{
	EFilterElementClass *class;

	g_return_val_if_fail (E_IS_FILTER_ELEMENT (element_a), FALSE);
	g_return_val_if_fail (E_IS_FILTER_ELEMENT (element_b), FALSE);

	/* The elements must be the same type. */
	if (G_OBJECT_TYPE (element_a) != G_OBJECT_TYPE (element_b))
		return FALSE;

	class = E_FILTER_ELEMENT_GET_CLASS (element_a);
	g_return_val_if_fail (class != NULL, FALSE);
	g_return_val_if_fail (class->eq != NULL, FALSE);

	return class->eq (element_a, element_b);
}

/**
 * filter_element_xml_create:
 * @fe: filter element
 * @node: xml node
 *
 * Create a new filter element based on an xml definition of
 * that element.
 **/
void
e_filter_element_xml_create (EFilterElement *element,
                             xmlNodePtr node)
{
	EFilterElementClass *class;

	g_return_if_fail (E_IS_FILTER_ELEMENT (element));
	g_return_if_fail (node != NULL);

	class = E_FILTER_ELEMENT_GET_CLASS (element);
	g_return_if_fail (class != NULL);
	g_return_if_fail (class->xml_create != NULL);

	class->xml_create (element, node);
}

/**
 * filter_element_xml_encode:
 * @fe: filter element
 *
 * Encode the values of a filter element into xml format.
 *
 * Return value:
 **/
xmlNodePtr
e_filter_element_xml_encode (EFilterElement *element)
{
	EFilterElementClass *class;

	g_return_val_if_fail (E_IS_FILTER_ELEMENT (element), NULL);

	class = E_FILTER_ELEMENT_GET_CLASS (element);
	g_return_val_if_fail (class != NULL, NULL);
	g_return_val_if_fail (class->xml_encode != NULL, NULL);

	return class->xml_encode (element);
}

/**
 * filter_element_xml_decode:
 * @fe: filter element
 * @node: xml node
 *
 * Decode the values of a fitler element from xml format.
 *
 * Return value:
 **/
gint
e_filter_element_xml_decode (EFilterElement *element,
                             xmlNodePtr node)
{
	EFilterElementClass *class;

	g_return_val_if_fail (E_IS_FILTER_ELEMENT (element), FALSE);
	g_return_val_if_fail (node != NULL, FALSE);

	class = E_FILTER_ELEMENT_GET_CLASS (element);
	g_return_val_if_fail (class != NULL, FALSE);
	g_return_val_if_fail (class->xml_decode != NULL, FALSE);

	return class->xml_decode (element, node);
}

/**
 * filter_element_clone:
 * @fe: filter element
 *
 * Clones the EFilterElement @fe.
 *
 * Return value:
 **/
EFilterElement *
e_filter_element_clone (EFilterElement *element)
{
	EFilterElementClass *class;

	g_return_val_if_fail (E_IS_FILTER_ELEMENT (element), NULL);

	class = E_FILTER_ELEMENT_GET_CLASS (element);
	g_return_val_if_fail (class != NULL, NULL);
	g_return_val_if_fail (class->clone != NULL, NULL);

	return class->clone (element);
}

/**
 * e_filter_element_get_widget:
 * @fe: filter element
 *
 * Create a widget to represent this element.
 *
 * Returns: (transfer full): a new GtkWidget
 **/
GtkWidget *
e_filter_element_get_widget (EFilterElement *element)
{
	EFilterElementClass *class;

	g_return_val_if_fail (E_IS_FILTER_ELEMENT (element), NULL);

	class = E_FILTER_ELEMENT_GET_CLASS (element);
	g_return_val_if_fail (class != NULL, NULL);
	g_return_val_if_fail (class->get_widget != NULL, NULL);

	return class->get_widget (element);
}

/**
 * filter_element_build_code:
 * @fe: filter element
 * @out: output buffer
 * @ff:
 *
 * Add the code representing this element to the output string @out.
 **/
void
e_filter_element_build_code (EFilterElement *element,
                             GString *out,
                             EFilterPart *part)
{
	EFilterElementClass *class;

	g_return_if_fail (E_IS_FILTER_ELEMENT (element));
	g_return_if_fail (out != NULL);
	g_return_if_fail (E_IS_FILTER_PART (part));

	class = E_FILTER_ELEMENT_GET_CLASS (element);
	g_return_if_fail (class != NULL);

	/* This method is optional. */
	if (class->build_code != NULL)
		class->build_code (element, out, part);
}

/**
 * filter_element_format_sexp:
 * @fe: filter element
 * @out: output buffer
 *
 * Format the value(s) of this element in a method suitable for the context of
 * sexp where it is used.  Usually as space separated, double-quoted strings.
 **/
void
e_filter_element_format_sexp (EFilterElement *element,
                              GString *out)
{
	EFilterElementClass *class;

	g_return_if_fail (E_IS_FILTER_ELEMENT (element));
	g_return_if_fail (out != NULL);

	class = E_FILTER_ELEMENT_GET_CLASS (element);
	g_return_if_fail (class != NULL);
	g_return_if_fail (class->format_sexp != NULL);

	class->format_sexp (element, out);
}

void
e_filter_element_set_data (EFilterElement *element,
                           gpointer data)
{
	g_return_if_fail (E_IS_FILTER_ELEMENT (element));

	element->data = data;
}

/* only copies the value, not the name/type */
void
e_filter_element_copy_value (EFilterElement *dst_element,
                             EFilterElement *src_element)
{
	EFilterElementClass *class;

	g_return_if_fail (E_IS_FILTER_ELEMENT (dst_element));
	g_return_if_fail (E_IS_FILTER_ELEMENT (src_element));

	class = E_FILTER_ELEMENT_GET_CLASS (dst_element);
	g_return_if_fail (class != NULL);
	g_return_if_fail (class->copy_value != NULL);

	class->copy_value (dst_element, src_element);
}

/**
 * e_filter_element_describe:
 * @fe: filter element
 * @out: a #GString to add the description to
 *
 * Describes the @element in a human-readable way.
 **/
void
e_filter_element_describe (EFilterElement *element,
			   GString *out)
{
	EFilterElementClass *klass;

	g_return_if_fail (E_IS_FILTER_ELEMENT (element));
	g_return_if_fail (out != NULL);

	klass = E_FILTER_ELEMENT_GET_CLASS (element);
	g_return_if_fail (klass != NULL);
	g_return_if_fail (klass->describe != NULL);

	klass->describe (element, out);
}
