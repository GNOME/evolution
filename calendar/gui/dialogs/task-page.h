/*
 *
 * Evolution calendar - Main page of the task editor dialog
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Federico Mena-Quintero <federico@ximian.com>
 *      Miguel de Icaza <miguel@ximian.com>
 *      Seth Alves <alves@hungry.com>
 *      JP Rosevear <jpr@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef TASK_PAGE_H
#define TASK_PAGE_H

#include "comp-editor.h"
#include "comp-editor-page.h"
#include "../e-meeting-attendee.h"
#include "../e-meeting-store.h"
#include "../e-meeting-list-view.h"

/* Standard GObject macros */
#define TYPE_TASK_PAGE \
	(task_page_get_type ())
#define TASK_PAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), TYPE_TASK_PAGE, TaskPage))
#define TASK_PAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), TYPE_TASK_PAGE, TaskPageClass))
#define IS_TASK_PAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), TYPE_TASK_PAGE))
#define IS_TASK_PAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((obj), TYPE_TASK_PAGE))
#define TASK_PAGE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), TYPE_TASK_PAGE, TaskPageClass))

G_BEGIN_DECLS

typedef struct _TaskPage TaskPage;
typedef struct _TaskPageClass TaskPageClass;
typedef struct _TaskPagePrivate TaskPagePrivate;

struct _TaskPage {
	CompEditorPage page;
	TaskPagePrivate *priv;
};

struct _TaskPageClass {
	CompEditorPageClass parent_class;
};

GType		task_page_get_type		(void);
TaskPage *	task_page_construct		(TaskPage *epage,
						 EMeetingStore *model,
						 ECal *client);
TaskPage *	task_page_new			(EMeetingStore *model,
						 CompEditor *editor);
ECalComponent *	task_page_get_cancel_comp	(TaskPage *page);
void		task_page_show_options		(TaskPage *page);
void		task_page_hide_options		(TaskPage *page);
void		task_page_set_assignment	(TaskPage *page,
						 gboolean set);
void		task_page_send_options_clicked_cb(TaskPage *tpage);
void		task_page_set_view_role		(TaskPage *page,
						 gboolean state);
void		task_page_set_view_status	(TaskPage *page,
						 gboolean state);
void		task_page_set_view_type		(TaskPage *page,
						 gboolean state);
void		task_page_set_view_rsvp		(TaskPage *page,
						 gboolean state);
void		task_page_set_show_timezone	(TaskPage *page,
						 gboolean state);
void		task_page_set_show_categories	(TaskPage *page,
						 gboolean state);
void		task_page_set_info_string	(TaskPage *tpage,
						 const gchar *icon,
						 const gchar *msg);
void		task_page_add_attendee		(TaskPage *tpage,
						 EMeetingAttendee *attendee);

G_END_DECLS

#endif
