/* Evolution calendar - Main page of the event editor dialog
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

#ifndef EVENT_PAGE_H
#define EVENT_PAGE_H

#include <bonobo/bonobo-window.h>
#include <bonobo/bonobo-ui-util.h>
#include <bonobo/bonobo-widget.h>
#include "comp-editor-page.h"
#include "../e-meeting-attendee.h"
#include "../e-meeting-store.h"
#include "../e-meeting-list-view.h"

G_BEGIN_DECLS



#define TYPE_EVENT_PAGE            (event_page_get_type ())
#define EVENT_PAGE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_EVENT_PAGE, EventPage))
#define EVENT_PAGE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_EVENT_PAGE, EventPageClass))
#define IS_EVENT_PAGE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_EVENT_PAGE))
#define IS_EVENT_PAGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), TYPE_EVENT_PAGE))

typedef struct _EventPagePrivate EventPagePrivate;

typedef struct {
	CompEditorPage page;

	/* Private data */
	EventPagePrivate *priv;
} EventPage;

typedef struct {
	CompEditorPageClass parent_class;
} EventPageClass;


GtkType    event_page_get_type  (void);
EventPage *event_page_construct (EventPage *epage, EMeetingStore *model, ECal *client);
EventPage *event_page_new       (EMeetingStore *model, ECal *client, BonoboUIComponent *uic);
ECalComponent *event_page_get_cancel_comp (EventPage *page);
void event_page_show_options (EventPage *page);
void event_page_hide_options (EventPage *page);
void event_page_sendoptions_clicked_cb (EventPage *epage);
void event_page_set_meeting (EventPage *page, gboolean set);
void event_page_set_show_timezone (EventPage *epage, gboolean state);
void event_page_set_view_rsvp (EventPage *epage, gboolean state);
void event_page_set_classification (EventPage *epage, ECalComponentClassification class);
void event_page_set_delegate (EventPage *page, gboolean set);
void event_page_set_all_day_event (EventPage *epage, gboolean all_day);
void event_page_set_show_categories (EventPage *epage, gboolean state);
void event_page_set_show_time_busy (EventPage *epage, gboolean state);
void event_page_show_alarm (EventPage *epage);

void event_page_set_view_attendee (EventPage *epage, gboolean state);
void event_page_set_view_role (EventPage *epage, gboolean state);
void event_page_set_view_status (EventPage *epage, gboolean state);
void event_page_set_view_type (EventPage *epage, gboolean state);
void event_page_set_view_rvsp (EventPage *epage, gboolean state);



G_END_DECLS

#endif
