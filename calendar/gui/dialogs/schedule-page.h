/* Evolution calendar - Scheduling page
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * Authors: JP Rosevear <jpr@ximian.com>
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

#ifndef SCHEDULE_PAGE_H
#define SCHEDULE_PAGE_H

#include "../e-meeting-store.h"
#include "comp-editor-page.h"
#include <libedataserverui/e-name-selector.h>

G_BEGIN_DECLS



#define TYPE_SCHEDULE_PAGE            (schedule_page_get_type ())
#define SCHEDULE_PAGE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_SCHEDULE_PAGE, SchedulePage))
#define SCHEDULE_PAGE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_SCHEDULE_PAGE, SchedulePageClass))
#define IS_SCHEDULE_PAGE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_SCHEDULE_PAGE))
#define IS_SCHEDULE_PAGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), TYPE_SCHEDULE_PAGE))

typedef struct _SchedulePagePrivate SchedulePagePrivate;

typedef struct {
	CompEditorPage page;

	/* Private data */
	SchedulePagePrivate *priv;
} SchedulePage;

typedef struct {
	CompEditorPageClass parent_class;
} SchedulePageClass;


GType        schedule_page_get_type  (void);
SchedulePage *schedule_page_construct (SchedulePage *mpage, EMeetingStore *ems);
SchedulePage *schedule_page_new       (EMeetingStore *ems);
void schedule_page_set_name_selector (SchedulePage *spage, ENameSelector *name_selector);
void schedule_page_set_meeting_time (SchedulePage *spage, icaltimetype *start_tt, icaltimetype *end_tt);
void schedule_page_update_free_busy (SchedulePage *spage);


G_END_DECLS

#endif
