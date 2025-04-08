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

typedef void (* EDatetimeFormatChangedFunc)	(const gchar *component,
						 const gchar *part,
						 DTFormatKind kind,
						 gpointer user_data);
void		e_datetime_format_add_change_listener
						(EDatetimeFormatChangedFunc func,
						 gpointer user_data);
void		e_datetime_format_remove_change_listener
						(EDatetimeFormatChangedFunc func,
						 gpointer user_data);

gchar *		e_datetime_format_dup_config_filename
						(void);
void		e_datetime_format_free_memory	(void);
void		e_datetime_format_add_setup_widget
						(GtkGrid *grid,
						 gint row,
						 const gchar *component,
						 const gchar *part,
						 DTFormatKind kind,
						 const gchar *caption);
gchar *		e_datetime_format_format	(const gchar *component,
						 const gchar *part,
						 DTFormatKind kind,
						 time_t value);
void		e_datetime_format_format_inline	(const gchar *component,
						 const gchar *part,
						 DTFormatKind kind,
						 time_t value,
						 gchar *buffer,
						 gint buffer_size);
gchar *		e_datetime_format_format_tm	(const gchar *component,
						 const gchar *part,
						 DTFormatKind kind,
						 struct tm *tm_time);
void		e_datetime_format_format_tm_inline
						(const gchar *component,
						 const gchar *part,
						 DTFormatKind kind,
						 struct tm *tm_time,
						 gchar *buffer,
						 gint buffer_size);
gboolean	e_datetime_format_includes_day_name
						(const gchar *component,
						 const gchar *part,
						 DTFormatKind kind);
const gchar *	e_datetime_format_get_format	(const gchar *component,
						 const gchar *part,
						 DTFormatKind kind);

G_END_DECLS

#endif /* E_DATETIME_FORMAT_H */
