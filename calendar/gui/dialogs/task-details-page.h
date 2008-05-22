/* Evolution calendar - Main page of the task editor dialog
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Federico Mena-Quintero <federico@ximian.com>
 *          Miguel de Icaza <miguel@ximian.com>
 *          Seth Alves <alves@hungry.com>
 *          JP Rosevear <jpr@ximian.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef TASK_DETAILS_PAGE_H
#define TASK_DETAILS_PAGE_H

#include "comp-editor-page.h"

G_BEGIN_DECLS



#define TYPE_TASK_DETAILS_PAGE            (task_details_page_get_type ())
#define TASK_DETAILS_PAGE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_TASK_DETAILS_PAGE, TaskDetailsPage))
#define TASK_DETAILS_PAGE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_TASK_DETAILS_PAGE, TaskDetailsPageClass))
#define IS_TASK_DETAILS_PAGE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_TASK_DETAILS_PAGE))
#define IS_TASK_DETAILS_PAGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), TYPE_TASK_DETAILS_PAGE))

typedef struct _TaskDetailsPagePrivate TaskDetailsPagePrivate;

typedef struct {
	CompEditorPage page;

	/* Private data */
	TaskDetailsPagePrivate *priv;
} TaskDetailsPage;

typedef struct {
	CompEditorPageClass parent_class;
} TaskDetailsPageClass;


GType            task_details_page_get_type        (void);
TaskDetailsPage *task_details_page_construct       (TaskDetailsPage *tdpage);
TaskDetailsPage *task_details_page_new             (void);



G_END_DECLS

#endif
