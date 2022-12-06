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
#include <sys/types.h>
#include <regex.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gmodule.h>

#include "e-alert.h"
#include "e-filter-input.h"
#include "e-filter-part.h"

G_DEFINE_TYPE (
	EFilterInput,
	e_filter_input,
	E_TYPE_FILTER_ELEMENT)

static void
filter_input_entry_changed (GtkEntry *entry,
                            EFilterElement *element)
{
	EFilterInput *input = E_FILTER_INPUT (element);
	const gchar *text;

	g_list_foreach (input->values, (GFunc) g_free, NULL);
	g_list_free (input->values);

	text = gtk_entry_get_text (entry);
	input->values = g_list_append (NULL, g_strdup (text));
}

static void
filter_input_finalize (GObject *object)
{
	EFilterInput *input = E_FILTER_INPUT (object);

	xmlFree (input->type);
	g_free (input->code_gen_func);

	g_list_foreach (input->values, (GFunc) g_free, NULL);
	g_list_free (input->values);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_filter_input_parent_class)->finalize (object);
}

static gboolean
filter_input_validate (EFilterElement *element,
                       EAlert **alert)
{
	EFilterInput *input = E_FILTER_INPUT (element);
	gboolean valid = TRUE;

	g_warn_if_fail (alert == NULL || *alert == NULL);

	if (input->values && input->type && !strcmp (input->type, "regex")) {
		const gchar *pattern;
		regex_t regexpat;
		gint regerr;

		pattern = input->values->data;

		regerr = regcomp (
			&regexpat, pattern,
			REG_EXTENDED | REG_NEWLINE | REG_ICASE);
		if (regerr != 0) {
			if (alert) {
				gsize reglen;
				gchar *regmsg;

				/* regerror gets called twice to get the full error string
				 * length to do proper posix error reporting */
				reglen = regerror (regerr, &regexpat, 0, 0);
				regmsg = g_malloc0 (reglen + 1);
				regerror (regerr, &regexpat, regmsg, reglen);

				*alert = e_alert_new ("filter:bad-regexp",
						      pattern, regmsg, NULL);
				g_free (regmsg);
			}

			valid = FALSE;
		}

		regfree (&regexpat);
	} else if (!input->allow_empty && (!input->values || !input->values->next)) {
		const gchar *value = input->values ? input->values->data : NULL;
		gboolean is_empty = value == NULL;

		if (value) {
			gint ii;

			is_empty = TRUE;

			for (ii = 0; value[ii]; ii++) {
				if (!g_ascii_isspace (value[ii])) {
					is_empty = FALSE;
					break;
				}
			}
		}

		if (is_empty) {
			valid = FALSE;
			if (alert)
				*alert = e_alert_new ("filter:not-allow-empty", NULL);
		}
	}

	return valid;
}

static gint
filter_input_eq (EFilterElement *element_a,
                 EFilterElement *element_b)
{
	EFilterInput *input_a = E_FILTER_INPUT (element_a);
	EFilterInput *input_b = E_FILTER_INPUT (element_b);
	GList *link_a;
	GList *link_b;

	/* Chain up to parent's eq() method. */
	if (!E_FILTER_ELEMENT_CLASS (e_filter_input_parent_class)->
		eq (element_a, element_b))
		return FALSE;

	if (g_strcmp0 (input_a->type, input_b->type) != 0)
		return FALSE;

	link_a = input_a->values;
	link_b = input_b->values;

	while (link_a != NULL && link_b != NULL) {
		if (g_strcmp0 (link_a->data, link_b->data) != 0)
			return FALSE;

		link_a = g_list_next (link_a);
		link_b = g_list_next (link_b);
	}

	if (link_a != NULL || link_b != NULL)
		return FALSE;

	if (g_strcmp0 (input_a->code_gen_func, input_b->code_gen_func) != 0)
		return FALSE;

	return input_a->allow_empty == input_b->allow_empty;
}

