/* Evolution calendar - Main page of the task editor dialog
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Federico Mena-Quintero <federico@ximian.com>
 *          Miguel de Icaza <miguel@ximian.com>
 *          Seth Alves <alves@hungry.com>
 *          JP Rosevear <jpr@ximian.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#ifndef TASK_DETAILS_PAGE_H
#define TASK_DETAILS_PAGE_H

#include "comp-editor-page.h"

BEGIN_GNOME_DECLS



#define TYPE_TASK_DETAILS_PAGE            (task_details_page_get_type ())
#define TASK_DETAILS_PAGE(obj)            (GTK_CHECK_CAST ((obj), TYPE_TASK_DETAILS_PAGE, TaskDetailsPage))
#define TASK_DETAILS_PAGE_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), TYPE_TASK_DETAILS_PAGE, TaskDetailsPageClass))
#define IS_TASK_DETAILS_PAGE(obj)         (GTK_CHECK_TYPE ((obj), TYPE_TASK_DETAILS_PAGE))
#define IS_TASK_DETAILS_PAGE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((obj), TYPE_TASK_DETAILS_PAGE))

typedef struct _TaskDetailsPagePrivate TaskDetailsPagePrivate;

typedef struct {
	CompEditorPage page;

	/* Private data */
	TaskDetailsPagePrivate *priv;
} TaskDetailsPage;

typedef struct {
	CompEditorPageClass parent_class;
} TaskDetailsPageClass;


GtkType          task_details_page_get_type  (void);
TaskDetailsPage *task_details_page_construct (TaskDetailsPage *epage);
TaskDetailsPage *task_details_page_new       (void);



END_GNOME_DECLS

#endif
