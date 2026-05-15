/*
 * SPDX-FileCopyrightText: (C) 2015 Red Hat, Inc. (www.redhat.com)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef E_COMP_EDITOR_PAGE_REMINDERS_H
#define E_COMP_EDITOR_PAGE_REMINDERS_H

#include <calendar/gui/e-comp-editor.h>
#include <calendar/gui/e-comp-editor-page.h>

/* Standard GObject macros */

#define E_TYPE_COMP_EDITOR_PAGE_REMINDERS \
	(e_comp_editor_page_reminders_get_type ())
#define E_COMP_EDITOR_PAGE_REMINDERS(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_COMP_EDITOR_PAGE_REMINDERS, ECompEditorPageReminders))
#define E_COMP_EDITOR_PAGE_REMINDERS_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_COMP_EDITOR_PAGE_REMINDERS, ECompEditorPageRemindersClass))
#define E_IS_COMP_EDITOR_PAGE_REMINDERS(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_COMP_EDITOR_PAGE_REMINDERS))
#define E_IS_COMP_EDITOR_PAGE_REMINDERS_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_COMP_EDITOR_PAGE_REMINDERS))
#define E_COMP_EDITOR_PAGE_REMINDERS_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_COMP_EDITOR_PAGE_REMINDERS, ECompEditorPageRemindersClass))

typedef struct _ECompEditorPageReminders ECompEditorPageReminders;
typedef struct _ECompEditorPageRemindersClass ECompEditorPageRemindersClass;
typedef struct _ECompEditorPageRemindersPrivate ECompEditorPageRemindersPrivate;

struct _ECompEditorPageReminders {
	ECompEditorPage parent;

	ECompEditorPageRemindersPrivate *priv;
};

struct _ECompEditorPageRemindersClass {
	ECompEditorPageClass parent_class;
};

GType		e_comp_editor_page_reminders_get_type	(void) G_GNUC_CONST;
ECompEditorPage *
		e_comp_editor_page_reminders_new	(ECompEditor *editor);

G_END_DECLS

#endif /* E_COMP_EDITOR_PAGE_REMINDERS_H */
