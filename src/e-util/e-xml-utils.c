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
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "e-xml-utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <locale.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>

#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

#include "e-misc-utils.h"

/* Returns the first child with the name child_name and the "lang"
 * attribute that matches the current LC_MESSAGES, or else, the first
 * child with the name child_name and no "lang" attribute.
 */
xmlNode *
e_xml_get_child_by_name_by_lang (const xmlNode *parent,
                                 const xmlChar *child_name,
                                 const gchar *lang)
{
#ifdef G_OS_WIN32
	gchar *freeme = NULL;
#endif
	xmlNode *child;
	/* This is the default version of the string. */
	xmlNode *C = NULL;

	g_return_val_if_fail (parent != NULL, NULL);
	g_return_val_if_fail (child_name != NULL, NULL);

	if (lang == NULL) {
#ifndef G_OS_WIN32
#ifdef HAVE_LC_MESSAGES
		lang = setlocale (LC_MESSAGES, NULL);
#else
		lang = setlocale (LC_CTYPE, NULL);
#endif
#else
		lang = freeme = g_win32_getlocale ();
#endif
	}
	for (child = parent->xmlChildrenNode; child != NULL; child = child->next) {
		if (xmlStrcmp (child->name, child_name) == 0) {
			xmlChar *this_lang = xmlGetProp (
				child, (const guchar *)"lang");
			if (this_lang == NULL) {
				C = child;
			} else if (xmlStrcmp (this_lang, (xmlChar *) lang) == 0) {
#ifdef G_OS_WIN32
				g_free (freeme);
#endif
				return child;
			}
		}
	}
#ifdef G_OS_WIN32
	g_free (freeme);
#endif
	return C;
}

static xmlNode *
e_xml_get_child_by_name_by_lang_list_with_score (const xmlNode *parent,
                                                 const gchar *name,
                                                 const GList *lang_list,
                                                 gint *best_lang_score)
{
	xmlNodePtr best_node = NULL, node;

	for (node = parent->xmlChildrenNode; node != NULL; node = node->next) {
		xmlChar *lang;

		if (node->name == NULL || strcmp ((gchar *) node->name, name) != 0) {
			continue;
		}
		lang = xmlGetProp (node, (const guchar *)"xml:lang");
		if (lang != NULL) {
			const GList *l;
			gint i;

			for (l = lang_list, i = 0;
			     l != NULL && i < *best_lang_score;
			     l = l->next, i++) {
				if (strcmp ((gchar *) l->data, (gchar *) lang) == 0) {
					best_node = node;
					*best_lang_score = i;
				}
			}
		} else {
			if (best_node == NULL) {
				best_node = node;
			}
		}
		xmlFree (lang);
		if (*best_lang_score == 0) {
			return best_node;
		}
	}

	return best_node;
}

xmlNode *
e_xml_get_child_by_name_by_lang_list (const xmlNode *parent,
                                      const gchar *name,
                                      const GList *lang_list)
{
	gint best_lang_score = INT_MAX;

	g_return_val_if_fail (parent != NULL, NULL);
	g_return_val_if_fail (name != NULL, NULL);

	if (lang_list == NULL) {
		const gchar * const *language_names;

		language_names = g_get_language_names ();
		while (*language_names != NULL)
			lang_list = g_list_append (
				(GList *) lang_list, (gchar *) * language_names++);
	}
	return e_xml_get_child_by_name_by_lang_list_with_score
		(parent,name,
		 lang_list,
		 &best_lang_score);
}

xmlNode *
e_xml_get_child_by_name_no_lang (const xmlNode *parent,
                                 const gchar *name)
{
	xmlNodePtr node;

	g_return_val_if_fail (parent != NULL, NULL);
	g_return_val_if_fail (name != NULL, NULL);

	for (node = parent->xmlChildrenNode; node != NULL; node = node->next) {
		xmlChar *lang;

		if (node->name == NULL || strcmp ((gchar *) node->name, name) != 0) {
			continue;
		}
		lang = xmlGetProp (node, (const guchar *)"xml:lang");
		if (lang == NULL) {
			return node;
		}
		xmlFree (lang);
	}

	return NULL;
}

