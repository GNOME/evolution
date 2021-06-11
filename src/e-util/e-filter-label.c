/*
 * Copyright (C) 2021 Red Hat (www.redhat.com)
 *
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
 */

#include "evolution-config.h"

#include <string.h>

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>

#include "e-filter-part.h"

#include "e-filter-label.h"

struct _EFilterLabelPrivate {
	gchar *title;
};

G_DEFINE_TYPE_WITH_PRIVATE (EFilterLabel, e_filter_label, E_TYPE_FILTER_ELEMENT)

static void
filter_label_finalize (GObject *object)
{
	EFilterLabel *label = E_FILTER_LABEL (object);

	g_free (label->priv->title);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_filter_label_parent_class)->finalize (object);
}

static gint
filter_label_eq (EFilterElement *element_a,
		 EFilterElement *element_b)
{
	EFilterLabel *label_a = E_FILTER_LABEL (element_a);
	EFilterLabel *label_b = E_FILTER_LABEL (element_b);

	/* Chain up to parent's method. */
	if (!E_FILTER_ELEMENT_CLASS (e_filter_label_parent_class)->eq (element_a, element_b))
		return FALSE;

	return g_strcmp0 (label_a->priv->title, label_b->priv->title) == 0;
}

static void
filter_label_xml_create (EFilterElement *element,
			 xmlNodePtr node)
{
	EFilterLabel *label = E_FILTER_LABEL (element);
	xmlNodePtr n;

	/* Chain up to parent's method. */
	E_FILTER_ELEMENT_CLASS (e_filter_label_parent_class)->xml_create (element, node);

	n = node->children;
	while (n) {
		if (!g_strcmp0 ((gchar *) n->name, "title") ||
		    !g_strcmp0 ((gchar *) n->name, "_title")) {
			if (!label->priv->title) {
				xmlChar *tmp;

				tmp = xmlNodeGetContent (n);
				label->priv->title = tmp ? g_strdup ((gchar *) tmp) : NULL;
				if (tmp)
					xmlFree (tmp);
			}
		} else if (n->type == XML_ELEMENT_NODE) {
			g_warning ("Unknown xml node within 'label': %s\n", n->name);
		}
		n = n->next;
	}
}

static xmlNodePtr
filter_label_xml_encode (EFilterElement *element)
{
	xmlNodePtr value;

	value = xmlNewNode (NULL, (xmlChar *) "value");
	xmlSetProp (value, (xmlChar *) "name", (xmlChar *) element->name);

	return value;
}

static gint
filter_label_xml_decode (EFilterElement *element,
			 xmlNodePtr node)
{
	xmlFree (element->name);
	element->name = (gchar *) xmlGetProp (node, (xmlChar *) "name");

	return 0;
}

static EFilterElement *
filter_label_clone (EFilterElement *element)
{
	EFilterLabel *label = E_FILTER_LABEL (element);
	EFilterLabel *clone_label;
	EFilterElement *clone;

	/* Chain up to parent's method. */
	clone = E_FILTER_ELEMENT_CLASS (e_filter_label_parent_class)->clone (element);

	clone_label = E_FILTER_LABEL (clone);
	clone_label->priv->title = g_strdup (label->priv->title);

	return clone;
}

static GtkWidget *
filter_label_get_widget (EFilterElement *element)
{
	EFilterLabel *label = E_FILTER_LABEL (element);
	GtkWidget *widget;

	widget = gtk_label_new ((label->priv->title && *label->priv->title) ? _(label->priv->title) : "");

	return widget;
}

static void
filter_label_build_code (EFilterElement *element,
			 GString *out,
			 EFilterPart *part)
{
}

static void
filter_label_format_sexp (EFilterElement *element,
                          GString *out)
{
}

static void
filter_label_describe (EFilterElement *element,
		       GString *out)
{
	EFilterLabel *label = E_FILTER_LABEL (element);

	if (label->priv->title && *label->priv->title)
		g_string_append (out, _(label->priv->title));
}

static void
e_filter_label_class_init (EFilterLabelClass *class)
{
	GObjectClass *object_class;
	EFilterElementClass *filter_element_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = filter_label_finalize;

	filter_element_class = E_FILTER_ELEMENT_CLASS (class);
	filter_element_class->eq = filter_label_eq;
	filter_element_class->xml_create = filter_label_xml_create;
	filter_element_class->xml_encode = filter_label_xml_encode;
	filter_element_class->xml_decode = filter_label_xml_decode;
	filter_element_class->clone = filter_label_clone;
	filter_element_class->get_widget = filter_label_get_widget;
	filter_element_class->build_code = filter_label_build_code;
	filter_element_class->format_sexp = filter_label_format_sexp;
	filter_element_class->describe = filter_label_describe;
}

static void
e_filter_label_init (EFilterLabel *label)
{
	label->priv = e_filter_label_get_instance_private (label);
}

EFilterElement *
e_filter_label_new (void)
{
	return g_object_new (E_TYPE_FILTER_LABEL, NULL);
}

void
e_filter_label_set_title (EFilterLabel *label,
			  const gchar *title)
{
	g_return_if_fail (E_IS_FILTER_LABEL (label));

	if (label->priv->title != title) {
		g_free (label->priv->title);
		label->priv->title = g_strdup (title);
	}
}

const gchar *
e_filter_label_get_title (EFilterLabel *label)
{
	g_return_val_if_fail (E_IS_FILTER_LABEL (label), NULL);

	return label->priv->title;
}
