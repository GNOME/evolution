/*
 *
 * Evolution calendar - Task editor dialog
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
 *		Miguel de Icaza <miguel@ximian.com>
 *      Federico Mena-Quintero <federico@ximian.com>
 *      Seth Alves <alves@hungry.com>
 *      Nathan Owens <pianocomp81@yahoo.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __MEMO_EDITOR_H__
#define __MEMO_EDITOR_H__

#include <gtk/gtk.h>
#include "comp-editor.h"

/* Standard GObject macros */
#define TYPE_MEMO_EDITOR \
	(memo_editor_get_type ())
#define MEMO_EDITOR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), TYPE_MEMO_EDITOR, MemoEditor))
#define MEMO_EDITOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), TYPE_MEMO_EDITOR, MemoEditorClass))
#define IS_MEMO_EDITOR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), TYPE_MEMO_EDITOR))
#define IS_MEMO_EDITOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), TYPE_MEMO_EDITOR))
#define MEMO_EDITOR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), TYPE_MEMO_EDITOR, MemoEditorClass))

G_BEGIN_DECLS

typedef struct _MemoEditor MemoEditor;
typedef struct _MemoEditorClass MemoEditorClass;
typedef struct _MemoEditorPrivate MemoEditorPrivate;

struct _MemoEditor {
	CompEditor parent;
	MemoEditorPrivate *priv;
};

struct _MemoEditorClass {
	CompEditorClass parent_class;
};

GType		memo_editor_get_type		(void);
CompEditor *	memo_editor_new			(ECalClient *client,
						 EShell *shell,
						 CompEditorFlags flags);

G_END_DECLS

#endif /* __MEMO_EDITOR_H__ */
