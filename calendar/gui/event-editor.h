/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#ifndef __EVENT_EDITOR_DIALOG_H__
#define __EVENT_EDITOR_DIALOG_H__

#include "gnome-cal.h"
#include <glade/glade.h>
#include <libgnomeui/gnome-dialog.h>

typedef struct {

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
} EventEditor;


GtkWidget *event_editor_new (GnomeCalendar *owner, iCalObject *ico);

/* Convenience function to create and show a new event editor for an
 * event that goes from day_begin to day_end of the specified day.
 */
void event_editor_new_whole_day (GnomeCalendar *owner, time_t day);

GtkWidget *make_date_edit (void);
GtkWidget *make_date_edit_with_time (void);
GtkWidget *date_edit_new (time_t the_time, int show_time);

GtkWidget *make_spin_button (int val, int low, int high);



#endif /* __EVENT_EDITOR_DIALOG_H__ */
