/*
 * SPDX-FileCopyrightText: (C) 2025 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include <libedataserver/libedataserver.h>

#include "e-split-date-edit.h"

/**
 * SECTION: e-split-date-edit
 * @include: e-util/e-util.h
 * @short_description: edit date split into respective parts
 *
 * The #ESplitDateEdit allows to edit date with each part split.
 * Either of the year, month and day parts can be unset, which
 * is recognized by using a zero value for that part.
 **/

#define DAY_COMBO_COLUMNS 7

struct _ESplitDateEdit {
	GtkGrid parent;

	GtkEntry *year_entry;
	GtkComboBox *month_combo;
	GtkComboBox *day_combo;

	GtkWidget *popover;

	gulong year_change_handler;
	gulong month_change_handler;
	gulong day_change_handler;

	gchar *format;
	guint year;
	guint month;
	guint day;
};

G_DEFINE_TYPE (ESplitDateEdit, e_split_date_edit, GTK_TYPE_GRID)

enum {
	CHANGED,
	LAST_SIGNAL
};

static gint signals[LAST_SIGNAL];

static guint
e_split_date_edit_extract_combo_value (GtkComboBox *combo,
				       guint max_value)
{
	const gchar *text;
	gint active;

	active = gtk_combo_box_get_active (combo);

	if (active == 0)
		return 0;

	if (active == -1) {
		GtkWidget *entry;
		guint value;

		entry = gtk_bin_get_child (GTK_BIN (combo));

		g_warn_if_fail (GTK_IS_ENTRY (entry));

		if (!GTK_IS_ENTRY (entry))
			return 0;

		text = gtk_entry_get_text (GTK_ENTRY (entry));
		if (!text || !*text)
			return 0;

		value = (guint) g_ascii_strtoull (text, NULL, 10);
		if (value > max_value)
			return 0;

		return value;
	}

	text = gtk_combo_box_get_active_id (combo);
	g_warn_if_fail (text != NULL);

	if (!text || !*text)
		return 0;

	/* these cannot be out of bounds */
	return (guint) g_ascii_strtoull (text, NULL, 10);
}

static void
e_split_date_edit_reorder_parts (ESplitDateEdit *self)
{
	GtkContainer *container;
	GtkGrid *grid;
	const gchar *format;
	gchar *locale_format = NULL;
	guint ii, pos_index = 0;
	guint year_index = G_MAXUINT, month_index = G_MAXUINT, day_index = G_MAXUINT;
	gboolean get_from_locale = FALSE;

	if (!self->format)
		self->format = e_time_get_d_fmt_with_4digit_year ();

 again:
	format = locale_format ? locale_format : self->format;

	for (ii = 0; format && format[ii] && pos_index < 3; ii++) {
		if (format[ii] == '%') {
			if (format[ii + 1] == 'E' ||
			    format[ii + 1] == 'O' ||
			    format[ii + 1] == '-' ||
			    format[ii + 1] == '+' ||
			    format[ii + 1] == '0' ||
			    format[ii + 1] == ' ')
				ii++;
			switch (format[ii + 1]) {
			case '%':
				ii++;
				break;
			case 'y':
			case 'Y':
			case 'g':
			case 'G':
				if (year_index == G_MAXUINT) {
					year_index = pos_index;
					pos_index++;
				}
				break;
			case 'b' :
			case 'B':
			case 'h':
			case 'm':
				if (month_index == G_MAXUINT) {
					month_index = pos_index;
					pos_index++;
				}
				break;
			case 'd':
			case 'e':
				if (day_index == G_MAXUINT) {
					day_index = pos_index;
					pos_index++;
				}
				break;
			case 'D':
			case 'x':
				month_index = 0;
				day_index = 1;
				year_index = 2;
				pos_index = 3;
				get_from_locale = format[ii + 1] == 'x';
				break;
			case 'F':
				year_index = 0;
				month_index = 1;
				day_index = 2;
				pos_index = 3;
				break;
			}
		}
	}

	if (get_from_locale && pos_index == 0) {
		gchar *tmp = e_time_get_d_fmt_with_4digit_year ();

		if (tmp && !strchr (tmp, 'x')) {
			get_from_locale = FALSE;
			locale_format = tmp;
			goto again;
		}

		g_free (tmp);
	}

	if (year_index == G_MAXUINT) {
		year_index = pos_index;
		pos_index++;
	}

	if (month_index == G_MAXUINT) {
		month_index = pos_index;
		pos_index++;
	}

	if (day_index == G_MAXUINT) {
		day_index = pos_index;
		pos_index++;
	}

	g_object_ref (self->year_entry);
	g_object_ref (self->month_combo);
	g_object_ref (self->day_combo);

	container = GTK_CONTAINER (self);
	grid = GTK_GRID (self);

	gtk_container_remove (container, GTK_WIDGET (self->year_entry));
	gtk_container_remove (container, GTK_WIDGET (self->month_combo));
	gtk_container_remove (container, GTK_WIDGET (self->day_combo));

	gtk_grid_attach (grid, GTK_WIDGET (self->year_entry), year_index, 0, 1, 1);
	gtk_grid_attach (grid, GTK_WIDGET (self->month_combo), month_index, 0, 1, 1);
	gtk_grid_attach (grid, GTK_WIDGET (self->day_combo), day_index, 0, 1, 1);

	g_object_unref (self->year_entry);
	g_object_unref (self->month_combo);
	g_object_unref (self->day_combo);

	g_free (locale_format);
}

