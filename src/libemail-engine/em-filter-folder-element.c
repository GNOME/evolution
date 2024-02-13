/*
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

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "e-mail-folder-utils.h"
#include "em-filter-folder-element.h"

struct _EMFilterFolderElementPrivate {
	gchar *uri;
};

G_DEFINE_TYPE_WITH_PRIVATE (EMFilterFolderElement, em_filter_folder_element, E_TYPE_FILTER_ELEMENT)

static void
filter_folder_element_finalize (GObject *object)
{
	EMFilterFolderElement *self = EM_FILTER_FOLDER_ELEMENT (object);

	g_free (self->priv->uri);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (em_filter_folder_element_parent_class)->finalize (object);
}

static gboolean
filter_folder_element_validate (EFilterElement *fe,
                                EAlert **alert)
{
	EMFilterFolderElement *ff = (EMFilterFolderElement *) fe;

	g_warn_if_fail (alert == NULL || *alert == NULL);

	if (ff->priv->uri != NULL && *ff->priv->uri != '\0')
		return TRUE;

	if (alert)
		*alert = e_alert_new ("mail:no-folder", NULL);

	return FALSE;
}

static gint
filter_folder_element_eq (EFilterElement *fe,
                          EFilterElement *cm)
{
	return E_FILTER_ELEMENT_CLASS (
		em_filter_folder_element_parent_class)->eq (fe, cm) &&
		strcmp (((EMFilterFolderElement *) fe)->priv->uri,
		((EMFilterFolderElement *) cm)->priv->uri)== 0;
}

static xmlNodePtr
filter_folder_element_xml_encode (EFilterElement *fe)
{
	xmlNodePtr value, work;
	EMFilterFolderElement *ff = (EMFilterFolderElement *) fe;

	value = xmlNewNode (NULL, (xmlChar *) "value");
	xmlSetProp (value, (xmlChar *) "name", (xmlChar *) fe->name);
	xmlSetProp (value, (xmlChar *) "type", (xmlChar *) "folder");

	work = xmlNewChild (value, NULL, (xmlChar *) "folder", NULL);
	xmlSetProp (work, (xmlChar *) "uri", (xmlChar *) ff->priv->uri);

	return value;
}

static gint
filter_folder_element_xml_decode (EFilterElement *fe,
                                  xmlNodePtr node)
{
	EMFilterFolderElement *ff = (EMFilterFolderElement *) fe;
	xmlNodePtr n;

	xmlFree (fe->name);
	fe->name = (gchar *) xmlGetProp (node, (xmlChar *) "name");

	n = node->children;
	while (n) {
		if (!strcmp ((gchar *) n->name, "folder")) {
			gchar *uri;

			uri = (gchar *) xmlGetProp (n, (xmlChar *) "uri");
			g_free (ff->priv->uri);
			ff->priv->uri = g_strdup (uri);
			xmlFree (uri);
			break;
		}
		n = n->next;
	}

	return 0;
}

static GtkWidget *
filter_folder_element_get_widget (EFilterElement *fe)
{
	GtkWidget *widget;

	widget = E_FILTER_ELEMENT_CLASS (em_filter_folder_element_parent_class)->
		get_widget (fe);

	return widget;
}

static void
filter_folder_element_build_code (EFilterElement *fe,
                                  GString *out,
                                  EFilterPart *ff)
{
	/* We are doing nothing on purpose. */
}

static void
filter_folder_element_format_sexp (EFilterElement *fe,
                                   GString *out)
{
	EMFilterFolderElement *ff = (EMFilterFolderElement *) fe;

	camel_sexp_encode_string (out, ff->priv->uri);
}

static void
filter_folder_element_copy_value (EFilterElement *de,
                                  EFilterElement *se)
{
	if (EM_IS_FILTER_FOLDER_ELEMENT (se)) {
		em_filter_folder_element_set_uri (
			EM_FILTER_FOLDER_ELEMENT (de),
			EM_FILTER_FOLDER_ELEMENT (se)->priv->uri);
	} else {
		E_FILTER_ELEMENT_CLASS (
		em_filter_folder_element_parent_class)->copy_value (de, se);
	}
}

