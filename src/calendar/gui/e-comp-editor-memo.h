/*
 * SPDX-FileCopyrightText: (C) 2015 Red Hat, Inc. (www.redhat.com)
 * SPDX-License-Identifier: GPL-2.0-or-later
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
