/*
 * ECellDateEditText - a subclass of ECellText used to show and edit the text
 * representation of the date, from a ECalComponentDateTime* model value.
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
 * Authors:
 *		Damon Chaplin <damon@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 */

#include "evolution-config.h"

#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <glib/gi18n.h>
#include <libecal/libecal.h>

#include "e-cell-date-edit-text.h"

struct _ECellDateEditTextPrivate {

	/* The timezone to display the date in. */
	ICalTimezone *timezone;

	/* Whether to display in 24-hour format. */
	gboolean use_24_hour_format;
};

enum {
	PROP_0,
	PROP_TIMEZONE,
	PROP_USE_24_HOUR_FORMAT
};

G_DEFINE_TYPE_WITH_PRIVATE (ECellDateEditText, e_cell_date_edit_text, E_TYPE_CELL_TEXT)

static void
cell_date_edit_text_set_property (GObject *object,
                                  guint property_id,
                                  const GValue *value,
                                  GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_TIMEZONE:
			e_cell_date_edit_text_set_timezone (
				E_CELL_DATE_EDIT_TEXT (object),
				g_value_get_object (value));
			return;

		case PROP_USE_24_HOUR_FORMAT:
			e_cell_date_edit_text_set_use_24_hour_format (
				E_CELL_DATE_EDIT_TEXT (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
cell_date_edit_text_get_property (GObject *object,
                                  guint property_id,
                                  GValue *value,
                                  GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_TIMEZONE:
			g_value_set_object (
				value,
				e_cell_date_edit_text_get_timezone (
				E_CELL_DATE_EDIT_TEXT (object)));
			return;

		case PROP_USE_24_HOUR_FORMAT:
			g_value_set_boolean (
				value,
				e_cell_date_edit_text_get_use_24_hour_format (
				E_CELL_DATE_EDIT_TEXT (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
cell_date_edit_text_finalize (GObject *object)
{
	ECellDateEditText *ecd = E_CELL_DATE_EDIT_TEXT (object);

	g_clear_object (&ecd->priv->timezone);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_cell_date_edit_text_parent_class)->finalize (object);
}

static gchar *
cell_date_edit_text_get_text (ECellText *cell,
                              ETableModel *model,
                              gint col,
                              gint row)
{
	ECellDateEditText *ecd = E_CELL_DATE_EDIT_TEXT (cell);
	ECellDateEditValue *dv = e_table_model_value_at (model, col, row);
	ICalTimezone *timezone;
	ICalTime *tt;
	struct tm tmp_tm;
	gchar *res;

	if (!dv)
		return g_strdup ("");

	timezone = e_cell_date_edit_text_get_timezone (ecd);

	tt = e_cell_date_edit_value_get_time (dv);

	/* Note that although the property may be in a different
	 * timezone, we convert it to the current timezone to display
	 * it in the table. If the user actually edits the value,
	 * it will be set to the current timezone. See set_value (). */
	tmp_tm = e_cal_util_icaltime_to_tm_with_zone (tt, e_cell_date_edit_value_get_zone (dv), timezone);

	res = e_datetime_format_format_tm (
		"calendar", "table", i_cal_time_is_date (tt) ?
		DTFormatKindDate : DTFormatKindDateTime, &tmp_tm);

	e_table_model_free_value (model, col, dv);

	return res;
}

static void
cell_date_edit_text_free_text (ECellText *cell,
			       ETableModel *model,
			       gint col,
                               gchar *text)
{
	g_free (text);
}

/* FIXME: We need to set the "transient_for" property for the dialog. */
static void
show_date_warning (ECellDateEditText *ecd)
{
	GtkWidget *dialog;
	gchar buffer[64], *format;
	time_t t;
	struct tm *tmp_tm;

	t = time (NULL);
	/* We are only using this as an example, so the timezone doesn't
	 * matter. */
	tmp_tm = localtime (&t);

	if (e_cell_date_edit_text_get_use_24_hour_format (ecd))
		/* strftime format of a weekday, a date and a time, 24-hour. */
		format = _("%a %m/%d/%Y %H:%M:%S");
	else
		/* strftime format of a weekday, a date and a time, 12-hour. */
		format = _("%a %m/%d/%Y %I:%M:%S %p");

	e_utf8_strftime (buffer, sizeof (buffer), format, tmp_tm);

	dialog = gtk_message_dialog_new (
		NULL, 0,
		GTK_MESSAGE_ERROR,
		GTK_BUTTONS_OK,
		_("The date must be entered in the format: \n%s"),
		buffer);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

static void
cell_date_edit_text_set_value (ECellText *cell,
                               ETableModel *model,
                               gint col,
                               gint row,
                               const gchar *text)
{
	ECellDateEditText *ecd = E_CELL_DATE_EDIT_TEXT (cell);
	ETimeParseStatus status;
	struct tm tmp_tm;
	ECellDateEditValue *dv = NULL;
	ECellDateEditValue *value;
	gboolean is_date = TRUE;

	/* Try to parse just a date first. If the value is only a date, we
	 * use a DATE value. */
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
		ICalTime *tt;
		ICalTimezone *zone;

		tt = e_cal_util_tm_to_icaltime (&tmp_tm, is_date);

		if (is_date) {
			zone = NULL;
		} else {
			zone = e_cell_date_edit_text_get_timezone (ecd);
		}

		dv = e_cell_date_edit_value_new (tt, zone);
		value = dv;

		g_clear_object (&tt);
	}

	e_table_model_set_value_at (model, col, row, value);

	e_cell_date_edit_value_free (dv);
}

static void
e_cell_date_edit_text_class_init (ECellDateEditTextClass *class)
{
	GObjectClass *object_class;
	ECellTextClass *cell_text_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = cell_date_edit_text_set_property;
	object_class->get_property = cell_date_edit_text_get_property;
	object_class->finalize = cell_date_edit_text_finalize;

	cell_text_class = E_CELL_TEXT_CLASS (class);
	cell_text_class->get_text = cell_date_edit_text_get_text;
	cell_text_class->free_text = cell_date_edit_text_free_text;
	cell_text_class->set_value = cell_date_edit_text_set_value;

	g_object_class_install_property (
		object_class,
		PROP_TIMEZONE,
		g_param_spec_object (
			"timezone",
			"Time Zone",
			NULL,
			I_CAL_TYPE_TIMEZONE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_USE_24_HOUR_FORMAT,
		g_param_spec_boolean (
			"use-24-hour-format",
			"Use 24-Hour Format",
			NULL,
			TRUE,
			G_PARAM_READWRITE));
}

static void
e_cell_date_edit_text_init (ECellDateEditText *ecd)
{
	ecd->priv = e_cell_date_edit_text_get_instance_private (ecd);

	ecd->priv->timezone = e_cal_util_copy_timezone (i_cal_timezone_get_utc_timezone ());
	ecd->priv->use_24_hour_format = TRUE;
	g_object_set (ecd, "use-tabular-numbers", TRUE, NULL);
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
e_cell_date_edit_text_new (const gchar *fontname,
                           GtkJustification justify)
{
	ECell *cell;

	cell = g_object_new (E_TYPE_CELL_DATE_EDIT_TEXT, NULL);
	e_cell_text_construct (E_CELL_TEXT (cell), fontname, justify);

	return cell;
}

ICalTimezone *
e_cell_date_edit_text_get_timezone (ECellDateEditText *ecd)
{
	g_return_val_if_fail (E_IS_CELL_DATE_EDIT_TEXT (ecd), NULL);

	return ecd->priv->timezone;
}

void
e_cell_date_edit_text_set_timezone (ECellDateEditText *ecd,
				    const ICalTimezone *timezone)
{
	g_return_if_fail (E_IS_CELL_DATE_EDIT_TEXT (ecd));

	if (ecd->priv->timezone == timezone)
		return;

	g_clear_object (&ecd->priv->timezone);
	ecd->priv->timezone = timezone ? e_cal_util_copy_timezone (timezone) : NULL;

	g_object_notify (G_OBJECT (ecd), "timezone");
}

gboolean
e_cell_date_edit_text_get_use_24_hour_format (ECellDateEditText *ecd)
{
	g_return_val_if_fail (E_IS_CELL_DATE_EDIT_TEXT (ecd), FALSE);

	return ecd->priv->use_24_hour_format;
}

void
e_cell_date_edit_text_set_use_24_hour_format (ECellDateEditText *ecd,
                                              gboolean use_24_hour)
{
	g_return_if_fail (E_IS_CELL_DATE_EDIT_TEXT (ecd));

	if (ecd->priv->use_24_hour_format == use_24_hour)
		return;

	ecd->priv->use_24_hour_format = use_24_hour;

	g_object_notify (G_OBJECT (ecd), "use-24-hour-format");
}

gint
e_cell_date_edit_compare_cb (gconstpointer a,
                             gconstpointer b,
                             gpointer cmp_cache)
{
	ECellDateEditValue *dv1 = (ECellDateEditValue *) a;
	ECellDateEditValue *dv2 = (ECellDateEditValue *) b;
	ICalTime *tt;
	gint res;

	/* First check if either is NULL. NULL dates sort last. */
	if (!dv1 || !dv2) {
		if (dv1 == dv2)
			return 0;
		else if (dv1)
			return -1;
		else
			return 1;
	}

	/* Copy the 2nd value and convert it to the same timezone as the first. */
	tt = i_cal_time_clone (e_cell_date_edit_value_get_time (dv2));
	i_cal_time_convert_timezone (tt, e_cell_date_edit_value_get_zone (dv2), e_cell_date_edit_value_get_zone (dv1));

	/* Now we can compare them. */
	res = i_cal_time_compare (e_cell_date_edit_value_get_time (dv1), tt);

	g_clear_object (&tt);

	return res;
}

struct _ECellDateEditValue {
	ICalTime *tt;
	ICalTimezone *zone;
};

ECellDateEditValue *
e_cell_date_edit_value_new (const ICalTime *tt,
			    const ICalTimezone *zone)
{
	g_return_val_if_fail (I_CAL_IS_TIME ((ICalTime *) tt), NULL);
	if (zone)
		g_return_val_if_fail (I_CAL_IS_TIMEZONE ((ICalTimezone *) zone), NULL);

	return e_cell_date_edit_value_new_take (i_cal_time_clone (tt),
		zone ? e_cal_util_copy_timezone (zone) : NULL);
}

ECellDateEditValue *
e_cell_date_edit_value_new_take (ICalTime *tt,
				 ICalTimezone *zone)
{
	ECellDateEditValue *value;

	g_return_val_if_fail (I_CAL_IS_TIME (tt), NULL);
	if (zone)
		g_return_val_if_fail (I_CAL_IS_TIMEZONE (zone), NULL);

	value = g_new0 (ECellDateEditValue, 1);
	value->tt = tt;
	value->zone = zone;

	return value;
}

ECellDateEditValue *
e_cell_date_edit_value_copy (const ECellDateEditValue *src)
{
	if (!src)
		return NULL;

	return e_cell_date_edit_value_new (src->tt, src->zone);
}

void
e_cell_date_edit_value_free (ECellDateEditValue *value)
{
	if (value) {
		g_clear_object (&value->tt);
		g_clear_object (&value->zone);
		g_free (value);
	}
}

ICalTime *
e_cell_date_edit_value_get_time (const ECellDateEditValue *value)
{
	g_return_val_if_fail (value != NULL, NULL);

	return value->tt;
}

void
e_cell_date_edit_value_set_time (ECellDateEditValue *value,
				 const ICalTime *tt)
{
	g_return_if_fail (value != NULL);
	g_return_if_fail (I_CAL_IS_TIME ((ICalTime *) tt));

	e_cell_date_edit_value_take_time (value, i_cal_time_clone (tt));
}

void
e_cell_date_edit_value_take_time (ECellDateEditValue *value,
				  ICalTime *tt)
{
	g_return_if_fail (value != NULL);
	g_return_if_fail (I_CAL_IS_TIME (tt));

	if (value->tt != tt) {
		g_clear_object (&value->tt);
		value->tt = tt;
	} else {
		g_clear_object (&tt);
	}
}

ICalTimezone *
e_cell_date_edit_value_get_zone (const ECellDateEditValue *value)
{
	g_return_val_if_fail (value != NULL, NULL);

	return value->zone;
}

void
e_cell_date_edit_value_set_zone (ECellDateEditValue *value,
				 const ICalTimezone *zone)
{
	g_return_if_fail (value != NULL);
	if (zone)
		g_return_if_fail (I_CAL_IS_TIMEZONE ((ICalTimezone *) zone));

	e_cell_date_edit_value_take_zone (value, zone ? e_cal_util_copy_timezone (zone) : NULL);
}

void
e_cell_date_edit_value_take_zone (ECellDateEditValue *value,
				  ICalTimezone *zone)
{
	g_return_if_fail (value != NULL);
	if (zone)
		g_return_if_fail (I_CAL_IS_TIMEZONE (zone));

	if (zone != value->zone) {
		g_clear_object (&value->zone);
		value->zone = zone;
	} else {
		g_clear_object (&zone);
	}
}
