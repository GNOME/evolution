/* Evolution calendar - Event editor dialog
 *
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Authors: Miguel de Icaza <miguel@helixcode.com>
 *          Federico Mena-Quintero <federico@helixcode.com>
 *          Seth Alves <alves@helixcode.com>
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

#ifndef __EVENT_EDITOR_DIALOG_H__
#define __EVENT_EDITOR_DIALOG_H__

#include <libgnome/gnome-defs.h>
#include <libgnomeui/gnome-app.h>
#include "gnome-cal.h"



#define TYPE_EVENT_EDITOR            (event_editor_get_type ())
#define EVENT_EDITOR(obj)            (GTK_CHECK_CAST ((obj), TYPE_EVENT_EDITOR, EventEditor))
#define EVENT_EDITOR_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), TYPE_EVENT_EDITOR,	\
				      EventEditorClass))
#define IS_EVENT_EDITOR(obj)         (GTK_CHECK_TYPE ((obj), TYPE_EVENT_EDITOR))
#define IS_EVENT_EDITOR_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), TYPE_EVENT_EDITOR))

typedef struct _EventEditor EventEditor;
typedef struct _EventEditorClass EventEditorClass;

struct _EventEditor {
	GnomeApp app;

	/* Private data */
	gpointer priv;

	/* The associated ical object */
	iCalObject *ical;

	/* The calendar owner of this event */
        GnomeCalendar *gnome_cal;

	/*
	char *description;
	char *host;
	int port;
	char *rootdn;
	*/
};

struct _EventEditorClass {
	GnomeAppClass parent_class;
};


GtkType event_editor_get_type (void);
GtkWidget *event_editor_construct (EventEditor *ee, GnomeCalendar *gcal, iCalObject *ico);

GtkWidget *event_editor_new (GnomeCalendar *gcal, iCalObject *ico);

#if 0
/* Convenience function to create and show a new event editor for an
 * event that goes from day_begin to day_end of the specified day.
 */
void event_editor_new_whole_day (GnomeCalendar *owner, time_t day);
#endif

GtkWidget *make_date_edit (void);
GtkWidget *make_date_edit_with_time (void);
GtkWidget *date_edit_new (time_t the_time, int show_time);

GtkWidget *make_spin_button (int val, int low, int high);



#endif /* __EVENT_EDITOR_DIALOG_H__ */
