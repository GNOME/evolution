/*
 * Author :
 *  Damon Chaplin <damon@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * Based on the GnomeDateEdit, part of the Gnome Library.
 * Copyright (C) 1997, 1998, 1999, 2000 Free Software Foundation
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * published by the Free Software Foundation; either the version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser  General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

/*
 * EDateEdit - a widget based on GnomeDateEdit to provide a date & optional
 * time field with popups for entering a date.
 *
 * It emits a "changed" signal when the date and/or time has changed.
 * You can check if the last date or time entered was invalid by
 * calling e_date_edit_date_is_valid() and e_date_edit_time_is_valid().
 *
 * Note that when the user types in a date or time, it will only emit the
 * signals when the user presses the return key or switches the keyboard
 * focus to another widget, or you call one of the _get_time/date functions.
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_DATE_EDIT_H
#define E_DATE_EDIT_H

#include <time.h>
#include <gtk/gtk.h>

/* Standard GObject macros */
#define E_TYPE_DATE_EDIT \
	(e_date_edit_get_type ())
#define E_DATE_EDIT(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_DATE_EDIT, EDateEdit))
#define E_DATE_EDIT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_DATE_EDIT, EDateEditClass))
#define E_IS_DATE_EDIT(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_DATE_EDIT))
#define E_IS_DATE_EDIT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_DATE_EDIT))
#define E_DATE_EDIT_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_DATE_EDIT, EDateEditClass))

G_BEGIN_DECLS

typedef struct _EDateEdit EDateEdit;
typedef struct _EDateEditClass EDateEditClass;
typedef struct _EDateEditPrivate EDateEditPrivate;

/* The type of the callback function optionally used to get the current time.
 */
typedef struct tm	(*EDateEditGetTimeCallback)
						(EDateEdit *dedit,
						 gpointer data);

struct _EDateEdit {
	GtkBox hbox;
	EDateEditPrivate *priv;
};

struct _EDateEditClass {
	GtkBoxClass parent_class;

	/* Signals */
	void		(*changed)		(EDateEdit *dedit);
};

GType		e_date_edit_get_type		(void) G_GNUC_CONST;
GtkWidget *	e_date_edit_new			(void);

/* Analogous to gtk_editable_set_editable.  disable editing, while still
 * allowing selection. */
void		e_date_edit_set_editable	(EDateEdit *dedit,
						 gboolean editable);

/* Returns TRUE if the last date and time set were valid. The date and time
 * are only set when the user hits Return or switches keyboard focus, or
 * selects a date or time from the popup. */
gboolean	e_date_edit_date_is_valid	(EDateEdit *dedit);
gboolean	e_date_edit_time_is_valid	(EDateEdit *dedit);

/* Returns the last valid date & time set, or -1 if the date & time was set to
 * 'None' and this is permitted via e_date_edit_set_allow_no_date_set. */
time_t		e_date_edit_get_time		(EDateEdit *dedit);
void		e_date_edit_set_time		(EDateEdit *dedit,
						 time_t the_time);

/* This returns the last valid date set, without the time. It returns TRUE
 * if a date is set, or FALSE if the date is set to 'None' and this is
 * permitted via e_date_edit_set_allow_no_date_set. (Month is 1 - 12). */
gboolean	e_date_edit_get_date		(EDateEdit *dedit,
						 gint *year,
						 gint *month,
						 gint *day);
void		e_date_edit_set_date		(EDateEdit *dedit,
						 gint year,
						 gint month,
						 gint day);

/* This returns the last valid time set, without the date. It returns TRUE
 * if a time is set, or FALSE if the time is set to 'None' and this is
 * permitted via e_date_edit_set_allow_no_date_set. */
gboolean	e_date_edit_get_time_of_day	(EDateEdit *dedit,
						 gint *hour,
						 gint *minute);
/* Set the time. Pass -1 as hour to set to empty. */
void		e_date_edit_set_time_of_day	(EDateEdit *dedit,
						 gint		 hour,
						 gint		 minute);

void		e_date_edit_set_date_and_time_of_day
						(EDateEdit *dedit,
						 gint year,
						 gint month,
						 gint day,
						 gint hour,
						 gint minute);

/* Whether we show the date field. */
gboolean	e_date_edit_get_show_date	(EDateEdit *dedit);
void		e_date_edit_set_show_date	(EDateEdit *dedit,
						 gboolean show_date);

/* Whether we show the time field. */
gboolean	e_date_edit_get_show_time	(EDateEdit *dedit);
void		e_date_edit_set_show_time	(EDateEdit *dedit,
						 gboolean show_time);

/* The week start day, used in the date popup. */
GDateWeekday	e_date_edit_get_week_start_day	(EDateEdit *dedit);
void		e_date_edit_set_week_start_day	(EDateEdit *dedit,
						 GDateWeekday week_start_day);

/* Whether we show week numbers in the date popup. */
gboolean	e_date_edit_get_show_week_numbers
						(EDateEdit *dedit);
void		e_date_edit_set_show_week_numbers
						(EDateEdit *dedit,
						 gboolean show_week_numbers);

/* Whether we use 24 hour format in the time field & popup. */
gboolean	e_date_edit_get_use_24_hour_format
						(EDateEdit *dedit);
void		e_date_edit_set_use_24_hour_format
						(EDateEdit *dedit,
						 gboolean use_24_hour_format);

/* Whether we allow the date to be set to 'None'. e_date_edit_get_time() will
 * return (time_t) -1 in this case. */
gboolean	e_date_edit_get_allow_no_date_set
						(EDateEdit *dedit);
void		e_date_edit_set_allow_no_date_set
						(EDateEdit *dedit,
						 gboolean allow_no_date_set);

/* The range of time to show in the time combo popup. */
void		e_date_edit_get_time_popup_range
						(EDateEdit *dedit,
						 gint *lower_hour,
						 gint *upper_hour);
void		e_date_edit_set_time_popup_range
						(EDateEdit *dedit,
						 gint lower_hour,
						 gint upper_hour);

/* Whether the time field is made insensitive rather than hiding it. */
gboolean	e_date_edit_get_make_time_insensitive
						(EDateEdit *dedit);
void		e_date_edit_set_make_time_insensitive
						(EDateEdit *dedit,
						 gboolean make_insensitive);

/* Whether two-digit years in date could be modified as in future; default is TRUE */
gboolean	e_date_edit_get_twodigit_year_can_future
						(EDateEdit *dedit);
void		e_date_edit_set_twodigit_year_can_future
						(EDateEdit *dedit,
						 gboolean value);

/* Sets a callback to use to get the current time. This is useful if the
 * application needs to use its own timezone data rather than rely on the
 * Unix timezone. */
void		e_date_edit_set_get_time_callback
						(EDateEdit *dedit,
						 EDateEditGetTimeCallback cb,
						 gpointer data,
						 GDestroyNotify destroy);

GtkWidget *	e_date_edit_get_entry		(EDateEdit *dedit);

gboolean	e_date_edit_has_focus		(EDateEdit *dedit);

gint		e_date_edit_get_shorten_time	(EDateEdit *self);
void		e_date_edit_set_shorten_time	(EDateEdit *self,
						 gint minutes);
gboolean	e_date_edit_get_shorten_time_end(EDateEdit *self);
void		e_date_edit_set_shorten_time_end(EDateEdit *self,
						 gboolean shorten_time_end);
const gchar *	e_date_edit_get_date_format	(EDateEdit *self);
void		e_date_edit_set_date_format	(EDateEdit *self,
						 const gchar *strftime_format);

G_END_DECLS

#endif /* E_DATE_EDIT_H */
