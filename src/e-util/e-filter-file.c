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

#include <string.h>
#include <sys/types.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include "e-alert.h"
#include "e-filter-file.h"
#include "e-filter-part.h"

G_DEFINE_TYPE (
	EFilterFile,
	e_filter_file,
	E_TYPE_FILTER_ELEMENT)

static void
filter_file_filename_changed (GtkFileChooser *file_chooser,
                              EFilterElement *element)
{
	EFilterFile *file = E_FILTER_FILE (element);
	const gchar *path;

	path = gtk_file_chooser_get_filename (file_chooser);

	g_free (file->path);
	file->path = g_strdup (path);
}

static void
filter_file_finalize (GObject *object)
{
	EFilterFile *file = E_FILTER_FILE (object);

	xmlFree (file->type);
	g_free (file->path);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_filter_file_parent_class)->finalize (object);
}

static gboolean
filter_file_validate (EFilterElement *element,
                      EAlert **alert)
{
	EFilterFile *file = E_FILTER_FILE (element);

	g_warn_if_fail (alert == NULL || *alert == NULL);

	if (!file->path) {
		if (alert)
			*alert = e_alert_new ("filter:no-file", NULL);
		return FALSE;
	}

	/* FIXME: do more to validate command-lines? */

	if (g_strcmp0 (file->type, "file") == 0) {
		if (!g_file_test (file->path, G_FILE_TEST_IS_REGULAR)) {
			if (alert)
				*alert = e_alert_new ("filter:bad-file",
						       file->path, NULL);
			return FALSE;
		}
	} else if (g_strcmp0 (file->type, "command") == 0) {
		/* Only requirements so far is that the
		 * command can't be an empty string. */
		return (file->path[0] != '\0');
	}

	return TRUE;
}

static gint
filter_file_eq (EFilterElement *element_a,
                EFilterElement *element_b)
{
	EFilterFile *file_a = E_FILTER_FILE (element_a);
	EFilterFile *file_b = E_FILTER_FILE (element_b);

	/* Chain up to parent's eq() method. */
	if (!E_FILTER_ELEMENT_CLASS (e_filter_file_parent_class)->
		eq (element_a, element_b))
		return FALSE;

	if (g_strcmp0 (file_a->path, file_b->path) != 0)
		return FALSE;

	if (g_strcmp0 (file_a->type, file_b->type) != 0)
		return FALSE;

	return TRUE;
}

static xmlNodePtr
filter_file_xml_encode (EFilterElement *element)
{
	EFilterFile *file = E_FILTER_FILE (element);
	xmlNodePtr cur, value;
	const gchar *type;

	type = file->type ? file->type : "file";

	value = xmlNewNode (NULL, (xmlChar *)"value");
	xmlSetProp (value, (xmlChar *) "name", (xmlChar *) element->name);
	xmlSetProp (value, (xmlChar *) "type", (xmlChar *) type);

	cur = xmlNewChild (value, NULL, (xmlChar *) type, NULL);
	xmlNodeSetContent (cur, (xmlChar *) file->path);

	return value;
}

static gint
filter_file_xml_decode (EFilterElement *element,
                        xmlNodePtr node)
{
	EFilterFile *file = E_FILTER_FILE (element);
	gchar *name, *str, *type;
	xmlNodePtr child;

	name = (gchar *) xmlGetProp (node, (xmlChar *) "name");
	type = (gchar *) xmlGetProp (node, (xmlChar *) "type");

	xmlFree (element->name);
	element->name = name;

	xmlFree (file->type);
	file->type = type;

	g_free (file->path);
	file->path = NULL;

	child = node->children;
	while (child != NULL) {
		if (!strcmp ((gchar *) child->name, type)) {
			str = (gchar *) xmlNodeGetContent (child);
			file->path = g_strdup (str ? str : "");
			xmlFree (str);

			break;
		} else if (child->type == XML_ELEMENT_NODE) {
			g_warning (
				"Unknown node type '%s' encountered "
				"decoding a %s\n", child->name, type);
		}

		child = child->next;
	}

	return 0;
}

static GtkWidget *
filter_file_get_widget (EFilterElement *element)
{
	EFilterFile *file = E_FILTER_FILE (element);
	GtkWidget *widget;

	widget = gtk_file_chooser_button_new (
		_("Choose a File"), GTK_FILE_CHOOSER_ACTION_OPEN);
	gtk_file_chooser_set_filename (
		GTK_FILE_CHOOSER (widget), file->path);
	g_signal_connect (
		widget, "selection-changed",
		G_CALLBACK (filter_file_filename_changed), element);

	return widget;
}

static void
filter_file_format_sexp (EFilterElement *element,
                         GString *out)
{
	EFilterFile *file = E_FILTER_FILE (element);

	camel_sexp_encode_string (out, file->path);
}

static void
filter_file_describe (EFilterElement *element,
		      GString *out)
{
	EFilterFile *file = E_FILTER_FILE (element);

	g_string_append_c (out, E_FILTER_ELEMENT_DESCRIPTION_VALUE_START);
	g_string_append (out, file->path);
	g_string_append_c (out, E_FILTER_ELEMENT_DESCRIPTION_VALUE_END);
}

static void
e_filter_file_class_init (EFilterFileClass *class)
{
	GObjectClass *object_class;
	EFilterElementClass *filter_element_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = filter_file_finalize;

	filter_element_class = E_FILTER_ELEMENT_CLASS (class);
	filter_element_class->validate = filter_file_validate;
	filter_element_class->eq = filter_file_eq;
	filter_element_class->xml_encode = filter_file_xml_encode;
	filter_element_class->xml_decode = filter_file_xml_decode;
	filter_element_class->get_widget = filter_file_get_widget;
	filter_element_class->format_sexp = filter_file_format_sexp;
	filter_element_class->describe = filter_file_describe;
}

static void
e_filter_file_init (EFilterFile *filter)
{
}

/**
 * filter_file_new:
 *
 * Create a new EFilterFile object.
 *
 * Return value: A new #EFilterFile object.
 **/
EFilterFile *
e_filter_file_new (void)
{
	return g_object_new (E_TYPE_FILTER_FILE, NULL);
}

EFilterFile *
e_filter_file_new_type_name (const gchar *type)
{
	EFilterFile *file;

	file = e_filter_file_new ();
	file->type = (gchar *) xmlStrdup ((xmlChar *) type);

	return file;
}

void
e_filter_file_set_path (EFilterFile *file,
                        const gchar *path)
{
	g_return_if_fail (E_IS_FILTER_FILE (file));

	g_free (file->path);
	file->path = g_strdup (path);
}
