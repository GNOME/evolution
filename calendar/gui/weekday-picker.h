/* Evolution calendar - Week day picker widget
 *
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Author: Federico Mena-Quintero <federico@helixcode.com>
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

#ifndef WEEKDAY_PICKER_H
#define WEEKDAY_PICKER_H

#include <libgnome/gnome-defs.h>
#include <libgnomeui/gnome-canvas.h>

BEGIN_GNOME_DECLS



#define TYPE_WEEKDAY_PICKER            (weekday_picker_get_type ())
#define WEEKDAY_PICKER(obj)            (GTK_CHECK_CAST ((obj), TYPE_WEEKDAY_PICKER, WeekdayPicker))
#define WEEKDAY_PICKER_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), TYPE_WEEKDAY_PICKER,	\
					WeekdayPickerClass))
#define IS_WEEKDAY_PICKER(obj)         (GTK_CHECK_TYPE ((obj), TYPE_WEEKDAY_PICKER))
#define IS_WEEKDAY_PICKER_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), TYPE_WEEKDAY_PICKER))

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
};

GtkType weekday_picker_get_type (void);

GtkWidget *weekday_picker_new (void);

void weekday_picker_set_days (WeekdayPicker *wp, guint8 day_mask);
guint8 weekday_picker_get_days (WeekdayPicker *wp);

void weekday_picker_set_week_starts_on_monday (WeekdayPicker *wp, gboolean on_monday);
gboolean weekday_picker_get_week_starts_on_monday (WeekdayPicker *wp);



END_GNOME_DECLS

#endif