static void
filter_input_xml_create (EFilterElement *element,
                         xmlNodePtr node)
{
	EFilterInput *input = E_FILTER_INPUT (element);
	xmlNodePtr n;
	gchar *allow_empty;

	g_free (input->code_gen_func);
	input->code_gen_func = NULL;

	/* Chain up to parent's method. */
	E_FILTER_ELEMENT_CLASS (e_filter_input_parent_class)->xml_create (element, node);

	allow_empty = (gchar *) xmlGetProp (node, (xmlChar *) "allow-empty");

	input->allow_empty = !allow_empty || g_strcmp0 (allow_empty, "true") == 0;
	xmlFree (allow_empty);

	for (n = node->children; n; n = n->next) {
		if (g_str_equal ((const gchar *) n->name, "code")) {
			xmlChar *func = xmlGetProp (n, (xmlChar *) "func");

			if (func && *func) {
				g_free (input->code_gen_func);
				input->code_gen_func = g_strdup ((gchar *) func);
			}

			if (func)
				xmlFree (func);
			break;
		}
	}
}

static xmlNodePtr
filter_input_xml_encode (EFilterElement *element)
{
	EFilterInput *input = E_FILTER_INPUT (element);
	xmlNodePtr value, cur;
	GList *link;
	const gchar *type;

	type = input->type ? input->type : "string";

	value = xmlNewNode (NULL, (xmlChar *) "value");
	xmlSetProp (value, (xmlChar *) "name", (xmlChar *) element->name);
	xmlSetProp (value, (xmlChar *) "type", (xmlChar *) type);
	xmlSetProp (value, (xmlChar *) "allow-empty", (xmlChar *) (input->allow_empty ? "true" : "false"));

	for (link = input->values; link != NULL; link = g_list_next (link)) {
		xmlChar *str = link->data;

		cur = xmlNewChild (value, NULL, (xmlChar *) type, NULL);

		str = xmlEncodeEntitiesReentrant (NULL, str);
		xmlNodeSetContent (cur, str);
		xmlFree (str);
	}

	return value;
}

static gint
filter_input_xml_decode (EFilterElement *element,
                         xmlNodePtr node)
{
	EFilterInput *input = (EFilterInput *) element;
	gchar *name, *str, *type, *allow_empty;
	xmlNodePtr child;

	g_list_foreach (input->values, (GFunc) g_free, NULL);
	g_list_free (input->values);
	input->values = NULL;

	name = (gchar *) xmlGetProp (node, (xmlChar *) "name");
	type = (gchar *) xmlGetProp (node, (xmlChar *) "type");
	allow_empty = (gchar *) xmlGetProp (node, (xmlChar *) "allow-empty");

	xmlFree (element->name);
	element->name = name;

	xmlFree (input->type);
	input->type = type;

	input->allow_empty = !allow_empty || g_strcmp0 (allow_empty, "true") == 0;
	xmlFree (allow_empty);

	child = node->children;
	while (child != NULL) {
		if (!strcmp ((gchar *) child->name, type)) {
			if (!(str = (gchar *) xmlNodeGetContent (child)))
				str = (gchar *) xmlStrdup ((xmlChar *)"");

			input->values = g_list_append (input->values, g_strdup (str));
			xmlFree (str);
		} else if (child->type == XML_ELEMENT_NODE) {
			g_warning (
				"Unknown node type '%s' encountered "
				"decoding a %s\n", child->name, type);
		}
		child = child->next;
	}

	return 0;
}

static EFilterElement *
filter_input_clone (EFilterElement *element)
{
	EFilterInput *input = E_FILTER_INPUT (element);
	EFilterInput *clone_input;
	EFilterElement *clone;
	GList *link;

	/* Chain up to parent's clone() method. */
	clone = E_FILTER_ELEMENT_CLASS (e_filter_input_parent_class)->clone (element);

	clone_input = E_FILTER_INPUT (clone);

	if (clone_input->type)
		xmlFree (clone_input->type);
	clone_input->type = input->type ? (gchar *) xmlStrdup ((const xmlChar *) input->type) : NULL;
	clone_input->allow_empty = input->allow_empty;
	clone_input->code_gen_func = g_strdup (input->code_gen_func);

	g_list_free_full (clone_input->values, g_free);
	clone_input->values = NULL;

	for (link = input->values; link != NULL; link = g_list_next (link)) {
		clone_input->values = g_list_prepend (clone_input->values, g_strdup (link->data));
	}

	clone_input->values = g_list_reverse (clone_input->values);

	return clone;
}

