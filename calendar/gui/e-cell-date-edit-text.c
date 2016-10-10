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

#define E_CELL_DATE_EDIT_TEXT_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_CELL_DATE_EDIT_TEXT, ECellDateEditTextPrivate))

struct _ECellDateEditTextPrivate {

	/* The timezone to display the date in. */
	icaltimezone *timezone;

	/* Whether to display in 24-hour format. */
	gboolean use_24_hour_format;
};

enum {
	PROP_0,
	PROP_TIMEZONE,
	PROP_USE_24_HOUR_FORMAT
};

G_DEFINE_TYPE (
	ECellDateEditText,
	e_cell_date_edit_text,
	E_TYPE_CELL_TEXT)

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
				g_value_get_pointer (value));
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
			g_value_set_pointer (
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

static gchar *
cell_date_edit_text_get_text (ECellText *cell,
                              ETableModel *model,
                              gint col,
                              gint row)
{
	ECellDateEditText *ecd = E_CELL_DATE_EDIT_TEXT (cell);
	ECellDateEditValue *dv = e_table_model_value_at (model, col, row);
	icaltimezone *timezone;
	struct tm tmp_tm;
	gchar *res;

	if (!dv)
		return g_strdup ("");

	timezone = e_cell_date_edit_text_get_timezone (ecd);

	/* Note that although the property may be in a different
	 * timezone, we convert it to the current timezone to display
	 * it in the table. If the user actually edits the value,
	 * it will be set to the current timezone. See set_value (). */
	tmp_tm = icaltimetype_to_tm_with_zone (&dv->tt, dv->zone, timezone);

	res = e_datetime_format_format_tm (
		"calendar", "table", dv->tt.is_date ?
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
	ECellDateEditValue dv;
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
		dv.tt = icaltime_null_time ();

		dv.tt.year = tmp_tm.tm_year + 1900;
		dv.tt.month = tmp_tm.tm_mon + 1;
		dv.tt.day = tmp_tm.tm_mday;
		dv.tt.hour = tmp_tm.tm_hour;
		dv.tt.minute = tmp_tm.tm_min;
		dv.tt.second = tmp_tm.tm_sec;
		dv.tt.is_date = is_date;

		/* FIXME: We assume it is being set to the current timezone.
		 * Is that OK? */
		if (is_date) {
			dv.zone = NULL;
		} else {
			dv.zone = e_cell_date_edit_text_get_timezone (ecd);
		}

		value = &dv;
	}

	e_table_model_set_value_at (model, col, row, value);
}

static void
e_cell_date_edit_text_class_init (ECellDateEditTextClass *class)
{
	GObjectClass *object_class;
	ECellTextClass *cell_text_class;

	g_type_class_add_private (class, sizeof (ECellDateEditTextPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = cell_date_edit_text_set_property;
	object_class->get_property = cell_date_edit_text_get_property;

	cell_text_class = E_CELL_TEXT_CLASS (class);
	cell_text_class->get_text = cell_date_edit_text_get_text;
	cell_text_class->free_text = cell_date_edit_text_free_text;
	cell_text_class->set_value = cell_date_edit_text_set_value;

	g_object_class_install_property (
		object_class,
		PROP_TIMEZONE,
		g_param_spec_pointer (
			"timezone",
			"Time Zone",
			NULL,
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
	ecd->priv = E_CELL_DATE_EDIT_TEXT_GET_PRIVATE (ecd);

	ecd->priv->timezone = icaltimezone_get_utc_timezone ();
	ecd->priv->use_24_hour_format = TRUE;
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

icaltimezone *
e_cell_date_edit_text_get_timezone (ECellDateEditText *ecd)
{
	g_return_val_if_fail (E_IS_CELL_DATE_EDIT_TEXT (ecd), NULL);

	return ecd->priv->timezone;
}

void
e_cell_date_edit_text_set_timezone (ECellDateEditText *ecd,
                                    icaltimezone *timezone)
{
	g_return_if_fail (E_IS_CELL_DATE_EDIT_TEXT (ecd));

	if (ecd->priv->timezone == timezone)
		return;

	ecd->priv->timezone = timezone;

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
	struct icaltimetype tt;

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
	tt = dv2->tt;

	icaltimezone_convert_time (&tt, dv2->zone, dv1->zone);

	/* Now we can compare them. */
	return icaltime_compare (dv1->tt, tt);
}
