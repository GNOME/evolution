/* Week view display for gncal
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 */

#include <config.h>
#include "year-view.h"


static void week_view_class_init (WeekViewClass *class);
static void week_view_init       (WeekView      *wv);


GtkType
week_view_get_type (void)
{
	static GtkType week_view_type = 0;

	if (!week_view_type) {
		GtkTypeInfo week_view_info = {
			"WeekView",
			sizeof (WeekView),
			sizeof (WeekViewClass),
			(GtkClassInitFunc) week_view_class_init,
			(GtkObjectInitFunc) week_view_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		week_view_type = gtk_type_unique (gnome_canvas_get_type (), &week_view_info);
	}

	return week_view_type;
}

static void
week_view_class_init (WeekViewClass *class)
{
	/* FIXME */
}

static void
week_view_init (WeekView *wv)
{
	GnomeCanvasGroup *root;

	root = gnome_canvas_root (GNOME_CANVAS (wv));

	/* Title */

	wv->title = gnome_canvas_item_new (root,
					   gnome_canvas_text_get_type (),
					   "anchor", GTK_ANCHOR_N,
					   "font", HEADING_FONT,
					   "fill_color", "black",
					   NULL);
}

GtkWidget *
week_view_new (GnomeCalendar *calendar, time_t week)
{
	WeekView *wv;

	g_return_val_if_fail (calendar != NULL, NULL);
	g_return_val_if_fail (GNOME_IS_CALENDAR (calendar), NULL);

	wv = gtk_type_new (week_view_get_type ());
	wv->calendar = calendar;

	week_view_colors_changed (wv);
	week_view_set (wv, week);
	return GTK_WIDGET (wv);
}

void
week_view_update (WeekView *wv, iCalObject *ico, int flags)
{
	/* FIXME */
}

void
week_view_set (WeekView *wv, time_t week)
{
	/* FIXME */
}

void
week_view_time_format_changed (WeekView *wv)
{
	/* FIXME */
}

void
week_view_colors_changed (WeekView *wv)
{
	/* FIXME */
}
