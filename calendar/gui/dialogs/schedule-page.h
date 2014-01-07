/*
 * Evolution calendar - Scheduling page
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		JP Rosevear <jpr@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef SCHEDULE_PAGE_H
#define SCHEDULE_PAGE_H

#include "../e-meeting-store.h"
#include "comp-editor.h"
#include "comp-editor-page.h"

/* Standard GObject macros */
#define TYPE_SCHEDULE_PAGE \
	(schedule_page_get_type ())
#define SCHEDULE_PAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), TYPE_SCHEDULE_PAGE, SchedulePage))
#define SCHEDULE_PAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), TYPE_SCHEDULE_PAGE, SchedulePageClass))
#define IS_SCHEDULE_PAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), TYPE_SCHEDULE_PAGE))
#define IS_SCHEDULE_PAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((obj), TYPE_SCHEDULE_PAGE))
#define SCHEDULE_PAGE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), TYPE_SCHEDULE_PAGE, SchedulePageClass))

G_BEGIN_DECLS

typedef struct _SchedulePage SchedulePage;
typedef struct _SchedulePageClass SchedulePageClass;
typedef struct _SchedulePagePrivate SchedulePagePrivate;

struct _SchedulePage {
	CompEditorPage page;
	SchedulePagePrivate *priv;
};

struct _SchedulePageClass {
	CompEditorPageClass parent_class;
};

GType		schedule_page_get_type		(void);
SchedulePage *	schedule_page_construct		(SchedulePage *mpage,
						 EMeetingStore *ems);
SchedulePage *	schedule_page_new		(EMeetingStore *ems,
						 CompEditor *editor);
void		schedule_page_set_name_selector	(SchedulePage *spage,
						 ENameSelector *name_selector);
void		schedule_page_set_meeting_time	(SchedulePage *spage,
						 icaltimetype *start_tt,
						 icaltimetype *end_tt);
void		schedule_page_update_free_busy	(SchedulePage *spage);

G_END_DECLS

#endif /* SCHEDULE_PAGE_H */
