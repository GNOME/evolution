/*
 *  Copyright (C) 2000 Helix Code Inc.
 *
 *  Authors: Not Zed <notzed@lostzed.mmc.com.au>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License
 *  as published by the Free Software Foundation; either version 2 of
 *  the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef _VFOLDER_EDITOR_H
#define _VFOLDER_EDITOR_H

#include <gtk/gtk.h>
#include <libgnomeui/gnome-dialog.h>

#if 0
/* NOTE: object stuff not used (yet?), this is just a holder file for a static factory */

#define VFOLDER_EDITOR(obj)	GTK_CHECK_CAST (obj, vfolder_editor_get_type (), VfolderEditor)
#define VFOLDER_EDITOR_CLASS(klass)	GTK_CHECK_CLASS_CAST (klass, vfolder_editor_get_type (), VfolderEditorClass)
#define IS_VFOLDER_EDITOR(obj)      GTK_CHECK_TYPE (obj, vfolder_editor_get_type ())

typedef struct _VfolderEditor	VfolderEditor;
typedef struct _VfolderEditorClass	VfolderEditorClass;

struct _VfolderEditor {
	GnomeDialog parent;
	struct _VfolderEditorPrivate *priv;

};

struct _VfolderEditorClass {
	GnomeDialogClass parent_class;

	/* virtual methods */

	/* signals */
};

guint		vfolder_editor_get_type	(void);
VfolderEditor	*vfolder_editor_new	(void);
#endif

struct _VfolderContext;

/* methods */
GtkWidget	*vfolder_editor_construct	(struct _VfolderContext *f);

#endif /* ! _VFOLDER_EDITOR_H */

