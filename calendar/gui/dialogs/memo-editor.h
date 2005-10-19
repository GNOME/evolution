/* Evolution calendar - Task editor dialog
 *
 * Copyright (C) 2000 Ximian, Inc.
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Miguel de Icaza <miguel@ximian.com>
 *          Federico Mena-Quintero <federico@ximian.com>
 *          Seth Alves <alves@hungry.com>
 *          Nathan Owens <pianocomp81@yahoo.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __MEMO_EDITOR_H__
#define __MEMO_EDITOR_H__

#include <gtk/gtkobject.h>
#include "comp-editor.h"

#define TYPE_MEMO_EDITOR            (memo_editor_get_type ())
#define MEMO_EDITOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_MEMO_EDITOR, MemoEditor))
#define MEMO_EDITOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_MEMO_EDITOR,	\
				      MemoEditorClass))
#define IS_MEMO_EDITOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_MEMO_EDITOR))
#define IS_MEMO_EDITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_MEMO_EDITOR))

typedef struct _MemoEditor MemoEditor;
typedef struct _MemoEditorClass MemoEditorClass;
typedef struct _MemoEditorPrivate MemoEditorPrivate;

struct _MemoEditor {
	CompEditor parent;

	/* Private data */
	MemoEditorPrivate *priv;
};

struct _MemoEditorClass {
	CompEditorClass parent_class;
};

GtkType     memo_editor_get_type       (void);
MemoEditor *memo_editor_construct      (MemoEditor *te,
					ECal  *client);
MemoEditor *memo_editor_new            (ECal  *client);


#endif /* __MEMO_EDITOR_H__ */