gint
e_xml_get_integer_prop_by_name (const xmlNode *parent,
                                const xmlChar *prop_name)
{
	g_return_val_if_fail (parent != NULL, 0);
	g_return_val_if_fail (prop_name != NULL, 0);

	return e_xml_get_integer_prop_by_name_with_default (parent, prop_name, 0);
}

gint
e_xml_get_integer_prop_by_name_with_default (const xmlNode *parent,
                                             const xmlChar *prop_name,
                                             gint def)
{
	xmlChar *prop;
	gint ret_val = def;

	g_return_val_if_fail (parent != NULL, 0);
	g_return_val_if_fail (prop_name != NULL, 0);

	prop = xmlGetProp ((xmlNode *) parent, prop_name);
	if (prop != NULL) {
		(void) sscanf ((gchar *) prop, "%d", &ret_val);
		xmlFree (prop);
	}
	return ret_val;
}

void
e_xml_set_integer_prop_by_name (xmlNode *parent,
                                const xmlChar *prop_name,
                                gint value)
{
	gchar *valuestr;

	g_return_if_fail (parent != NULL);
	g_return_if_fail (prop_name != NULL);

	valuestr = g_strdup_printf ("%d", value);
	xmlSetProp (parent, prop_name, (guchar *) valuestr);
	g_free (valuestr);
}

guint
e_xml_get_uint_prop_by_name (const xmlNode *parent,
                             const xmlChar *prop_name)
{
	g_return_val_if_fail (parent != NULL, 0);
	g_return_val_if_fail (prop_name != NULL, 0);

	return e_xml_get_uint_prop_by_name_with_default (parent, prop_name, 0);
}

guint
e_xml_get_uint_prop_by_name_with_default (const xmlNode *parent,
                                          const xmlChar *prop_name,
                                          guint def)
{
	xmlChar *prop;
	guint ret_val = def;

	g_return_val_if_fail (parent != NULL, 0);
	g_return_val_if_fail (prop_name != NULL, 0);

	prop = xmlGetProp ((xmlNode *) parent, prop_name);
	if (prop != NULL) {
		(void) sscanf ((gchar *) prop, "%u", &ret_val);
		xmlFree (prop);
	}
	return ret_val;
}

void
e_xml_set_uint_prop_by_name (xmlNode *parent,
                             const xmlChar *prop_name,
                             guint value)
{
	gchar *valuestr;

	g_return_if_fail (parent != NULL);
	g_return_if_fail (prop_name != NULL);

	valuestr = g_strdup_printf ("%u", value);
	xmlSetProp (parent, prop_name, (guchar *) valuestr);
	g_free (valuestr);
}

gboolean
e_xml_get_bool_prop_by_name (const xmlNode *parent,
                             const xmlChar *prop_name)
{
	g_return_val_if_fail (parent != NULL, 0);
	g_return_val_if_fail (prop_name != NULL, 0);

	return e_xml_get_bool_prop_by_name_with_default (
		parent, prop_name, FALSE);
}

gboolean
e_xml_get_bool_prop_by_name_with_default (const xmlNode *parent,
                                          const xmlChar *prop_name,
                                          gboolean def)
{
	xmlChar *prop;
	gboolean ret_val = def;

	g_return_val_if_fail (parent != NULL, 0);
	g_return_val_if_fail (prop_name != NULL, 0);

	prop = xmlGetProp ((xmlNode *) parent, prop_name);
	if (prop != NULL) {
		if (g_ascii_strcasecmp ((gchar *) prop, "true") == 0) {
			ret_val = TRUE;
		} else if (g_ascii_strcasecmp ((gchar *) prop, "false") == 0) {
			ret_val = FALSE;
		}
		xmlFree (prop);
	}
	return ret_val;
}

void
e_xml_set_bool_prop_by_name (xmlNode *parent,
                             const xmlChar *prop_name,
                             gboolean value)
{
	g_return_if_fail (parent != NULL);
	g_return_if_fail (prop_name != NULL);

	if (value) {
		xmlSetProp (parent, prop_name, (const guchar *)"true");
	} else {
		xmlSetProp (parent, prop_name, (const guchar *)"false");
	}
}

