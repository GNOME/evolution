/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-xml-utils.c
 * Copyright (C) 2000  Helix Code, Inc.
 * Author: Chris Lahey <clahey@helixcode.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <locale.h>
#include "gal/util/e-i18n.h"
#include <math.h>
#include <gnome-xml/parser.h>
#include <gnome-xml/xmlmemory.h>
#include "e-xml-utils.h"

xmlNode *e_xml_get_child_by_name(const xmlNode *parent, const xmlChar *child_name)
{
	xmlNode *child;

	g_return_val_if_fail(parent != NULL, NULL);
	g_return_val_if_fail(child_name != NULL, NULL);
	
	for (child = parent->childs; child; child = child->next) {
		if ( !xmlStrcmp( child->name, child_name ) ) {
			return child;
		}
	}
	return NULL;
}

/* Returns the first child with the name child_name and the "lang"
 * attribute that matches the current LC_MESSAGES, or else, the first
 * child with the name child_name and no "lang" attribute.
 */
xmlNode *
e_xml_get_child_by_name_by_lang(const xmlNode *parent, const xmlChar *child_name, const char *lang)
{
	xmlNode *child;
	/* This is the default version of the string. */
	xmlNode *C = NULL;

	g_return_val_if_fail(parent != NULL, NULL);
	g_return_val_if_fail(child_name != NULL, NULL);

	if (lang == NULL)
		lang = setlocale(LC_MESSAGES, NULL);

	for (child = parent->childs; child; child = child->next) {
		if ( !xmlStrcmp( child->name, child_name ) ) {
			char *this_lang = xmlGetProp(child, "lang");
			if ( this_lang == NULL ) {
				C = child;
			}
			else if (!strcmp(this_lang, "lang"))
				return child;
		}
	}
	return C;
}

int
e_xml_get_integer_prop_by_name(const xmlNode *parent, const xmlChar *prop_name)
{
	xmlChar *prop;
	int ret_val = 0;

	g_return_val_if_fail (parent != NULL, 0);
	g_return_val_if_fail (prop_name != NULL, 0);

	prop = xmlGetProp ((xmlNode *)parent, prop_name);
	if (prop) {
		ret_val = atoi (prop);
		xmlFree (prop);
	}
	return ret_val;
}

void
e_xml_set_integer_prop_by_name(xmlNode *parent, const xmlChar *prop_name, int value)
{
	xmlChar *valuestr;

	g_return_if_fail (parent != NULL);
	g_return_if_fail (prop_name != NULL);

	valuestr = g_strdup_printf ("%d", value);
	xmlSetProp (parent, prop_name, valuestr);
	g_free (valuestr);
}

gboolean
e_xml_get_bool_prop_by_name(const xmlNode *parent, const xmlChar *prop_name)
{
	xmlChar *prop;
	gboolean ret_val = FALSE;

	g_return_val_if_fail (parent != NULL, 0);
	g_return_val_if_fail (prop_name != NULL, 0);

	prop = xmlGetProp ((xmlNode *)parent, prop_name);
	if (prop) {
		if(!strcasecmp(prop, "true"))
			ret_val = TRUE;
		xmlFree(prop);
	}
	return ret_val;
}

void
e_xml_set_bool_prop_by_name(xmlNode *parent, const xmlChar *prop_name, gboolean value)
{
	g_return_if_fail (parent != NULL);
	g_return_if_fail (prop_name != NULL);

	if (value)
		xmlSetProp (parent, prop_name, "true");
	else
		xmlSetProp (parent, prop_name, "false");
}

double
e_xml_get_double_prop_by_name(const xmlNode *parent, const xmlChar *prop_name)
{
	xmlChar *prop;
	double ret_val = 0;

	g_return_val_if_fail (parent != NULL, 0);
	g_return_val_if_fail (prop_name != NULL, 0);

	prop = xmlGetProp ((xmlNode *)parent, prop_name);
	if (prop) {
		sscanf (prop, "%lf", &ret_val);
		xmlFree (prop);
	}
	return ret_val;
}

double
e_xml_get_double_prop_by_name_with_default(const xmlNode *parent, const xmlChar *prop_name,
					   gdouble def)
{
	xmlChar *prop;
	double ret_val = 0;

	g_return_val_if_fail (parent != NULL, 0);
	g_return_val_if_fail (prop_name != NULL, 0);

	prop = xmlGetProp ((xmlNode *)parent, prop_name);
	if (prop) {
		sscanf (prop, "%lf", &ret_val);
		xmlFree (prop);
	} else {
		ret_val = def;
	}
	return ret_val;
}

void
e_xml_set_double_prop_by_name(xmlNode *parent, const xmlChar *prop_name, double value)
{
	xmlChar *valuestr;

	g_return_if_fail (parent != NULL);
	g_return_if_fail (prop_name != NULL);

	if (fabs (value) < 1e9 && fabs (value) > 1e-5)
		valuestr = g_strdup_printf ("%f", value);
	else
		valuestr = g_strdup_printf ("%.*g", DBL_DIG, value);
	xmlSetProp (parent, prop_name, valuestr);
	g_free (valuestr);
}

char *
e_xml_get_string_prop_by_name(const xmlNode *parent, const xmlChar *prop_name)
{
	xmlChar *prop;
	char *ret_val = NULL;

	g_return_val_if_fail (parent != NULL, 0);
	g_return_val_if_fail (prop_name != NULL, 0);

	prop = xmlGetProp ((xmlNode *)parent, prop_name);
	if (prop) {
		ret_val = g_strdup (prop);
		xmlFree (prop);
	}
	return ret_val;
}

void
e_xml_set_string_prop_by_name(xmlNode *parent, const xmlChar *prop_name, const char *value)
{
	g_return_if_fail (parent != NULL);
	g_return_if_fail (prop_name != NULL);

	if (value)
		xmlSetProp (parent, prop_name, value);
}


char *
e_xml_get_translated_string_prop_by_name(const xmlNode *parent, const xmlChar *prop_name)
{
	xmlChar *prop;
	char *ret_val = NULL;

	g_return_val_if_fail (parent != NULL, 0);
	g_return_val_if_fail (prop_name != NULL, 0);

	prop = xmlGetProp ((xmlNode *)parent, prop_name);
	if (prop) {
		ret_val = g_strdup (_(prop));
		xmlFree (prop);
	}
	return ret_val;
}

