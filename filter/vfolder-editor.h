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


#ifndef _VFOLDER_EDITOR_H
#define _VFOLDER_EDITOR_H

#include "rule-editor.h"
#include "vfolder-context.h"

#define VFOLDER_TYPE_EDITOR            (vfolder_editor_get_type ())
#define VFOLDER_EDITOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VFOLDER_TYPE_EDITOR, VfolderEditor))
#define VFOLDER_EDITOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), VFOLDER_TYPE_EDITOR, VfolderEditorClass))
#define IS_VFOLDER_EDITOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VFOLDER_TYPE_EDITOR))
#define IS_VFOLDER_EDITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VFOLDER_TYPE_EDITOR))
#define VFOLDER_EDITOR_GET_CLASS(obj)  (GTK_CHECK_GET_CLASS ((obj), VFOLDER_TYPE_EDITOR, VfolderEditorClass))

typedef struct _VfolderEditor VfolderEditor;
typedef struct _VfolderEditorClass VfolderEditorClass;

struct _VfolderEditor {
	RuleEditor parent_object;
	
};

struct _VfolderEditorClass {
	RuleEditorClass parent_class;
	
	/* virtual methods */
	
	/* signals */
};


GtkType vfolder_editor_get_type (void);

VfolderEditor *vfolder_editor_new (VfolderContext *vc);

#endif /* ! _VFOLDER_EDITOR_H */
