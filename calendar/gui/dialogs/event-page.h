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

#include "comp-editor-page.h"

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
EventPage *event_page_construct (EventPage *epage);
EventPage *event_page_new       (void);
void event_page_show_options (EventPage *page);
void event_page_hide_options (EventPage *page);


G_END_DECLS

#endif
