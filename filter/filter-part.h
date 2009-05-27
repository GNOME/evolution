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

#ifndef _FILTER_PART_H
#define _FILTER_PART_H

#include <glib.h>
#include <glib-object.h>

#include "filter-input.h"

struct _RuleContext;

#define FILTER_TYPE_PART            (filter_part_get_type ())
#define FILTER_PART(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FILTER_TYPE_PART, FilterPart))
#define FILTER_PART_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FILTER_TYPE_PART, FilterPartClass))
#define IS_FILTER_PART(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FILTER_TYPE_PART))
#define IS_FILTER_PART_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FILTER_TYPE_PART))
#define FILTER_PART_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FILTER_TYPE_PART, FilterElementClass))

typedef struct _FilterPart FilterPart;
typedef struct _FilterPartClass FilterPartClass;

struct _FilterPart {
	GObject parent_object;
	struct _FilterPartPrivate *priv;

	gchar *name;
	gchar *title;
	gchar *code;
	GList *elements;
};

struct _FilterPartClass {
	GObjectClass parent_class;

	/* virtual methods */

	/* signals */
};

GType           filter_part_get_type     (void);
FilterPart     *filter_part_new          (void);

/* methods */
gboolean        filter_part_validate     (FilterPart *fp);
gint             filter_part_eq           (FilterPart *fp, FilterPart *fc);

gint             filter_part_xml_create   (FilterPart *ff, xmlNodePtr node, struct _RuleContext *rc);

xmlNodePtr      filter_part_xml_encode   (FilterPart *fe);
gint             filter_part_xml_decode   (FilterPart *fe, xmlNodePtr node);

FilterPart     *filter_part_clone        (FilterPart *fp);
void            filter_part_copy_values  (FilterPart *dfp, FilterPart *sfp);

FilterElement  *filter_part_find_element (FilterPart *ff, const gchar *name);

GtkWidget      *filter_part_get_widget   (FilterPart *ff);
void		filter_part_build_code   (FilterPart *ff, GString *out);
void		filter_part_expand_code  (FilterPart *ff, const gchar *str, GString *out);

/* static functions */
void            filter_part_build_code_list (GList *l, GString *out);
FilterPart     *filter_part_find_list    (GList *l, const gchar *name);
FilterPart     *filter_part_next_list    (GList *l, FilterPart *last);

#endif /* ! _FILTER_PART_H */
