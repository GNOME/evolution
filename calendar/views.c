/*
 * Calendar views.
 * Copyright (C) 1998 the Free Software Foundation
 *
 * Author: Miguel de Icaza (miguel@kernel.org)
 */
#include <gnome.h>
#include "calendar.h"
#include "gnome-cal.h"

GtkWidget *
day_view_create (GnomeCalendar *gcal)
{
	return gtk_label_new ("This is supposed to be the Day View");
}

GtkWidget *
week_view_create (GnomeCalendar *gcal)
{
	return gtk_label_new ("This is supposed to be the Week View");
}

GtkWidget *
year_view_create (GnomeCalendar *gcal)
{
	return gtk_label_new ("This is supposed to be the Year View");
}

GtkWidget *
tasks_create (GnomeCalendar *gcal)
{
	return gtk_label_new ("This is supposed to be the Tasks View");
}
