/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Not Zed <notzed@lostzed.mmc.com.au>
 * SPDX-FileContributor: Jeffrey Stedfast <fejj@ximian.com>
 */

#ifndef EM_VFOLDER_EDITOR_H
#define EM_VFOLDER_EDITOR_H

#include <e-util/e-util.h>

#include "em-vfolder-editor-context.h"

/* Standard GObject macros */
#define EM_TYPE_VFOLDER_EDITOR \
	(em_vfolder_editor_get_type ())
#define EM_VFOLDER_EDITOR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), EM_TYPE_VFOLDER_EDITOR, EMVFolderEditor))
#define EM_VFOLDER_EDITOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), EM_TYPE_VFOLDER_EDITOR, EMVFolderEditorClass))
#define EM_IS_VFOLDER_EDITOR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), EM_TYPE_VFOLDER_EDITOR))
#define EM_IS_VFOLDER_EDITOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), EM_TYPE_VFOLDER_EDITOR))
#define EM_VFOLDER_EDITOR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), EM_TYPE_VFOLDER_EDITOR, EMVFolderEditorClass))

G_BEGIN_DECLS

typedef struct _EMVFolderEditor EMVFolderEditor;
typedef struct _EMVFolderEditorClass EMVFolderEditorClass;

struct _EMVFolderEditor {
	ERuleEditor parent;
};

struct _EMVFolderEditorClass {
	ERuleEditorClass parent_class;
};

GType		em_vfolder_editor_get_type	(void);
GtkWidget *	em_vfolder_editor_new		(EMVFolderContext *vc);

G_END_DECLS

#endif /* EM_VFOLDER_EDITOR_H */
