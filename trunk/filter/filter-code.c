/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright (C) 2000-2002 Ximian Inc.
 *
 *  Authors: Not Zed <notzed@lostzed.mmc.com.au>
 *           Jeffrey Stedfast <fejj@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
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
	((FilterInput *) fc)->type = xmlStrdup ("code");
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
filter_code_new (void)
{
	return (FilterCode *) g_object_new (FILTER_TYPE_CODE, NULL, NULL);
}

/* here, the string IS the code */
static void
build_code (FilterElement *fe, GString *out, struct _FilterPart *ff)
{
	GList *l;
	FilterInput *fi = (FilterInput *)fe;

	g_string_append(out, "(match-all ");
	l = fi->values;
	while (l) {
		g_string_append(out, (char *)l->data);
		l = g_list_next(l);
	}
	g_string_append(out, ")");
}

/* and we have no value */
static void
format_sexp (FilterElement *fe, GString *out)
{
	;
}