static void
e_split_date_edit_normalize (ESplitDateEdit *self)
{
	if (self->year > 9999)
		self->year = 0;
	if (self->month > 12)
		self->month = 0;
	if (self->day > 31)
		self->day = 0;

	if (self->day > 0 && (self->year > 0 || self->month > 0)) {
		guint max_days;

		if (self->year > 0 && self->month > 0) {
			max_days = g_date_get_days_in_month (self->month, self->year);
			if (self->day > max_days)
				self->day = max_days;
		} else if (self->month > 0) {
			/* use a leap year, when the year is not known */
			max_days = g_date_get_days_in_month (self->month, 2004);
			if (self->day > max_days)
				self->day = max_days;
		}
	}
}

static void
e_split_date_edit_update_content (ESplitDateEdit *self)
{
	gchar buff[128];

	g_signal_handler_block (self->year_entry, self->year_change_handler);
	g_signal_handler_block (self->month_combo, self->month_change_handler);
	g_signal_handler_block (self->day_combo, self->day_change_handler);

	if (self->year == 0 || self->year > 9999)
		buff[0] = '\0';
	else
		g_snprintf (buff, sizeof (buff) - 1, "%u", self->year);
	gtk_entry_set_text (self->year_entry, buff);

	gtk_combo_box_set_active (self->month_combo, self->month);

	if (self->day > 0) {
		g_snprintf (buff, sizeof (buff) - 1, "%u", self->day);
		gtk_combo_box_set_active_id (self->day_combo, buff);
	} else {
		gtk_combo_box_set_active_id (self->day_combo, "");
	}

	g_signal_handler_unblock (self->year_entry, self->year_change_handler);
	g_signal_handler_unblock (self->month_combo, self->month_change_handler);
	g_signal_handler_unblock (self->day_combo, self->day_change_handler);
}

static void
e_split_date_edit_fill_day_combo (ESplitDateEdit *self)
{
	GtkComboBoxText *text_combo = GTK_COMBO_BOX_TEXT (self->day_combo);
	guint ii, max_days;

	if (self->year != 0 && self->month != 0)
		max_days = g_date_get_days_in_month (self->month, self->year);
	else if (self->month != 0)
		max_days = g_date_get_days_in_month (self->month, 2004);
	else
		max_days = 31;

	g_signal_handler_block (self->day_combo, self->day_change_handler);

	gtk_combo_box_text_remove_all (text_combo);

	for (ii = 0; ii < max_days; ii++) {
		gchar buff[64];

		g_snprintf (buff, sizeof (buff), "%u", ii + 1);
		gtk_combo_box_text_append (text_combo, buff, buff);
	}

	gtk_combo_box_text_append (text_combo, "", _("Not set"));

	if (self->day > max_days)
		self->day = max_days;

	if (self->day > 0) {
		gchar buff[64];

		g_snprintf (buff, sizeof (buff) - 1, "%u", self->day);
		gtk_combo_box_set_active_id (self->day_combo, buff);
	} else {
		gtk_combo_box_set_active_id (self->day_combo, "");
	}

	g_signal_handler_unblock (self->day_combo, self->day_change_handler);
}

