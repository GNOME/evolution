/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * Author :
 *  Damon Chaplin <damon@ximian.com>
 *
 * Copyright 2001, Ximian, Inc.
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

/*
 * ECellDateEditText - a subclass of ECellText used to show and edit the text
 * representation of the date, from a ECalComponentDateTime* model value.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <libgnomeui/gnome-messagebox.h>
#include <libgnomeui/gnome-stock-icons.h> 
#include <libgnome/gnome-i18n.h>
#include <gal/util/e-util.h>
#include <e-util/e-time-utils.h>
#include <libecal/e-cal-time-util.h>

#include "e-cell-date-edit-text.h"

G_DEFINE_TYPE (ECellDateEditText, e_cell_date_edit_text, E_CELL_TEXT_TYPE);

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
	struct tm tmp_tm;
	char buffer[64];

	if (!dv)
		return g_strdup ("");

	/* Note that although the property may be in a different
	   timezone, we convert it to the current timezone to display
	   it in the table. If the user actually edits the value,
	   it will be set to the current timezone. See set_value(). */
	tmp_tm = icaltimetype_to_tm_with_zone (&dv->tt, dv->zone, ecd->zone);

	e_time_format_date_and_time (&tmp_tm, ecd->use_24_hour_format,
				     !dv->tt.is_date, FALSE,
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

	e_utf8_strftime (buffer, sizeof (buffer), format, tmp_tm);

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
	gboolean is_date = TRUE;

	/* Try to parse just a date first. If the value is only a date, we
	   use a DATE value. */
	status = e_time_parse_date (text, &tmp_tm);
	if (status == E_TIME_PARSE_INVALID) {
		is_date = FALSE;
		status = e_time_parse_date_and_time (text, &tmp_tm);

		if (status == E_TIME_PARSE_INVALID) {
			show_date_warning (ecd);
			return;
		}
	}

	if (status == E_TIME_PARSE_NONE) {
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
		dv.tt.is_date = is_date;

		/* FIXME: We assume it is being set to the current timezone.
		   Is that OK? */
		if (is_date) {
			dv.zone = NULL;
		} else {
			dv.zone = ecd->zone;
		}

		value = &dv;
	}

	e_table_model_set_value_at (model, col, row, value);
}


static void
e_cell_date_edit_text_class_init (ECellDateEditTextClass *ecdet)
{
	ECellTextClass *ectc = E_CELL_TEXT_CLASS (ecdet);

	ectc->get_text  = ecd_get_text;
	ectc->free_text = ecd_free_text;
	ectc->set_value = ecd_set_value;
}


static void
e_cell_date_edit_text_init (ECellDateEditText *ecd)
{
	ecd->zone = icaltimezone_get_utc_timezone ();
	ecd->use_24_hour_format = TRUE;
}


/**
 * e_cell_date_edit_text_new:
 *
 * Creates a new ECell renderer that can be used to render and edit dates that
 * that come from the model.  The value returned from the model is
 * interpreted as being a ECalComponentDateTime*.
 *
 * Returns: an ECell object that can be used to render dates.
 */
ECell *
e_cell_date_edit_text_new (const char *fontname,
			   GtkJustification justify)
{
	ECellDateEditText *ecd = g_object_new (e_cell_date_edit_text_get_type (), NULL);

	e_cell_text_construct (E_CELL_TEXT (ecd), fontname, justify);
      
	return (ECell *) ecd;
}

