/* Day view notebook panel for gncal
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Author: Federico Mena <quartic@gimp.org>
 */

#ifndef GNCAL_DAY_PANEL_H
#define GNCAL_DAY_PANEL_H

#include <gtk/gtklabel.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtktable.h>
#include <libgnome/gnome-defs.h>
#include "gnome-cal.h"
#include "gncal-full-day.h"
#include "gncal-todo.h"


BEGIN_GNOME_DECLS


#define GNCAL_DAY_PANEL(obj)         GTK_CHECK_CAST (obj, gncal_day_panel_get_type (), GncalDayPanel)
#define GNCAL_DAY_PANEL_CLASS(klass) GTK_CHECK_CLASS_CAST (klass, gncal_day_panel_get_type (), GncalDayPanelClass)
#define GNCAL_IS_DAY_PANEL(obj)      GTK_CHECK_TYPE (obj, gncal_day_panel_get_type ())


typedef struct _GncalDayPanel GncalDayPanel;
typedef struct _GncalDayPanelClass GncalDayPanelClass;

struct _GncalDayPanel {
	GtkTable table;

	GnomeCalendar *calendar;	/* the calendar we are associated to */

	time_t start_of_day;

	GtkLabel          *date_label;
	GncalFullDay      *fullday;
	GtkScrolledWindow *fullday_sw;
	GtkCalendar       *gtk_calendar;
	GncalTodo         *todo;

	guint day_selected_id;
};

struct _GncalDayPanelClass {
	GtkTableClass parent_class;
};


guint gncal_day_panel_get_type (void);
GtkWidget *gncal_day_panel_new (GnomeCalendar *calendar, time_t start_of_day);

void gncal_day_panel_update (GncalDayPanel *dpanel, iCalObject *ico, int flags);
void gncal_day_panel_set (GncalDayPanel *dpanel, time_t start_of_day);
void gncal_day_panel_time_format_changed (GncalDayPanel *dpanel);

void todo_list_properties_changed (GncalDayPanel *dpanel);



END_GNOME_DECLS

#endif
