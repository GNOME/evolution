/* Evolution calendar - Main page of the event editor dialog
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

#ifndef EVENT_PAGE_H
#define EVENT_PAGE_H

#include "editor-page.h"

BEGIN_GNOME_DECLS



#define TYPE_EVENT_PAGE            (event_page_get_type ())
#define EVENT_PAGE(obj)            (GTK_CHECK_CAST ((obj), TYPE_EVENT_PAGE, EventPage))
#define EVENT_PAGE_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), TYPE_EVENT_PAGE,		\
				    EventPageClass))
#define IS_EVENT_PAGE(obj)         (GTK_CHECK_TYPE ((obj), TYPE_EVENT_PAGE))
#define IS_EVENT_PAGE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((obj), TYPE_EVENT_PAGE))

typedef struct _EventPagePrivate EventPagePrivate;

typedef struct {
	EditorPage page;

	/* Private data */
	EventPagePrivate *priv;
} EventPage;

typedef struct {
	EditorPageClass parent_class;

	/* Notification signals */

	void (* dates_changed) (EventPage *epage);
} EventPageClass;

GtkType event_page_get_type (void);

EventPage *event_page_construct (EventPage *epage);

EventPage *event_page_new (void);

void event_page_get_dates (EventPage *epage, time_t *start, time_t *end);



END_GNOME_DECLS

#endif
