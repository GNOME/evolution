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

#ifndef _EM_FILTER_EDITOR_H
#define _EM_FILTER_EDITOR_H

#include "filter/rule-editor.h"
#include "em-filter-context.h"

#define EM_FILTER_EDITOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), em_filter_editor_get_type(), EMFilterEditor))
#define EM_FILTER_EDITOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), em_filter_editor_get_type(), EMFilterEditorClass))
#define EM_IS_FILTER_EDITOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), em_filter_editor_get_type()))
#define EM_IS_FILTER_EDITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), em_filter_editor_get_type()))
#define EM_FILTER_EDITOR_GET_CLASS(obj)  (GTK_CHECK_GET_CLASS ((obj), em_filter_editor_get_type(), EMFilterEditorClass))

typedef struct _EMFilterEditor EMFilterEditor;
typedef struct _EMFilterEditorClass EMFilterEditorClass;

struct _EMFilterEditor {
	RuleEditor parent_object;
	
};

struct _EMFilterEditorClass {
	RuleEditorClass parent_class;
	
	/* virtual methods */
	
	/* signals */
};

GtkType em_filter_editor_get_type (void);

EMFilterEditor *em_filter_editor_new (EMFilterContext *f, const char **source_names);
void em_filter_editor_construct (EMFilterEditor *fe, EMFilterContext *fc, GladeXML *gui, const char **source_names);

#endif /* ! _EM_FILTER_EDITOR_H */
