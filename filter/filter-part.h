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
	
	char *name;
	char *title;
	char *code;
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
int             filter_part_eq           (FilterPart *fp, FilterPart *fc);

int             filter_part_xml_create   (FilterPart *ff, xmlNodePtr node, struct _RuleContext *rc);

xmlNodePtr      filter_part_xml_encode   (FilterPart *fe);
int             filter_part_xml_decode   (FilterPart *fe, xmlNodePtr node);

FilterPart     *filter_part_clone        (FilterPart *fp);
void            filter_part_copy_values  (FilterPart *dfp, FilterPart *sfp);

FilterElement  *filter_part_find_element (FilterPart *ff, const char *name);

GtkWidget      *filter_part_get_widget   (FilterPart *ff);
void		filter_part_build_code   (FilterPart *ff, GString *out);
void		filter_part_expand_code  (FilterPart *ff, const char *str, GString *out);

/* static functions */
void            filter_part_build_code_list (GList *l, GString *out);
FilterPart     *filter_part_find_list    (GList *l, const char *name);
FilterPart     *filter_part_next_list    (GList *l, FilterPart *last);

#endif /* ! _FILTER_PART_H */