static void
filter_folder_element_describe (EFilterElement *fe,
				GString *out)
{
	EMFilterFolderElement *ff = (EMFilterFolderElement *) fe;

	if (!ff->priv->uri)
		return;

	/* This might not be usually used, do some special processing
	   for it and call em_filter_folder_element_describe() instead */
	g_string_append (out, ff->priv->uri);
}

static void
em_filter_folder_element_class_init (EMFilterFolderElementClass *class)
{
	GObjectClass *object_class;
	EFilterElementClass *filter_element_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = filter_folder_element_finalize;

	filter_element_class = E_FILTER_ELEMENT_CLASS (class);
	filter_element_class->validate = filter_folder_element_validate;
	filter_element_class->eq = filter_folder_element_eq;
	filter_element_class->xml_encode = filter_folder_element_xml_encode;
	filter_element_class->xml_decode = filter_folder_element_xml_decode;
	filter_element_class->get_widget = filter_folder_element_get_widget;
	filter_element_class->build_code = filter_folder_element_build_code;
	filter_element_class->format_sexp = filter_folder_element_format_sexp;
	filter_element_class->copy_value = filter_folder_element_copy_value;
	filter_element_class->describe = filter_folder_element_describe;
}

static void
em_filter_folder_element_init (EMFilterFolderElement *element)
{
	element->priv = em_filter_folder_element_get_instance_private (element);
}

EFilterElement *
em_filter_folder_element_new (void)
{
	return g_object_new (
		EM_TYPE_FILTER_FOLDER_ELEMENT,
		NULL);
}

const gchar *
em_filter_folder_element_get_uri (EMFilterFolderElement *element)
{
	g_return_val_if_fail (EM_IS_FILTER_FOLDER_ELEMENT (element), NULL);

	return element->priv->uri;
}

void
em_filter_folder_element_set_uri (EMFilterFolderElement *element,
                                  const gchar *uri)
{
	g_return_if_fail (EM_IS_FILTER_FOLDER_ELEMENT (element));

	g_free (element->priv->uri);
	element->priv->uri = g_strdup (uri);
}

void
em_filter_folder_element_describe (EMFilterFolderElement *element,
				   CamelSession *session,
				   GString *out)
{
	g_return_if_fail (EM_IS_FILTER_FOLDER_ELEMENT (element));
	g_return_if_fail (CAMEL_IS_SESSION (session));
	g_return_if_fail (out != NULL);

	if (element->priv->uri) {
		gchar *full_name = NULL;
		const gchar *use_name = element->priv->uri;
		CamelStore *store = NULL;
		gchar *folder_name = NULL;

		if (e_mail_folder_uri_parse (session, element->priv->uri, &store, &folder_name, NULL)) {
			CamelFolder *folder;

			folder = camel_store_get_folder_sync (store, folder_name, 0, NULL, NULL);
			if (folder) {
				const gchar *service_display_name;

				service_display_name = camel_service_get_display_name (CAMEL_SERVICE (store));

				if (CAMEL_IS_VEE_FOLDER (folder) && (
				    g_strcmp0 (folder_name, CAMEL_VTRASH_NAME) == 0 ||
				    g_strcmp0 (folder_name, CAMEL_VJUNK_NAME) == 0)) {
					full_name = g_strdup_printf ("%s/%s", service_display_name, camel_folder_get_display_name (folder));
				} else {
					full_name = g_strdup_printf ("%s/%s", service_display_name, folder_name);
				}

				g_clear_object (&folder);
			}

			if (!full_name)
				full_name = g_strdup_printf ("%s/%s", camel_service_get_display_name (CAMEL_SERVICE (store)), folder_name);

			if (full_name)
				use_name = full_name;

			g_clear_object (&store);
			g_free (folder_name);
		}

		g_string_append_c (out, E_FILTER_ELEMENT_DESCRIPTION_VALUE_START);
		g_string_append (out, use_name);
		g_string_append_c (out, E_FILTER_ELEMENT_DESCRIPTION_VALUE_END);

		g_free (full_name);
	}
}
