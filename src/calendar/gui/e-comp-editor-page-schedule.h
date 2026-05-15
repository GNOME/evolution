/*
 * SPDX-FileCopyrightText: (C) 2015 Red Hat, Inc. (www.redhat.com)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef E_COMP_EDITOR_PAGE_SCHEDULE_H
#define E_COMP_EDITOR_PAGE_SCHEDULE_H

#include <calendar/gui/e-comp-editor.h>
#include <calendar/gui/e-comp-editor-page.h>
#include <calendar/gui/e-meeting-store.h>
#include <calendar/gui/e-meeting-time-sel.h>

/* Standard GObject macros */

#define E_TYPE_COMP_EDITOR_PAGE_SCHEDULE \
	(e_comp_editor_page_schedule_get_type ())
#define E_COMP_EDITOR_PAGE_SCHEDULE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_COMP_EDITOR_PAGE_SCHEDULE, ECompEditorPageSchedule))
#define E_COMP_EDITOR_PAGE_SCHEDULE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_COMP_EDITOR_PAGE_SCHEDULE, ECompEditorPageScheduleClass))
#define E_IS_COMP_EDITOR_PAGE_SCHEDULE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_COMP_EDITOR_PAGE_SCHEDULE))
#define E_IS_COMP_EDITOR_PAGE_SCHEDULE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_COMP_EDITOR_PAGE_SCHEDULE))
#define E_COMP_EDITOR_PAGE_SCHEDULE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_COMP_EDITOR_PAGE_SCHEDULE, ECompEditorPageScheduleClass))

typedef struct _ECompEditorPageSchedule ECompEditorPageSchedule;
typedef struct _ECompEditorPageScheduleClass ECompEditorPageScheduleClass;
typedef struct _ECompEditorPageSchedulePrivate ECompEditorPageSchedulePrivate;

struct _ECompEditorPageSchedule {
	ECompEditorPage parent;

	ECompEditorPageSchedulePrivate *priv;
};

struct _ECompEditorPageScheduleClass {
	ECompEditorPageClass parent_class;
};

GType		e_comp_editor_page_schedule_get_type	(void) G_GNUC_CONST;
ECompEditorPage *
		e_comp_editor_page_schedule_new		(ECompEditor *editor,
							 EMeetingStore *meeting_store,
							 ENameSelector *name_selector);
EMeetingStore *	e_comp_editor_page_schedule_get_store	(ECompEditorPageSchedule *page_schedule);
EMeetingTimeSelector *
		e_comp_editor_page_schedule_get_time_selector
							(ECompEditorPageSchedule *page_schedule);
ENameSelector *	e_comp_editor_page_schedule_get_name_selector
							(ECompEditorPageSchedule *page_schedule);

G_END_DECLS

#endif /* E_COMP_EDITOR_PAGE_SCHEDULE_H */
