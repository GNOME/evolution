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


#ifndef _FILTER_EDITOR_H
#define _FILTER_EDITOR_H

#include "rule-editor.h"
#include "filter-context.h"

#define FILTER_TYPE_EDITOR            (filter_editor_get_type ())
#define FILTER_EDITOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FILTER_TYPE_EDITOR, FilterEditor))
#define FILTER_EDITOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FILTER_TYPE_EDITOR, FilterEditorClass))
#define IS_FILTER_EDITOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FILTER_TYPE_EDITOR))
#define IS_FILTER_EDITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FILTER_TYPE_EDITOR))
#define FILTER_EDITOR_GET_CLASS(obj)  (GTK_CHECK_GET_CLASS ((obj), FILTER_TYPE_EDITOR, FilterEditorClass))

typedef struct _FilterEditor FilterEditor;
typedef struct _FilterEditorClass FilterEditorClass;

struct _FilterEditor {
	RuleEditor parent_object;
	
};

struct _FilterEditorClass {
	RuleEditorClass parent_class;
	
	/* virtual methods */
	
	/* signals */
};

GtkType filter_editor_get_type (void);

FilterEditor *filter_editor_new (FilterContext *f, const char **source_names);
void filter_editor_construct (FilterEditor *fe, FilterContext *fc, GladeXML *gui, const char **source_names);

#endif /* ! _FILTER_EDITOR_H */
