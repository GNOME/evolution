/*
 * GnomeCalendar widget
 * Copyright (C) 1998 the Free Software Foundation
 *
 * Author: Miguel de Icaza (miguel@kernel.org)
 */

#include <gnome.h>
#include "calendar.h"
#include "gnome-cal.h"
#include "views.h"

static void gnome_calendar_init                    (GnomeCalendar *gcal);

GnomeApp *parent_class;

guint
gnome_calendar_get_type (void)
{
	static guint gnome_calendar_type = 0;
	if(!gnome_calendar_type) {
		GtkTypeInfo gnome_calendar_info = {
			"GnomeCalendar",
			sizeof(GnomeCalendar),
			sizeof(GnomeCalendarClass),
			(GtkClassInitFunc) NULL,
			(GtkObjectInitFunc) gnome_calendar_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL,
		};
		gnome_calendar_type = gtk_type_unique(gnome_app_get_type(), &gnome_calendar_info);
		parent_class = gtk_type_class (gnome_app_get_type());
	}
	return gnome_calendar_type;
}

static void
setup_widgets (GnomeCalendar *gcal)
{
	GtkWidget *notebook;
	GtkWidget *day_view, *week_view, *year_view, *task_view;
	time_t now;

	now = time (NULL);
	
	notebook  = gtk_notebook_new ();
	day_view  = day_view_create  (gcal);
	week_view = gncal_week_view_new (gcal->cal, now);
	year_view = year_view_create (gcal);
	task_view = tasks_create (gcal);
	
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), day_view,  gtk_label_new (_("Day View")));
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), week_view, gtk_label_new (_("Week View")));
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), year_view, gtk_label_new (_("Year View")));
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), task_view, gtk_label_new (_("Tasks")));

	gtk_widget_show_all (notebook);
	
	gnome_app_set_contents (GNOME_APP (gcal), notebook);
	
}

static void
gnome_calendar_init(GnomeCalendar *gcal)
{
	gcal->cal = 0;

	setup_widgets (gcal);
}

GtkWidget *
gnome_calendar_new (char *title)
{
	GtkWidget      *retval;
	GnomeCalendar  *gcal;
	GnomeApp       *app;
		
	retval = gtk_type_new (gnome_calendar_get_type ());
	app = GNOME_APP (retval);
	gcal = GNOME_CALENDAR (retval);
	
	app->name = g_strdup ("calendar");
	app->prefix = g_copy_strings ("/", app->name, "/", NULL);
	
	gtk_window_set_title(GTK_WINDOW(retval), title);

	gcal->cal = calendar_new (title);
	return retval;
}

void
gnome_calendar_load (GnomeCalendar *gcal, char *file)
{
	calendar_load (gcal->cal, file);
}
