/*
 * Evolution calendar - Main page of the event editor dialog
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
 *      JP Rosevear <jpr@ximian.com>*
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef EVENT_PAGE_H
#define EVENT_PAGE_H

#include "comp-editor.h"
#include "comp-editor-page.h"
#include "../e-meeting-attendee.h"
#include "../e-meeting-store.h"
#include "../e-meeting-list-view.h"

/* Standard GObject macros */
#define TYPE_EVENT_PAGE \
	(event_page_get_type ())
#define EVENT_PAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), TYPE_EVENT_PAGE, EventPage))
#define EVENT_PAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), TYPE_EVENT_PAGE, EventPageClass))
#define IS_EVENT_PAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), TYPE_EVENT_PAGE))
#define IS_EVENT_PAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((obj), TYPE_EVENT_PAGE))
#define EVENT_PAGE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), TYPE_EVENT_PAGE, EventPageClass))

G_BEGIN_DECLS

typedef struct _EventPage EventPage;
typedef struct _EventPageClass EventPageClass;
typedef struct _EventPagePrivate EventPagePrivate;

struct _EventPage {
	CompEditorPage page;
	EventPagePrivate *priv;
};

struct _EventPageClass {
	CompEditorPageClass parent_class;
};

GType		event_page_get_type		(void);
EventPage *	event_page_construct		(EventPage *epage,
						 EMeetingStore *model);
EventPage *	event_page_new			(EMeetingStore *model,
						 CompEditor *editor);
ECalComponent *	event_page_get_cancel_comp	(EventPage *page);
void		event_page_show_options		(EventPage *page);
void		event_page_hide_options		(EventPage *page);
void		event_page_send_options_clicked_cb
						(EventPage *epage);
void		event_page_set_meeting		(EventPage *page,
						 gboolean set);
void		event_page_set_show_timezone	(EventPage *epage,
						 gboolean state);
void		event_page_set_view_rsvp	(EventPage *epage,
						 gboolean state);
void		event_page_set_delegate		(EventPage *page,
						 gboolean set);
void		event_page_set_all_day_event	(EventPage *epage,
						 gboolean all_day);
void		event_page_set_show_categories	(EventPage *epage,
						 gboolean state);
void		event_page_set_show_time_busy	(EventPage *epage,
						 gboolean state);
void		event_page_show_alarm		(EventPage *epage);
void		event_page_set_info_string	(EventPage *epage,
						 const gchar *icon,
						 const gchar *msg);

void		event_page_set_view_role	(EventPage *epage,
						 gboolean state);
void		event_page_set_view_status	(EventPage *epage,
						 gboolean state);
void		event_page_set_view_type	(EventPage *epage,
						 gboolean state);
void		event_page_set_view_rvsp	(EventPage *epage,
						 gboolean state);
ENameSelector *	event_page_get_name_selector	(EventPage *epage);
void		event_page_add_attendee		(EventPage *epage,
						 EMeetingAttendee *attendee);
void		event_page_remove_all_attendees (EventPage *epage);
GtkWidget *	event_page_get_alarm_page	(EventPage *epage);
GtkWidget *	event_page_get_attendee_page	(EventPage *epage);

G_END_DECLS

#endif