static void
e_split_date_edit_part_changed (GtkWidget *changed_widget,
				gpointer user_data)
{
	ESplitDateEdit *self = user_data;
	const gchar *text;
	guint year, month, day;

	g_return_if_fail (E_IS_SPLIT_DATE_EDIT (self));

	text = gtk_entry_get_text (self->year_entry);
	year = text ? (guint) g_ascii_strtoull (text, NULL, 10) : 0;
	month = e_split_date_edit_extract_combo_value (self->month_combo, 12);
	day = e_split_date_edit_extract_combo_value (self->day_combo, 31);

	e_split_date_edit_set_ymd (self, year, month, day);
}

static void
e_split_date_edit_day_selected_cb (GtkCalendar *calendar,
				   gpointer user_data)
{
	ESplitDateEdit *self = user_data;
	guint year = 0, month = 0, day = 0;

	gtk_calendar_get_date (calendar, &year, &month, &day);

	e_split_date_edit_set_ymd (self, year, month + 1, day);

	g_clear_pointer (&self->popover, gtk_widget_hide);
}

static void
e_split_date_edit_today_clicked_cb (GtkButton *button,
				    gpointer user_data)
{
	ESplitDateEdit *self = user_data;
	time_t now = time (NULL);
	const struct tm *tm = localtime (&now);

	if (tm)
		e_split_date_edit_set_ymd (self, tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
	else
		e_split_date_edit_set_ymd (self, 0, 0, 0);

	g_clear_pointer (&self->popover, gtk_widget_hide);
}

static void
e_split_date_edit_none_clicked_cb (GtkButton *button,
				   gpointer user_data)
{
	ESplitDateEdit *self = user_data;

	e_split_date_edit_set_ymd (self, 0, 0, 0);

	g_clear_pointer (&self->popover, gtk_widget_hide);
}

static void
e_split_date_edit_select_clicked_cb (GtkButton *button,
				     gpointer user_data)
{
	ESplitDateEdit *self = user_data;
	GtkWidget *widget;
	GtkGrid *grid;
	time_t now = time (NULL);
	struct tm *tm = localtime (&now);
	guint year, month, day;

	widget = gtk_grid_new ();
	grid = GTK_GRID (widget);

	widget = gtk_calendar_new ();
	gtk_widget_set_margin_bottom (widget, 6);

	year = self->year > 0 ? self->year : tm->tm_year + 1900;
	month = self->month > 0 ? self->month - 1 : tm->tm_mon;
	day = self->day > 0 ? self->day : tm->tm_mday;

	gtk_calendar_select_month (GTK_CALENDAR (widget), month, year);
	gtk_calendar_select_day (GTK_CALENDAR (widget), day);

	gtk_grid_attach (grid, widget, 0, 0, 2, 1);

	g_signal_connect_object (widget, "day-selected-double-click",
		G_CALLBACK (e_split_date_edit_day_selected_cb), self, 0);

	widget = gtk_button_new_with_mnemonic (_("_Today"));
	gtk_widget_set_halign (widget, GTK_ALIGN_START);
	gtk_grid_attach (grid, widget, 0, 1, 1, 1);

	g_signal_connect_object (widget, "clicked",
		G_CALLBACK (e_split_date_edit_today_clicked_cb), self, 0);

	widget = gtk_button_new_with_mnemonic (_("_None"));
	gtk_widget_set_halign (widget, GTK_ALIGN_END);
	gtk_grid_attach (grid, widget, 1, 1, 1, 1);

	g_signal_connect_object (widget, "clicked",
		G_CALLBACK (e_split_date_edit_none_clicked_cb), self, 0);

	gtk_widget_show_all (GTK_WIDGET (grid));

	widget = gtk_popover_new (GTK_WIDGET (button));
	gtk_popover_set_position (GTK_POPOVER (widget), GTK_POS_BOTTOM);
	gtk_container_add (GTK_CONTAINER (widget), GTK_WIDGET (grid));
	gtk_container_set_border_width (GTK_CONTAINER (widget), 6);

	self->popover = widget;
	g_object_weak_ref (G_OBJECT (self->popover), (GWeakNotify) g_nullify_pointer, &self->popover);

	g_signal_connect (widget, "closed",
		G_CALLBACK (gtk_widget_destroy), NULL);

	gtk_widget_show (widget);
}

static void
e_split_date_edit_grab_focus (GtkWidget *widget)
{
	GtkWidget *child;

	g_return_if_fail (E_IS_SPLIT_DATE_EDIT (widget));

	child = gtk_grid_get_child_at (GTK_GRID (widget), 0, 0);
	if (child)
		gtk_widget_grab_focus (child);
}

static gboolean
e_split_date_edit_mnemonic_activate (GtkWidget *widget,
				     gboolean group_cycling)
{
	e_split_date_edit_grab_focus (widget);
	return TRUE;
}

static void
e_split_date_edit_constructed (GObject *object)
{
	ESplitDateEdit *self = E_SPLIT_DATE_EDIT (object);
	GtkGrid *grid = GTK_GRID (self);
	GtkWidget *widget;
	GtkComboBoxText *text_combo;
	gchar buff[256];
	guint ii;

	G_OBJECT_CLASS (e_split_date_edit_parent_class)->constructed (object);

	widget = gtk_entry_new ();
	g_object_set (widget,
		"visible", TRUE,
		"width-chars", 5,
		"placeholder-text", _("Year"),
		"input-purpose", GTK_INPUT_PURPOSE_DIGITS,
		NULL);
	gtk_grid_attach (grid, widget, 0, 0, 1, 1);
	self->year_entry = GTK_ENTRY (widget);

	widget = gtk_combo_box_text_new ();
	g_object_set (widget,
		"visible", TRUE,
		NULL);
	gtk_grid_attach (grid, widget, 1, 0, 1, 1);
	self->month_combo = GTK_COMBO_BOX (widget);

	widget = gtk_combo_box_text_new ();
	g_object_set (widget,
		"visible", TRUE,
		"wrap-width", DAY_COMBO_COLUMNS,
		NULL);
	gtk_grid_attach (grid, widget, 2, 0, 1, 1);
	self->day_combo = GTK_COMBO_BOX (widget);

	widget = gtk_button_new_from_icon_name ("edit-find", GTK_ICON_SIZE_BUTTON);
	g_object_set (widget,
		"visible", TRUE,
		"margin-start", 4,
		"always-show-image", TRUE,
		"tooltip-text", _("Click to select date"),
		NULL);
	gtk_grid_attach (grid, widget, 3, 0, 1, 1);
	g_signal_connect (widget, "clicked",
		G_CALLBACK (e_split_date_edit_select_clicked_cb), self);

	text_combo = GTK_COMBO_BOX_TEXT (self->month_combo);
	gtk_combo_box_text_append_text (text_combo, _("Not set"));

	for (ii = 0; ii < 12; ii++) {
		gchar num_str[12];
		struct tm tm;
		gsize wrote;

		memset (&tm, 0, sizeof (struct tm));
		tm.tm_mday = 1;
		tm.tm_mon = ii;
		tm.tm_year = 2000;

		wrote = e_utf8_strftime (buff, sizeof (buff) - 1, "%B", &tm);

		if (wrote == 0 || wrote >= sizeof (buff) - 1)
			g_snprintf (buff, sizeof (buff) - 1, "%u", ii + 1);
		else
			buff[wrote] = '\0';

		g_snprintf (num_str, sizeof (num_str), "%u", ii + 1);

		gtk_combo_box_text_append (text_combo, num_str, buff);
	}

	gtk_combo_box_set_active (self->month_combo, 0);

	self->year_change_handler = g_signal_connect (self->year_entry, "changed",
		G_CALLBACK (e_split_date_edit_part_changed), self);

	self->month_change_handler = g_signal_connect (self->month_combo, "changed",
		G_CALLBACK (e_split_date_edit_part_changed), self);

	self->day_change_handler = g_signal_connect (self->day_combo, "changed",
		G_CALLBACK (e_split_date_edit_part_changed), self);

	e_split_date_edit_fill_day_combo (self);
	e_split_date_edit_reorder_parts (self);
}

static void
e_split_date_edit_dispose (GObject *object)
{
	ESplitDateEdit *self = E_SPLIT_DATE_EDIT (object);

	g_clear_pointer (&self->popover, gtk_widget_destroy);

	G_OBJECT_CLASS (e_split_date_edit_parent_class)->dispose (object);
}

static void
e_split_date_edit_finalize (GObject *object)
{
	ESplitDateEdit *self = E_SPLIT_DATE_EDIT (object);

	g_clear_pointer (&self->format, g_free);

	G_OBJECT_CLASS (e_split_date_edit_parent_class)->finalize (object);
}

static void
e_split_date_edit_class_init (ESplitDateEditClass *klass)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->constructed = e_split_date_edit_constructed;
	object_class->dispose = e_split_date_edit_dispose;
	object_class->finalize = e_split_date_edit_finalize;

	widget_class = GTK_WIDGET_CLASS (klass);
	widget_class->grab_focus = e_split_date_edit_grab_focus;
	widget_class->mnemonic_activate = e_split_date_edit_mnemonic_activate;

	signals[CHANGED] = g_signal_new (
		"changed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST,
		0,
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
e_split_date_edit_init (ESplitDateEdit *self)
{
}

/**
 * e_split_date_edit_new:
 *
 * Creates a new #ESplitDateEdit.
 *
 * Returns: (transfer full): a new #ESplitDateEdit
 *
 * Since: 3.60
 **/
GtkWidget *
e_split_date_edit_new (void)
{
	return g_object_new (E_TYPE_SPLIT_DATE_EDIT, NULL);
}

/**
 * e_split_date_edit_set_format:
 * @self: an #ESplitDateEdit
 * @format: (nullable): a date format to use, or %NULL to unset
 *
 * Sets the date format to be used to place the year, month
 * and day widgets in the given order. The format is expected
 * to be as for the strftime. Parts which cannot be recognized
 * in the string are added at the end in an order year, month
 * and then day. When a %NULL @format is used, the actual
 * format is tried to be recognized from the current locale.
 *
 * Since: 3.60
 **/
void
e_split_date_edit_set_format (ESplitDateEdit *self,
			      const gchar *format)
{
	g_return_if_fail (E_IS_SPLIT_DATE_EDIT (self));

	if (g_strcmp0 (self->format, format) != 0) {
		g_free (self->format);
		self->format = g_strdup (format);

		e_split_date_edit_reorder_parts (self);
	}
}

/**
 * e_split_date_edit_get_format:
 * @self: an #ESplitDateEdit
 *
 * Returns previously set format for the parts order by
 * the e_split_date_edit_set_format(). The %NULL means
 * to use the format from the current locale.
 *
 * Returns: previously set format by the e_split_date_edit_set_format()
 *
 * Since: 3.60
 **/
const gchar *
e_split_date_edit_get_format (ESplitDateEdit *self)
{
	g_return_val_if_fail (E_IS_SPLIT_DATE_EDIT (self), NULL);

	return self->format;
}

/**
 * e_split_date_edit_set_ymd:
 * @self: an #ESplitDateEdit
 * @year: year part of the date
 * @month: month part of the date
 * @day: day part of the date
 *
 * Sets the date shown in the @self. Either of the parts can
 * be zero (or otherwise out of bounds) to indicate that part
 * is not set.
 *
 * Since: 3.60
 **/
void
e_split_date_edit_set_ymd (ESplitDateEdit *self,
			   guint year,
			   guint month,
			   guint day)
{
	g_return_if_fail (E_IS_SPLIT_DATE_EDIT (self));

	if (self->year != year || self->month != month || self->day != day) {
		guint had_year, had_month;

		had_year = self->year;
		had_month = self->month;

		self->year = year;
		self->month = month;
		self->day = day;

		e_split_date_edit_normalize (self);

		/* may days could change */
		if (had_year != self->year || had_month != self->month)
			e_split_date_edit_fill_day_combo (self);

		e_split_date_edit_update_content (self);

		g_signal_emit (self, signals[CHANGED], 0);
	}
}

/**
 * e_split_date_edit_get_ymd:
 * @self: an #ESplitDateEdit
 * @out_year: (out): return location for the year part
 * @out_month: (out): return location for the month part
 * @out_day: (out): return location for the day part
 *
 * Returns the current set date in the @self. The unset
 * parts have set zero as their value.
 *
 * Since: 3.60
 **/
void
e_split_date_edit_get_ymd (ESplitDateEdit *self,
			   guint *out_year,
			   guint *out_month,
			   guint *out_day)
{
	g_return_if_fail (E_IS_SPLIT_DATE_EDIT (self));
	g_return_if_fail (out_year != NULL);
	g_return_if_fail (out_month != NULL);
	g_return_if_fail (out_day != NULL);

	*out_year = self->year;
	*out_month = self->month;
	*out_day = self->day;
}
