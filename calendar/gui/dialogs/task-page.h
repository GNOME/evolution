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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef TASK_PAGE_H
#define TASK_PAGE_H

#include <bonobo/bonobo-window.h>
#include <bonobo/bonobo-ui-util.h>
#include <bonobo/bonobo-widget.h>
 #include "comp-editor-page.h"
#include "../e-meeting-attendee.h"
#include "../e-meeting-store.h"
#include "../e-meeting-list-view.h"

G_BEGIN_DECLS



#define TYPE_TASK_PAGE            (task_page_get_type ())
#define TASK_PAGE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_TASK_PAGE, TaskPage))
#define TASK_PAGE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_TASK_PAGE, TaskPageClass))
#define IS_TASK_PAGE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_TASK_PAGE))
#define IS_TASK_PAGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), TYPE_TASK_PAGE))

typedef struct _TaskPagePrivate TaskPagePrivate;

typedef struct {
	CompEditorPage page;

	/* Private data */
	TaskPagePrivate *priv;
} TaskPage;

typedef struct {
	CompEditorPageClass parent_class;
} TaskPageClass;

GtkType   task_page_get_type  (void);
TaskPage *task_page_construct (TaskPage *epage, EMeetingStore *model, ECal *client);
TaskPage *task_page_new       (EMeetingStore *model, ECal *client, BonoboUIComponent *uic);
ECalComponent * task_page_get_cancel_comp (TaskPage *page);
void task_page_show_options (TaskPage *page);
void task_page_hide_options (TaskPage *page);
void task_page_set_assignment (TaskPage *page, gboolean set);
void task_page_sendoptions_clicked_cb (TaskPage *tpage);
void task_page_set_view_role (TaskPage *page, gboolean state);
void task_page_set_view_status (TaskPage *page, gboolean state);
void task_page_set_view_type (TaskPage *page, gboolean state);
void task_page_set_view_rsvp (TaskPage *page, gboolean state);
void task_page_set_classification (TaskPage *page, ECalComponentClassification class);
void task_page_set_show_timezone (TaskPage *page, gboolean state);
void task_page_set_show_categories (TaskPage *page, gboolean state);



G_END_DECLS

#endif
