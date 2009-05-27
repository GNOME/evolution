/*
 *
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

#ifndef _EM_FILTER_EDITOR_H
#define _EM_FILTER_EDITOR_H

#include "filter/rule-editor.h"
#include "em-filter-context.h"

#define EM_FILTER_EDITOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), em_filter_editor_get_type(), EMFilterEditor))
#define EM_FILTER_EDITOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), em_filter_editor_get_type(), EMFilterEditorClass))
#define EM_IS_FILTER_EDITOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), em_filter_editor_get_type()))
#define EM_IS_FILTER_EDITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), em_filter_editor_get_type()))
#define EM_FILTER_EDITOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), em_filter_editor_get_type(), EMFilterEditorClass))

typedef struct _EMFilterEditor EMFilterEditor;
typedef struct _EMFilterEditorClass EMFilterEditorClass;

typedef struct _EMFilterSource EMFilterSource;

struct _EMFilterSource {
	const gchar *source;
	const gchar *name;
};

struct _EMFilterEditor {
	RuleEditor parent_object;
};

struct _EMFilterEditorClass {
	RuleEditorClass parent_class;
};

GType em_filter_editor_get_type (void);

EMFilterEditor *em_filter_editor_new (EMFilterContext *f, const EMFilterSource *source_names);
void em_filter_editor_construct (EMFilterEditor *fe, EMFilterContext *fc, GladeXML *gui, const EMFilterSource *source_names);

#endif /* ! _EM_FILTER_EDITOR_H */
