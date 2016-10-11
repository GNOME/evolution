/*
 *
 * Customizable date/time formatting in Evolution
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Copyright (C) 1999-2009 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_DATETIME_FORMAT_H
#define E_DATETIME_FORMAT_H

#include <time.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef enum {
	DTFormatKindDate,
	DTFormatKindTime,
	DTFormatKindDateTime,
	DTFormatKindShortDate
} DTFormatKind;

void		e_datetime_format_add_setup_widget
						(GtkWidget *table,
						 gint row,
						 const gchar *component,
						 const gchar *part,
						 DTFormatKind kind,
						 const gchar *caption);
gchar *		e_datetime_format_format	(const gchar *component,
						 const gchar *part,
						 DTFormatKind kind,
						 time_t value);
gchar *		e_datetime_format_format_tm	(const gchar *component,
						 const gchar *part,
						 DTFormatKind kind,
						 struct tm *tm_time);
gboolean	e_datetime_format_includes_day_name
						(const gchar *component,
						 const gchar *part,
						 DTFormatKind kind);

G_END_DECLS

#endif /* E_DATETIME_FORMAT_H */