static GtkWidget *
filter_input_get_widget (EFilterElement *element)
{
	EFilterInput *input = E_FILTER_INPUT (element);
	GtkWidget *entry;

	entry = gtk_entry_new ();
	if (input->values && input->values->data)
		gtk_entry_set_text (
			GTK_ENTRY (entry), input->values->data);

	g_signal_connect (
		entry, "changed",
		G_CALLBACK (filter_input_entry_changed), element);

	return entry;
}

static void
filter_input_format_sexp (EFilterElement *element,
                          GString *out)
{
	EFilterInput *input = E_FILTER_INPUT (element);
	GList *link;

	if (input->code_gen_func)
		return;

	for (link = input->values; link != NULL; link = g_list_next (link))
		camel_sexp_encode_string (out, link->data);
}

static void
filter_input_build_code (EFilterElement *element,
			 GString *out,
			 EFilterPart *part)
{
	EFilterInput *input = E_FILTER_INPUT (element);
	GModule *module;
	void (*code_gen_func) (EFilterElement *element, GString *out, EFilterPart *part);

	if (!input->code_gen_func)
		return;

	module = g_module_open (NULL, G_MODULE_BIND_LAZY);

	if (g_module_symbol (module, input->code_gen_func, (gpointer) &code_gen_func)) {
		code_gen_func (E_FILTER_ELEMENT (input), out, part);
	} else {
		g_warning (
			"input dynamic code function '%s' not found",
			input->code_gen_func);
	}

	g_module_close (module);
}

static void
filter_input_describe (EFilterElement *element,
		       GString *out)
{
	EFilterInput *input = E_FILTER_INPUT (element);
	GList *link;
	gboolean added = FALSE;

	g_string_append_c (out, E_FILTER_ELEMENT_DESCRIPTION_VALUE_START);

	for (link = input->values; link; link = g_list_next (link)) {
		const gchar *value = link->data;

		if (value && *value) {
			if (added)
				g_string_append_c (out, ' ');
			else
				added = TRUE;

			g_string_append (out, value);
		}
	}

	g_string_append_c (out, E_FILTER_ELEMENT_DESCRIPTION_VALUE_END);
}

static void
e_filter_input_class_init (EFilterInputClass *class)
{
	GObjectClass *object_class;
	EFilterElementClass *filter_element_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = filter_input_finalize;

	filter_element_class = E_FILTER_ELEMENT_CLASS (class);
	filter_element_class->validate = filter_input_validate;
	filter_element_class->eq = filter_input_eq;
	filter_element_class->xml_create = filter_input_xml_create;
	filter_element_class->xml_encode = filter_input_xml_encode;
	filter_element_class->xml_decode = filter_input_xml_decode;
	filter_element_class->clone = filter_input_clone;
	filter_element_class->get_widget = filter_input_get_widget;
	filter_element_class->format_sexp = filter_input_format_sexp;
	filter_element_class->build_code = filter_input_build_code;
	filter_element_class->describe = filter_input_describe;
}

static void
e_filter_input_init (EFilterInput *input)
{
	input->values = g_list_prepend (NULL, g_strdup (""));
	input->allow_empty = TRUE;
	input->code_gen_func = NULL;
}

/**
 * filter_input_new:
 *
 * Create a new EFilterInput object.
 *
 * Return value: A new #EFilterInput object.
 **/
EFilterInput *
e_filter_input_new (void)
{
	return g_object_new (E_TYPE_FILTER_INPUT, NULL);
}

EFilterInput *
e_filter_input_new_type_name (const gchar *type)
{
	EFilterInput *input;

	input = e_filter_input_new ();
	input->type = (gchar *) xmlStrdup ((xmlChar *) type);

	return input;
}

void
e_filter_input_set_value (EFilterInput *input,
                          const gchar *value)
{
	g_return_if_fail (E_IS_FILTER_INPUT (input));

	g_list_foreach (input->values, (GFunc) g_free, NULL);
	g_list_free (input->values);

	input->values = g_list_append (NULL, g_strdup (value));
}
