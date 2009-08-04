/*
 *
 * Customizable date/time formatting in Evolution
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
 * Copyright (C) 1999-2009 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __E_DATETIME_FORMAT__
#define __E_DATETIME_FORMAT__

#include <time.h>
#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef enum _DTFormatKind {
	DTFormatKindDate,
	DTFormatKindTime,
	DTFormatKindDateTime,
	DTFormatKindShortDate
} DTFormatKind;

void e_datetime_format_add_setup_widget (GtkWidget *table, gint row, const gchar *component, const gchar *part, DTFormatKind kind, const gchar *caption);

gchar *e_datetime_format_format (const gchar *component, const gchar *part, DTFormatKind kind, time_t value);
gchar *e_datetime_format_format_tm (const gchar *component, const gchar *part, DTFormatKind kind, struct tm *tm_time);

G_END_DECLS

#endif
