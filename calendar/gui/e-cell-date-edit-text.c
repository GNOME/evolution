/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * Author :
 *  Damon Chaplin <damon@ximian.com>
 *
 * Copyright 2001, Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

/*
 * ECellDateEditText - a subclass of ECellText used to show and edit the text
 * representation of the date, from a CalComponentDateTime* model value.
 */

#include <config.h>

#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <libgnomeui/gnome-messagebox.h>
#include <libgnomeui/gnome-stock.h>
#include <libgnome/gnome-i18n.h>
#include <gal/util/e-util.h>
#include <gal/widgets/e-unicode.h>
#include <e-util/e-time-utils.h>
#include <cal-util/timeutil.h>

#include "e-cell-date-edit-text.h"


#define PARENT_TYPE e_cell_text_get_type ()

static ECellTextClass *parent_class;


void
e_cell_date_edit_text_set_timezone (ECellDateEditText *ecd,
				    icaltimezone *zone)
{
	g_return_if_fail (E_IS_CELL_DATE_EDIT_TEXT (ecd));

	ecd->zone = zone;
}


void
e_cell_date_edit_text_set_use_24_hour_format (ECellDateEditText *ecd,
					      gboolean use_24_hour)
{
	g_return_if_fail (E_IS_CELL_DATE_EDIT_TEXT (ecd));

	ecd->use_24_hour_format = use_24_hour;
}


static char *
ecd_get_text (ECellText *cell, ETableModel *model, int col, int row)
{
	ECellDateEditText *ecd = E_CELL_DATE_EDIT_TEXT (cell);
	ECellDateEditValue *dv = e_table_model_value_at (model, col, row);
	struct icaltimetype tt;
	struct tm tmp_tm;
	char buffer[64];

	if (!dv)
		return g_strdup ("");

	/* Note that although the property may be in a different
	   timezone, we convert it to the current timezone to display
	   it in the table. If the user actually edits the value,
	   it will be set to the current timezone. See set_value(). */
	tt = dv->tt;
	icaltimezone_convert_time (&tt, dv->zone, ecd->zone);

	tmp_tm.tm_year = tt.year - 1900;
	tmp_tm.tm_mon = tt.month - 1;
	tmp_tm.tm_mday = tt.day;
	tmp_tm.tm_hour = tt.hour;
	tmp_tm.tm_min = tt.minute;
	tmp_tm.tm_sec = tt.second;
	tmp_tm.tm_isdst = -1;

	tmp_tm.tm_wday = time_day_of_week (tt.day, tt.month - 1, tt.year);

	e_time_format_date_and_time (&tmp_tm, ecd->use_24_hour_format,
				     TRUE, FALSE,
				     buffer, sizeof (buffer));
	return g_strdup (buffer);
}


static void
ecd_free_text (ECellText *cell, char *text)
{
	g_free (text);
}


/* FIXME: We need to set the "transient_for" property for the dialog. */
static void
show_date_warning (ECellDateEditText *ecd)
{
	GtkWidget *dialog;
	char buffer[64], message[256], *format;
	time_t t;
	struct tm *tmp_tm;

	t = time (NULL);
	/* We are only using this as an example, so the timezone doesn't
	   matter. */
	tmp_tm = localtime (&t);

	if (ecd->use_24_hour_format)
		/* strftime format of a weekday, a date and a time, 24-hour. */
		format = _("%a %m/%d/%Y %H:%M:%S");
	else
		/* strftime format of a weekday, a date and a time, 12-hour. */
		format = _("%a %m/%d/%Y %I:%M:%S %p");

	strftime (buffer, sizeof (buffer), format, tmp_tm);

	g_snprintf (message, 256,
		    _("The date must be entered in the format: \n\n%s"),
		    buffer);

	dialog = gnome_message_box_new (message,
					GNOME_MESSAGE_BOX_ERROR,
					GNOME_STOCK_BUTTON_OK, NULL);
	gtk_widget_show (dialog);
}


static void
ecd_set_value (ECellText *cell, ETableModel *model, int col, int row,
	       const char *text)
{
	ECellDateEditText *ecd = E_CELL_DATE_EDIT_TEXT (cell);
	ETimeParseStatus status;
	struct tm tmp_tm;
	ECellDateEditValue *value;

	status = e_time_parse_date_and_time (text, &tmp_tm);

	if (status == E_TIME_PARSE_INVALID) {
		show_date_warning (ecd);
		return;
	} else if (status == E_TIME_PARSE_NONE) {
		value = NULL;
	} else {
		ECellDateEditValue dv;

		dv.tt = icaltime_null_time ();

		dv.tt.year   = tmp_tm.tm_year + 1900;
		dv.tt.month  = tmp_tm.tm_mon + 1;
		dv.tt.day    = tmp_tm.tm_mday;
		dv.tt.hour   = tmp_tm.tm_hour;
		dv.tt.minute = tmp_tm.tm_min;
		dv.tt.second = tmp_tm.tm_sec;

		/* FIXME: We assume it is being set to the current timezone.
		   Is that OK? */
		dv.zone	     = ecd->zone;

		value = &dv;
	}

	e_table_model_set_value_at (model, col, row, value);
}


static void
e_cell_date_edit_text_class_init (GtkObjectClass *object_class)
{
	ECellTextClass *ectc = (ECellTextClass *) object_class;

	parent_class = gtk_type_class (PARENT_TYPE);

	ectc->get_text  = ecd_get_text;
	ectc->free_text = ecd_free_text;
	ectc->set_value = ecd_set_value;
}


static void
e_cell_date_edit_text_init (GtkObject *object)
{
	ECellDateEditText *ecd = E_CELL_DATE_EDIT_TEXT (object);

	ecd->zone = icaltimezone_get_utc_timezone ();
	ecd->use_24_hour_format = TRUE;
}


/**
 * e_cell_date_edit_text_new:
 *
 * Creates a new ECell renderer that can be used to render and edit dates that
 * that come from the model.  The value returned from the model is
 * interpreted as being a CalComponentDateTime*.
 *
 * Returns: an ECell object that can be used to render dates.
 */
ECell *
e_cell_date_edit_text_new (const char *fontname,
			   GtkJustification justify)
{
	ECellDateEditText *ecd = gtk_type_new (e_cell_date_edit_text_get_type ());

	e_cell_text_construct (E_CELL_TEXT (ecd), fontname, justify);
      
	return (ECell *) ecd;
}


E_MAKE_TYPE (e_cell_date_edit_text, "ECellDateEditText", ECellDateEditText,
	     e_cell_date_edit_text_class_init, e_cell_date_edit_text_init,
	     PARENT_TYPE);
