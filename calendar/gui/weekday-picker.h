/*
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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef WEEKDAY_PICKER_H
#define WEEKDAY_PICKER_H

#include <libgnomecanvas/gnome-canvas.h>

G_BEGIN_DECLS



#define TYPE_WEEKDAY_PICKER            (weekday_picker_get_type ())
#define WEEKDAY_PICKER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_WEEKDAY_PICKER, WeekdayPicker))
#define WEEKDAY_PICKER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_WEEKDAY_PICKER,	\
					WeekdayPickerClass))
#define IS_WEEKDAY_PICKER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_WEEKDAY_PICKER))
#define IS_WEEKDAY_PICKER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_WEEKDAY_PICKER))

typedef struct _WeekdayPicker WeekdayPicker;
typedef struct _WeekdayPickerClass WeekdayPickerClass;
typedef struct _WeekdayPickerPrivate WeekdayPickerPrivate;

struct _WeekdayPicker {
	GnomeCanvas canvas;

	/* Private data */
	WeekdayPickerPrivate *priv;
};

struct _WeekdayPickerClass {
	GnomeCanvasClass parent_class;

	void (* changed) (WeekdayPicker *wp);
};

GType weekday_picker_get_type (void);

GtkWidget *weekday_picker_new (void);

void weekday_picker_set_days (WeekdayPicker *wp, guint8 day_mask);
guint8 weekday_picker_get_days (WeekdayPicker *wp);

void weekday_picker_set_blocked_days (WeekdayPicker *wp, guint8 blocked_day_mask);
guint weekday_picker_get_blocked_days (WeekdayPicker *wp);

void weekday_picker_set_week_start_day (WeekdayPicker *wp, gint week_start_day);
gint weekday_picker_get_week_start_day (WeekdayPicker *wp);



G_END_DECLS

#endif
