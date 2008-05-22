/* Evolution calendar - Week day picker widget
 *
 * Copyright (C) 2000 Ximian, Inc.
 *
 * Author: Federico Mena-Quintero <federico@ximian.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

void weekday_picker_set_week_start_day (WeekdayPicker *wp, int week_start_day);
int weekday_picker_get_week_start_day (WeekdayPicker *wp);



G_END_DECLS

#endif
