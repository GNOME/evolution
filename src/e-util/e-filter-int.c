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
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <stdlib.h>
#include <gtk/gtk.h>

#include "e-filter-int.h"

G_DEFINE_TYPE (
	EFilterInt,
	e_filter_int,
	E_TYPE_FILTER_ELEMENT)

static void
filter_int_spin_changed (GtkSpinButton *spin_button,
                         EFilterElement *element)
{
	EFilterInt *filter_int = E_FILTER_INT (element);

	filter_int->val = gtk_spin_button_get_value_as_int (spin_button);
}

static void
filter_int_finalize (GObject *object)
{
	EFilterInt *filter_int = E_FILTER_INT (object);

	g_free (filter_int->type);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_filter_int_parent_class)->finalize (object);
}

static gint
filter_int_eq (EFilterElement *element_a,
               EFilterElement *element_b)
{
	EFilterInt *filter_int_a = E_FILTER_INT (element_a);
	EFilterInt *filter_int_b = E_FILTER_INT (element_b);

	/* Chain up to parent's eq() method. */
	if (!E_FILTER_ELEMENT_CLASS (e_filter_int_parent_class)->
		eq (element_a, element_b))
		return FALSE;

	return (filter_int_a->val == filter_int_b->val);
}

static EFilterElement *
filter_int_clone (EFilterElement *element)
{
	EFilterInt *filter_int = E_FILTER_INT (element);
	EFilterInt *clone;

	clone = (EFilterInt *) e_filter_int_new_type (
		filter_int->type, filter_int->min, filter_int->max);
	clone->val = filter_int->val;

	E_FILTER_ELEMENT (clone)->name = g_strdup (element->name);

	return E_FILTER_ELEMENT (clone);
}

static xmlNodePtr
filter_int_xml_encode (EFilterElement *element)
{
	EFilterInt *filter_int = E_FILTER_INT (element);
	xmlNodePtr value;
	gchar intval[32];
	const gchar *type;

	type = filter_int->type ? filter_int->type : "integer";

	value = xmlNewNode (NULL, (xmlChar *)"value");
	xmlSetProp (value, (xmlChar *) "name", (xmlChar *) element->name);
	xmlSetProp (value, (xmlChar *) "type", (xmlChar *) type);

	sprintf (intval, "%d", filter_int->val);
	xmlSetProp (value, (xmlChar *) type, (xmlChar *) intval);

	return value;
}

static gint
filter_int_xml_decode (EFilterElement *element,
                       xmlNodePtr node)
{
	EFilterInt *filter_int = E_FILTER_INT (element);
	gchar *name, *type;
	gchar *intval;

	name = (gchar *) xmlGetProp (node, (xmlChar *)"name");
	xmlFree (element->name);
	element->name = name;

	type = (gchar *) xmlGetProp (node, (xmlChar *)"type");
	g_free (filter_int->type);
	filter_int->type = g_strdup (type);
	xmlFree (type);

	intval = (gchar *) xmlGetProp (
		node, (xmlChar *) (filter_int->type ?
		filter_int->type : "integer"));
	if (intval) {
		filter_int->val = atoi (intval);
		xmlFree (intval);
	} else {
		filter_int->val = 0;
	}

	return 0;
}

static GtkWidget *
filter_int_get_widget (EFilterElement *element)
{
	EFilterInt *filter_int = E_FILTER_INT (element);
	GtkWidget *widget;
	GtkAdjustment *adjustment;

	adjustment = GTK_ADJUSTMENT (gtk_adjustment_new (
		0.0, (gfloat) filter_int->min,
		(gfloat) filter_int->max, 1.0, 1.0, 0));
	widget = gtk_spin_button_new (
		adjustment,
		filter_int->max > filter_int->min + 1000 ? 5.0 : 1.0, 0);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (widget), TRUE);

	if (filter_int->val)
		gtk_spin_button_set_value (
			GTK_SPIN_BUTTON (widget), (gfloat) filter_int->val);

	g_signal_connect (
		widget, "value-changed",
		G_CALLBACK (filter_int_spin_changed), element);

	return widget;
}

static void
filter_int_format_sexp (EFilterElement *element,
                        GString *out)
{
	EFilterInt *filter_int = E_FILTER_INT (element);

	if (filter_int->val < 0)
		/* See #364731 #457523 C6*/
		g_string_append_printf (out, "(- 0 %d)", (filter_int->val * -1));
	else
		g_string_append_printf (out, "%d", filter_int->val);
}

static void
filter_int_describe (EFilterElement *element,
		     GString *out)
{
	EFilterInt *filter_int = E_FILTER_INT (element);

	g_string_append_printf (out, "%c%d%c",
		E_FILTER_ELEMENT_DESCRIPTION_VALUE_START,
		filter_int->val,
		E_FILTER_ELEMENT_DESCRIPTION_VALUE_END);
}

static void
e_filter_int_class_init (EFilterIntClass *class)
{
	GObjectClass *object_class;
	EFilterElementClass *filter_element_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = filter_int_finalize;

	filter_element_class = E_FILTER_ELEMENT_CLASS (class);
	filter_element_class->eq = filter_int_eq;
	filter_element_class->clone = filter_int_clone;
	filter_element_class->xml_encode = filter_int_xml_encode;
	filter_element_class->xml_decode = filter_int_xml_decode;
	filter_element_class->get_widget = filter_int_get_widget;
	filter_element_class->format_sexp = filter_int_format_sexp;
	filter_element_class->describe = filter_int_describe;
}

static void
e_filter_int_init (EFilterInt *filter_int)
{
	filter_int->min = 0;
	filter_int->max = G_MAXINT;
}

EFilterElement *
e_filter_int_new (void)
{
	return g_object_new (E_TYPE_FILTER_INT, NULL);
}

EFilterElement *
e_filter_int_new_type (const gchar *type,
                       gint min,
                       gint max)
{
	EFilterInt *filter_int;

	filter_int = g_object_new (E_TYPE_FILTER_INT, NULL);

	filter_int->type = g_strdup (type);
	filter_int->min = min;
	filter_int->max = max;

	return E_FILTER_ELEMENT (filter_int);
}

void
e_filter_int_set_value (EFilterInt *filter_int,
                        gint value)
{
	g_return_if_fail (E_IS_FILTER_INT (filter_int));

	filter_int->val = value;
}
