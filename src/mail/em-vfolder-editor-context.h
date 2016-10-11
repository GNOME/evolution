/*
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Not Zed <notzed@lostzed.mmc.com.au>
 *      Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef EM_VFOLDER_EDITOR_CONTEXT_H
#define EM_VFOLDER_EDITOR_CONTEXT_H

#include <e-util/e-util.h>
#include <libemail-engine/libemail-engine.h>

/* Standard GObject macros */
#define EM_TYPE_VFOLDER_EDITOR_CONTEXT \
	(em_vfolder_editor_context_get_type ())
#define EM_VFOLDER_EDITOR_CONTEXT(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), EM_TYPE_VFOLDER_EDITOR_CONTEXT, EMVFolderEditorContext))
#define EM_VFOLDER_EDITOR_CONTEXT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), EM_TYPE_VFOLDER_EDITOR_CONTEXT, EMVFolderEditorContextClass))
#define EM_IS_VFOLDER_EDITOR_CONTEXT(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), EM_TYPE_VFOLDER_EDITOR_CONTEXT))
#define EM_IS_VFOLDER_EDITOR_CONTEXT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), EM_TYPE_VFOLDER_EDITOR_CONTEXT))
#define EM_VFOLDER_EDITOR_CONTEXT_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), EM_TYPE_VFOLDER_EDITOR_CONTEXT, EMVFolderEditorContextClass))

G_BEGIN_DECLS

typedef struct _EMVFolderEditorContext EMVFolderEditorContext;
typedef struct _EMVFolderEditorContextClass EMVFolderEditorContextClass;
typedef struct _EMVFolderEditorContextPrivate EMVFolderEditorContextPrivate;

struct _EMVFolderEditorContext {
	EMVFolderContext parent;
	EMVFolderEditorContextPrivate *priv;
};

struct _EMVFolderEditorContextClass {
	EMVFolderContextClass parent_class;
};

GType		em_vfolder_editor_context_get_type	(void);
EMVFolderEditorContext *
		em_vfolder_editor_context_new		(EMailSession *session);
EMailSession *	em_vfolder_editor_context_get_session	(EMVFolderEditorContext *context);

G_END_DECLS

#endif /* EM_VFOLDER_EDITOR_CONTEXT_H */
