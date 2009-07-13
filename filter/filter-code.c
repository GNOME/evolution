/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Not Zed <notzed@lostzed.mmc.com.au>
 *      Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "filter-code.h"

static void build_code (FilterElement *fe, GString *out, struct _FilterPart *ff);
static void format_sexp (FilterElement *, GString *);

static void filter_code_class_init (FilterCodeClass *class);
static void filter_code_init (FilterCode *fc);
static void filter_code_finalise (GObject *obj);

static FilterInputClass *parent_class;

GType
filter_code_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (FilterCodeClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) filter_code_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (FilterCode),
			0,    /* n_preallocs */
			(GInstanceInitFunc) filter_code_init,
		};

		type = g_type_register_static (FILTER_TYPE_INPUT, "FilterCode", &info, 0);
	}

	return type;
}

static void
filter_code_class_init (FilterCodeClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	FilterElementClass *fe_class = FILTER_ELEMENT_CLASS (klass);

	parent_class = g_type_class_ref (FILTER_TYPE_INPUT);

	object_class->finalize = filter_code_finalise;

	/* override methods */
	fe_class->build_code = build_code;
	fe_class->format_sexp = format_sexp;
}

static void
filter_code_init (FilterCode *fc)
{
	((FilterInput *) fc)->type = (gchar *)xmlStrdup ((const guchar *)"code");
}

static void
filter_code_finalise (GObject *obj)
{
        G_OBJECT_CLASS (parent_class)->finalize (obj);
}

/**
 * filter_code_new:
 *
 * Create a new FilterCode object.
 *
 * Return value: A new #FilterCode object.
 **/
FilterCode *
filter_code_new (gboolean raw_code)
{
	FilterCode *fc = (FilterCode *) g_object_new (FILTER_TYPE_CODE, NULL, NULL);

	if (fc && raw_code) {
		xmlFree (((FilterInput *) fc)->type);
		((FilterInput *) fc)->type = (gchar *)xmlStrdup ((const guchar *)"rawcode");
	}

	return fc;
}

/* here, the string IS the code */
static void
build_code (FilterElement *fe, GString *out, struct _FilterPart *ff)
{
	GList *l;
	FilterInput *fi = (FilterInput *)fe;
	gboolean is_rawcode = fi && fi->type && g_str_equal (fi->type, "rawcode");

	if (!is_rawcode)
		g_string_append(out, "(match-all ");

	l = fi->values;
	while (l) {
		g_string_append(out, (gchar *)l->data);
		l = g_list_next(l);
	}

	if (!is_rawcode)
		g_string_append (out, ")");
}

/* and we have no value */
static void
format_sexp (FilterElement *fe, GString *out)
{
	;
}
