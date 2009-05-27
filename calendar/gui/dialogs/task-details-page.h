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

#ifndef TASK_DETAILS_PAGE_H
#define TASK_DETAILS_PAGE_H

#include "comp-editor.h"
#include "comp-editor-page.h"

/* Standard GObject macros */
#define TYPE_TASK_DETAILS_PAGE \
	(task_details_page_get_type ())
#define TASK_DETAILS_PAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), TYPE_TASK_DETAILS_PAGE, TaskDetailsPage))
#define TASK_DETAILS_PAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), TYPE_TASK_DETAILS_PAGE, TaskDetailsPageClass))
#define IS_TASK_DETAILS_PAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), TYPE_TASK_DETAILS_PAGE))
#define IS_TASK_DETAILS_PAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((obj), TYPE_TASK_DETAILS_PAGE))
#define TASK_DETAILS_PAGE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), TYPE_TASK_DETAILS_PAGE, TaskDetailsPageClass))

G_BEGIN_DECLS

typedef struct _TaskDetailsPage TaskDetailsPage;
typedef struct _TaskDetailsPageClass TaskDetailsPageClass;
typedef struct _TaskDetailsPagePrivate TaskDetailsPagePrivate;

struct _TaskDetailsPage {
	CompEditorPage page;
	TaskDetailsPagePrivate *priv;
};

struct _TaskDetailsPageClass {
	CompEditorPageClass parent_class;
};

GType		 task_details_page_get_type	(void);
TaskDetailsPage *task_details_page_construct	(TaskDetailsPage *tdpage);
TaskDetailsPage *task_details_page_new		(CompEditor *editor);

G_END_DECLS

#endif /* TASK_DETAILS_PAGE_H */
