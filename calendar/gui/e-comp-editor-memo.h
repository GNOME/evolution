/*
 * Copyright (C) 2015 Red Hat, Inc. (www.redhat.com)
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef E_COMP_EDITOR_MEMO_H
#define E_COMP_EDITOR_MEMO_H

#include <calendar/gui/e-comp-editor.h>

/* Standard GObject macros */

#define E_TYPE_COMP_EDITOR_MEMO \
	(e_comp_editor_memo_get_type ())
#define E_COMP_EDITOR_MEMO(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_COMP_EDITOR_MEMO, ECompEditorMemo))
#define E_COMP_EDITOR_MEMO_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_COMP_EDITOR_MEMO, ECompEditorMemoClass))
#define E_IS_COMP_EDITOR_MEMO(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_COMP_EDITOR_MEMO))
#define E_IS_COMP_EDITOR_MEMO_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_COMP_EDITOR_MEMO))
#define E_COMP_EDITOR_MEMO_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_COMP_EDITOR_MEMO, ECompEditorMemoClass))

G_BEGIN_DECLS

typedef struct _ECompEditorMemo ECompEditorMemo;
typedef struct _ECompEditorMemoClass ECompEditorMemoClass;
typedef struct _ECompEditorMemoPrivate ECompEditorMemoPrivate;

struct _ECompEditorMemo {
	ECompEditor parent;

	ECompEditorMemoPrivate *priv;
};

struct _ECompEditorMemoClass {
	ECompEditorClass parent_class;
};

GType		e_comp_editor_memo_get_type	(void) G_GNUC_CONST;

G_END_DECLS

#endif /* E_COMP_EDITOR_MEMO_H */
