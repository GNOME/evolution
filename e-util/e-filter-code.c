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

#include "e-filter-code.h"
#include "e-filter-part.h"

G_DEFINE_TYPE (
	EFilterCode,
	e_filter_code,
	E_TYPE_FILTER_INPUT)

/* here, the string IS the code */
static void
filter_code_build_code (EFilterElement *element,
                        GString *out,
                        EFilterPart *part)
{
	GList *l;
	EFilterInput *fi = (EFilterInput *) element;
	gboolean is_rawcode = fi->type && g_str_equal (fi->type, "rawcode");

	if (!is_rawcode)
		g_string_append (out, "(match-all ");

	l = fi->values;
	while (l) {
		g_string_append (out, (gchar *) l->data);
		l = g_list_next (l);
	}

	if (!is_rawcode)
		g_string_append (out, ")");
}

/* and we have no value */
static void
filter_code_format_sexp (EFilterElement *element,
                         GString *out)
{
}

static void
e_filter_code_class_init (EFilterCodeClass *class)
{
	EFilterElementClass *filter_element_class;

	filter_element_class = E_FILTER_ELEMENT_CLASS (class);
	filter_element_class->build_code = filter_code_build_code;
	filter_element_class->format_sexp = filter_code_format_sexp;
}

static void
e_filter_code_init (EFilterCode *code)
{
	EFilterInput *input = E_FILTER_INPUT (code);

	input->type = (gchar *) xmlStrdup ((xmlChar *) "code");
}

/**
 * filter_code_new:
 *
 * Create a new EFilterCode object.
 *
 * Return value: A new #EFilterCode object.
 **/
EFilterCode *
e_filter_code_new (gboolean raw_code)
{
	EFilterCode *fc = g_object_new (E_TYPE_FILTER_CODE, NULL, NULL);

	if (fc && raw_code) {
		xmlFree (((EFilterInput *) fc)->type);
		((EFilterInput *) fc)->type = (gchar *) xmlStrdup ((xmlChar *)"rawcode");
	}

	return fc;
}