gdouble
e_xml_get_double_prop_by_name (const xmlNode *parent,
                               const xmlChar *prop_name)
{
	g_return_val_if_fail (parent != NULL, 0);
	g_return_val_if_fail (prop_name != NULL, 0);

	return e_xml_get_double_prop_by_name_with_default (parent, prop_name, 0.0);
}

gdouble
e_xml_get_double_prop_by_name_with_default (const xmlNode *parent,
                                            const xmlChar *prop_name,
                                            gdouble def)
{
	xmlChar *prop;
	gdouble ret_val = def;

	g_return_val_if_fail (parent != NULL, 0);
	g_return_val_if_fail (prop_name != NULL, 0);

	prop = xmlGetProp ((xmlNode *) parent, prop_name);
	if (prop != NULL) {
		ret_val = e_flexible_strtod ((gchar *) prop, NULL);
		xmlFree (prop);
	}
	return ret_val;
}

void
e_xml_set_double_prop_by_name (xmlNode *parent,
                               const xmlChar *prop_name,
                               gdouble value)
{
	gchar buffer[E_ASCII_DTOSTR_BUF_SIZE];
	gchar *format;

	g_return_if_fail (parent != NULL);
	g_return_if_fail (prop_name != NULL);

	if (fabs (value) < 1e9 && fabs (value) > 1e-5) {
		format = g_strdup_printf ("%%.%df", DBL_DIG);
	} else {
		format = g_strdup_printf ("%%.%dg", DBL_DIG);
	}
	e_ascii_dtostr (buffer, sizeof (buffer), format, value);
	g_free (format);

	xmlSetProp (parent, prop_name, (const guchar *) buffer);
}

gchar *
e_xml_get_string_prop_by_name (const xmlNode *parent,
                               const xmlChar *prop_name)
{
	g_return_val_if_fail (parent != NULL, NULL);
	g_return_val_if_fail (prop_name != NULL, NULL);

	return e_xml_get_string_prop_by_name_with_default (parent, prop_name, NULL);
}

gchar *
e_xml_get_string_prop_by_name_with_default (const xmlNode *parent,
                                            const xmlChar *prop_name,
                                            const gchar *def)
{
	xmlChar *prop;
	gchar *ret_val;

	g_return_val_if_fail (parent != NULL, NULL);
	g_return_val_if_fail (prop_name != NULL, NULL);

	prop = xmlGetProp ((xmlNode *) parent, prop_name);
	if (prop != NULL) {
		ret_val = g_strdup ((gchar *) prop);
		xmlFree (prop);
	} else {
		ret_val = g_strdup (def);
	}
	return ret_val;
}

void
e_xml_set_string_prop_by_name (xmlNode *parent,
                               const xmlChar *prop_name,
                               const gchar *value)
{
	g_return_if_fail (parent != NULL);
	g_return_if_fail (prop_name != NULL);

	if (value != NULL) {
		xmlSetProp (parent, prop_name, (guchar *) value);
	}
}

gchar *
e_xml_get_translated_string_prop_by_name (const xmlNode *parent,
                                          const xmlChar *prop_name)
{
	xmlChar *prop;
	gchar *ret_val = NULL;
	gchar *combined_name;

	g_return_val_if_fail (parent != NULL, NULL);
	g_return_val_if_fail (prop_name != NULL, NULL);

	prop = xmlGetProp ((xmlNode *) parent, prop_name);
	if (prop != NULL) {
		ret_val = g_strdup ((gchar *) prop);
		xmlFree (prop);
		return ret_val;
	}

	combined_name = g_strdup_printf ("_%s", prop_name);
	prop = xmlGetProp ((xmlNode *) parent, (guchar *) combined_name);
	if (prop != NULL) {
		ret_val = g_strdup (gettext ((gchar *) prop));
		xmlFree (prop);
	}
	g_free (combined_name);

	return ret_val;
}

