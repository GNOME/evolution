/*
 * SPDX-FileCopyrightText: (C) 2022 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <gtk/gtk.h>

#include "e-month-widget.h"

#define MAX_WEEKS 6

#define CSS_CLASS_SELECTED "emw-selected"

struct _EMonthWidgetPrivate {
	GtkCssProvider *css_provider;
	GtkGrid *grid;
	GDateMonth month;
	guint year;
	GDateWeekday week_start_day;
	gboolean show_week_numbers;
	gboolean show_day_names;

	gboolean calculating_min_day_size;
	gint min_day_size; /* used for a square size */
	guint button_press_day;
};

/* A "day label", whose minimum size is always square */

#define E_TYPE_MONTH_WIDGET_DAY_LABEL (e_month_widget_day_label_get_type ())

G_DECLARE_FINAL_TYPE (EMonthWidgetDayLabel, e_month_widget_day_label, E, MONTH_WIDGET_DAY_LABEL, GtkLabel)

struct _EMonthWidgetDayLabel
{
	GtkLabel parent_instance;

	EMonthWidget *month_widget;
	guint day;
};

G_DEFINE_TYPE (EMonthWidgetDayLabel, e_month_widget_day_label, GTK_TYPE_LABEL)

static GtkSizeRequestMode
e_month_widget_day_label_get_request_mode (GtkWidget *widget)
{
	EMonthWidgetDayLabel *self = E_MONTH_WIDGET_DAY_LABEL (widget);

	if (self->month_widget->priv->calculating_min_day_size)
		return GTK_WIDGET_CLASS (e_month_widget_day_label_parent_class)->get_request_mode (widget);

	return GTK_SIZE_REQUEST_CONSTANT_SIZE;
}

static void
e_month_widget_day_label_get_preferred_height (GtkWidget *widget,
					       gint *minimum_height,
					       gint *natural_height)
{
	EMonthWidgetDayLabel *self = E_MONTH_WIDGET_DAY_LABEL (widget);

	if (self->month_widget->priv->calculating_min_day_size) {
		GTK_WIDGET_CLASS (e_month_widget_day_label_parent_class)->get_preferred_height (widget, minimum_height, natural_height);
		return;
	}

	if (minimum_height)
		*minimum_height = self->month_widget->priv->min_day_size;

	if (natural_height)
		*natural_height = self->month_widget->priv->min_day_size;
}

static void
e_month_widget_day_label_get_preferred_width (GtkWidget *widget,
					      gint *minimum_width,
					      gint *natural_width)
{
	EMonthWidgetDayLabel *self = E_MONTH_WIDGET_DAY_LABEL (widget);

	if (self->month_widget->priv->calculating_min_day_size) {
		GTK_WIDGET_CLASS (e_month_widget_day_label_parent_class)->get_preferred_width (widget, minimum_width, natural_width);
		return;
	}

	if (minimum_width)
		*minimum_width = self->month_widget->priv->min_day_size;

	if (natural_width)
		*natural_width = self->month_widget->priv->min_day_size;
}

static void
e_month_widget_day_label_class_init (EMonthWidgetDayLabelClass *klass)
{
	GtkWidgetClass *widget_class;

	widget_class = GTK_WIDGET_CLASS (klass);
	widget_class->get_request_mode = e_month_widget_day_label_get_request_mode;
	widget_class->get_preferred_height = e_month_widget_day_label_get_preferred_height;
	widget_class->get_preferred_width = e_month_widget_day_label_get_preferred_width;
}

static void
e_month_widget_day_label_init (EMonthWidgetDayLabel *self)
{
}

G_DEFINE_TYPE_WITH_PRIVATE (EMonthWidget, e_month_widget, GTK_TYPE_EVENT_BOX)

enum {
	PROP_0,
	PROP_WEEK_START_DAY,
	PROP_SHOW_WEEK_NUMBERS,
	PROP_SHOW_DAY_NAMES,
	LAST_PROP
};

enum {
	CHANGED,
	DAY_CLICKED,
	LAST_SIGNAL
};

static GParamSpec *obj_props[LAST_PROP] = { NULL, };
static guint signals[LAST_SIGNAL];

static const gchar *
get_digit_format (void)
{
#ifdef HAVE_GNU_GET_LIBC_VERSION
#include <gnu/libc-version.h>

	const gchar *libc_version = gnu_get_libc_version ();
	gchar **split = g_strsplit (libc_version, ".", -1);
	gint major = 0;
	gint minor = 0;
	gint revision = 0;

	major = atoi (split[0]);
	minor = atoi (split[1]);

	if (g_strv_length (split) > 2)
		revision = atoi (split[2]);
	g_strfreev (split);

	if (major > 2 || minor > 2 || (minor == 2 && revision > 2)) {
		return "%Id";
	}
#endif

	return "%d";
}

static void
e_month_widget_update (EMonthWidget *self)
{
	static const gchar *digit_format = NULL;
	GDate *date, tmp_date;
	GtkWidget *widget;
	gchar buffer[128];
	guint week_of_year, week_of_last_year = 0;
	guint ii, jj, month_day, max_month_days;

	if (!digit_format)
		digit_format = get_digit_format ();

	date = g_date_new_dmy (1, self->priv->month, self->priv->year);

	if (self->priv->week_start_day == G_DATE_SUNDAY) {
		week_of_year = g_date_get_sunday_week_of_year (date);
		if (!week_of_year)
			week_of_last_year = g_date_get_sunday_weeks_in_year (self->priv->year - 1);
	} else {
		week_of_year = g_date_get_monday_week_of_year (date);
		if (!week_of_year)
			week_of_last_year = g_date_get_monday_weeks_in_year (self->priv->year - 1);
	}

	/* Update week numbers */
	for (ii = 0; ii < MAX_WEEKS; ii++) {
		g_snprintf (buffer, sizeof (buffer), digit_format, !week_of_year ? week_of_last_year : week_of_year);

		widget = gtk_grid_get_child_at (self->priv->grid, 0, ii + 1);
		gtk_label_set_text (GTK_LABEL (widget), buffer);

		week_of_year++;
	}

	/* Update day names */
	tmp_date = *date;
	if (g_date_get_weekday (&tmp_date) > self->priv->week_start_day) {
		g_date_subtract_days (&tmp_date, g_date_get_weekday (&tmp_date) - self->priv->week_start_day);
	} else if (g_date_get_weekday (&tmp_date) < self->priv->week_start_day) {
		g_date_subtract_days (&tmp_date, 7 - (self->priv->week_start_day - g_date_get_weekday (&tmp_date)));
	}

	for (ii = 0; ii < 7; ii++) {
		g_warn_if_fail (g_date_strftime (buffer, sizeof (buffer), "%a", &tmp_date));
		g_date_add_days (&tmp_date, 1);

		widget = gtk_grid_get_child_at (self->priv->grid, ii + 1, 0);
		gtk_label_set_text (GTK_LABEL (widget), buffer);
	}

	g_date_subtract_days (&tmp_date, 7);

	/* Update days and weeks */
	month_day = 1;
	max_month_days = g_date_get_days_in_month (self->priv->month, self->priv->year);

	for (jj = 0; jj < MAX_WEEKS; jj++) {
		for (ii = 0; ii < 7; ii++) {
			EMonthWidgetDayLabel *day_label;

			widget = gtk_grid_get_child_at (self->priv->grid, ii + 1, jj + 1);
			day_label = E_MONTH_WIDGET_DAY_LABEL (widget);

			if (jj == 0 && g_date_compare (&tmp_date, date) < 0) {
				g_date_add_days (&tmp_date, 1);
				gtk_widget_set_visible (widget, FALSE);
				day_label->day = 0;
			} else if (month_day <= max_month_days) {
				g_snprintf (buffer, sizeof (buffer), digit_format, month_day);
				gtk_label_set_text (GTK_LABEL (widget), buffer);
				gtk_widget_set_visible (widget, TRUE);
				day_label->day = month_day;
				month_day++;

				if (ii == 0 && self->priv->show_week_numbers) {
					/* Show the week number */
					widget = gtk_grid_get_child_at (self->priv->grid, 0, jj + 1);
					gtk_widget_set_visible (widget, TRUE);
				}
			} else {
				gtk_widget_set_visible (widget, FALSE);
				day_label->day = 0;

				if (ii == 0 && self->priv->show_week_numbers) {
					/* Hide the week number */
					widget = gtk_grid_get_child_at (self->priv->grid, 0, jj + 1);
					gtk_widget_set_visible (widget, FALSE);
				}
			}
		}
	}

	g_date_free (date);
}

static GtkWidget *
e_month_widget_get_day_widget (EMonthWidget *self,
			       guint day)
{
	GtkWidget *widget;
	guint row, col, first_day;

	if (!day || day > g_date_get_days_in_month (self->priv->month, self->priv->year))
		return NULL;

	for (first_day = 0; first_day < 7; first_day++) {
		widget = gtk_grid_get_child_at (self->priv->grid, first_day + 1, 1);
		if (gtk_widget_get_visible (widget))
			break;
	}

	day--;

	row = day / 7;
	col = day % 7;

	if (col + first_day >= 7)
		row++;

	col = (col + first_day) % 7;

	widget = gtk_grid_get_child_at (self->priv->grid, col + 1, row + 1);
	g_warn_if_fail (gtk_widget_get_visible (widget));

	return widget;
}

static void
e_month_widget_style_updated (GtkWidget *widget)
{
	static const gchar *digit_format = NULL;
	EMonthWidget *self = E_MONTH_WIDGET (widget);
	GtkWidget *label_widget;
	GtkLabel *label;
	GDate *date;
	gchar buffer[128];
	gchar *previous_value;
	gboolean previous_visible;
	gint max_day_name_width = 0;
	gint max_week_num_height = 0;
	gint max_day_num_width = 0;
	gint max_day_num_height = 0;
	gint value;
	guint ii;

	if (!digit_format)
		digit_format = get_digit_format ();

	self->priv->calculating_min_day_size = TRUE;

	/* It does not matter what date it is, as it's used to get day names only */
	date = g_date_new_dmy (1, 1, 2000);

	/* Day name */
	label_widget = gtk_grid_get_child_at (self->priv->grid, 1, 0);
	label = GTK_LABEL (label_widget);
	previous_value = g_strdup (gtk_label_get_text (label));
	previous_visible = gtk_widget_get_visible (label_widget);
	gtk_widget_set_visible (label_widget, TRUE);

	for (ii = 0; ii < 7; ii++) {
		g_warn_if_fail (g_date_strftime (buffer, sizeof (buffer), "%a", date));
		g_date_add_days (date, 1);

		gtk_label_set_text (label, buffer);

		gtk_widget_get_preferred_width (label_widget, &value, NULL);
		if (value > max_day_name_width)
			max_day_name_width = value;
	}

	gtk_widget_set_visible (label_widget, previous_visible);
	gtk_label_set_text (label, previous_value);
	g_free (previous_value);
	g_date_free (date);

	/* Week number */
	label_widget = gtk_grid_get_child_at (self->priv->grid, 0, 1);
	label = GTK_LABEL (label_widget);
	previous_value = g_strdup (gtk_label_get_text (label));
	previous_visible = gtk_widget_get_visible (label_widget);
	gtk_widget_set_visible (label_widget, TRUE);

	for (ii = 1; ii < 54; ii++) {
		g_snprintf (buffer, sizeof (buffer), digit_format, ii);

		gtk_label_set_text (label, buffer);

		gtk_widget_get_preferred_height (label_widget, &value, NULL);
		if (value > max_week_num_height)
			max_week_num_height = value;
	}

	gtk_widget_set_visible (label_widget, previous_visible);
	gtk_label_set_text (label, previous_value);
	g_free (previous_value);

	/* Day number */
	label_widget = gtk_grid_get_child_at (self->priv->grid, 1, 1);
	label = GTK_LABEL (label_widget);
	previous_value = g_strdup (gtk_label_get_text (label));
	previous_visible = gtk_widget_get_visible (label_widget);
	gtk_widget_set_visible (label_widget, TRUE);

	for (ii = 1; ii < 32; ii++) {
		g_snprintf (buffer, sizeof (buffer), digit_format, ii);

		gtk_label_set_text (label, buffer);

		gtk_widget_get_preferred_width (label_widget, &value, NULL);
		if (value > max_day_num_width)
			max_day_num_width = value;

		gtk_widget_get_preferred_height (label_widget, &value, NULL);
		if (value > max_day_num_height)
			max_day_num_height = value;
	}

	gtk_widget_set_visible (label_widget, previous_visible);
	gtk_label_set_text (label, previous_value);
	g_free (previous_value);

	self->priv->calculating_min_day_size = FALSE;

	value = MAX (max_day_num_width, MAX (max_day_num_height, MAX (max_day_name_width, max_week_num_height)));

	/* Padding 2 pixels on each side */
	value += 4;

	if (value != self->priv->min_day_size) {
		self->priv->min_day_size = value;
		gtk_widget_queue_resize (widget);
	}
}

static void
e_month_widget_show_all (GtkWidget *widget)
{
	EMonthWidget *self = E_MONTH_WIDGET (widget);

	gtk_widget_show (widget);

	if (self->priv->grid)
		gtk_widget_show (GTK_WIDGET (self->priv->grid));
}

static gboolean
e_month_widget_button_press_event_cb (GtkWidget *widget,
				      GdkEventButton *event,
				      gpointer user_data)
{
	EMonthWidget *self = E_MONTH_WIDGET (widget);

	self->priv->button_press_day = event->type == GDK_BUTTON_PRESS ?
		e_month_widget_get_day_at_position (self, event->x, event->y) : 0;

	return FALSE;
}

static gboolean
e_month_widget_button_release_event_cb (GtkWidget *widget,
					GdkEventButton *event,
					gpointer user_data)
{
	EMonthWidget *self = E_MONTH_WIDGET (widget);
	guint day;

	day = event->type == GDK_BUTTON_RELEASE ?
		e_month_widget_get_day_at_position (self, event->x, event->y) : 0;

	if (day && self->priv->button_press_day == day) {
		g_signal_emit (self, signals[DAY_CLICKED], 0, event, self->priv->year, self->priv->month, day, NULL);
	}

	self->priv->button_press_day = 0;

	return FALSE;
}

static void
e_month_widget_set_property (GObject *object,
			     guint property_id,
			     const GValue *value,
			     GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_WEEK_START_DAY:
			e_month_widget_set_week_start_day (
				E_MONTH_WIDGET (object),
				g_value_get_int (value));
			return;

		case PROP_SHOW_WEEK_NUMBERS:
			e_month_widget_set_show_week_numbers (
				E_MONTH_WIDGET (object),
				g_value_get_boolean (value));
			return;

		case PROP_SHOW_DAY_NAMES:
			e_month_widget_set_show_day_names (
				E_MONTH_WIDGET (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_month_widget_get_property (GObject *object,
			     guint property_id,
			     GValue *value,
			     GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_WEEK_START_DAY:
			g_value_set_int (
				value, e_month_widget_get_week_start_day (
				E_MONTH_WIDGET (object)));
			return;

		case PROP_SHOW_WEEK_NUMBERS:
			g_value_set_boolean (
				value, e_month_widget_get_show_week_numbers (
				E_MONTH_WIDGET (object)));
			return;

		case PROP_SHOW_DAY_NAMES:
			g_value_set_boolean (
				value, e_month_widget_get_show_day_names (
				E_MONTH_WIDGET (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_month_widget_constructed (GObject *object)
{
	EMonthWidget *self = E_MONTH_WIDGET (object);
	PangoAttrList *attrs_small, *attrs_tnum, *attrs_small_tnum;
	GtkStyleProvider *style_provider;
	GtkStyleContext *style_context;
	guint ii, jj;
	GError *error = NULL;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_month_widget_parent_class)->constructed (object);

	g_object_set (object,
		"above-child", TRUE,
		"visible-window", TRUE,
		NULL);

	self->priv->grid = GTK_GRID (gtk_grid_new ());

	g_object_set (G_OBJECT (self->priv->grid),
		"column-homogeneous", FALSE,
		"column-spacing", 0,
		"row-homogeneous", FALSE,
		"row-spacing", 0,
		"visible", TRUE,
		NULL);

	gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (self->priv->grid));

	self->priv->css_provider = gtk_css_provider_new ();

	if (!gtk_css_provider_load_from_data (self->priv->css_provider,
		"EMonthWidget ." CSS_CLASS_SELECTED " {"
		"   background-color:@theme_selected_bg_color;"
		"   color:@theme_selected_fg_color;"
		"   border-radius:4px;"
		"   border-width:1px;"
		"   border-color:darker(@theme_selected_bg_color);"
		"   border-style:solid;"
		"}"
		"EMonthWidget .emw-day {"
		"   padding:1px;"
		"}"
		"EMonthWidget ." E_MONTH_WIDGET_CSS_CLASS_BOLD " {"
		"   font-weight:bold;"
		"}"
		"EMonthWidget ." E_MONTH_WIDGET_CSS_CLASS_ITALIC " {"
		"   font-style:italic;"
		"}"
		"EMonthWidget ." E_MONTH_WIDGET_CSS_CLASS_UNDERLINE " {"
		"   text-decoration:underline;"
		"}"
		"EMonthWidget ." E_MONTH_WIDGET_CSS_CLASS_HIGHLIGHT " {"
		"   border-radius:4px;"
		"   border-width:2px;"
		"   border-color:darker(@theme_selected_bg_color);"
		"   border-style:solid;"
		"}",
		-1, &error)) {
		g_warning ("%s: Failed to parse CSS: %s", G_STRFUNC, error ? error->message : "Unknown error");
		g_clear_error (&error);
	}

	style_provider = GTK_STYLE_PROVIDER (self->priv->css_provider);

	style_context = gtk_widget_get_style_context (GTK_WIDGET (self->priv->grid));
	gtk_style_context_add_provider (style_context, style_provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	gtk_style_context_add_class (style_context, GTK_STYLE_CLASS_VIEW);

	attrs_small = pango_attr_list_new ();
	pango_attr_list_insert (attrs_small, pango_attr_scale_new (PANGO_SCALE_SMALL));

	attrs_tnum = pango_attr_list_new ();
	pango_attr_list_insert_before (attrs_tnum, pango_attr_font_features_new ("tnum=1"));

	attrs_small_tnum = pango_attr_list_new ();
	pango_attr_list_insert (attrs_small_tnum, pango_attr_scale_new (PANGO_SCALE_SMALL));
	pango_attr_list_insert_before (attrs_small_tnum, pango_attr_font_features_new ("tnum=1"));

	for (jj = 0; jj < MAX_WEEKS + 1; jj++) {
		for (ii = 0; ii < 7 + 1; ii++) {
			GtkWidget *widget;
			PangoAttrList *attrs;

			if (!ii && !jj)
				continue;

			if (ii == 0)
				attrs = attrs_small_tnum;
			else if (jj == 0)
				attrs = attrs_small;
			else
				attrs = attrs_tnum;

			if (ii != 0 && jj != 0) {
				EMonthWidgetDayLabel *day_label;

				day_label = g_object_new (E_TYPE_MONTH_WIDGET_DAY_LABEL, NULL);
				day_label->month_widget = self;

				widget = GTK_WIDGET (day_label);
			} else {
				widget = gtk_label_new ("");
			}

			g_object_set (G_OBJECT (widget),
				"halign", GTK_ALIGN_FILL,
				"valign", GTK_ALIGN_FILL,
				"hexpand", ii != 0 && jj != 0,
				"vexpand", ii != 0 && jj != 0,
				"xalign", 0.5,
				"yalign", 0.5,
				"attributes", attrs,
				"visible", FALSE,
				"sensitive", ii != 0 && jj != 0,
				NULL);

			style_context = gtk_widget_get_style_context (widget);
			gtk_style_context_add_provider (style_context, style_provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

			if (ii == 0)
				gtk_style_context_add_class (style_context, "week-number");
			else if (jj == 0)
				gtk_style_context_add_class (style_context, "day-name");
			else
				gtk_style_context_add_class (style_context, "day-number");

			gtk_grid_attach (self->priv->grid, widget, ii, jj, 1, 1);
		}
	}

	e_month_widget_update (self);

	pango_attr_list_unref (attrs_small);
	pango_attr_list_unref (attrs_tnum);
	pango_attr_list_unref (attrs_small_tnum);

	g_signal_connect (self, "button-press-event",
		G_CALLBACK (e_month_widget_button_press_event_cb), NULL);

	g_signal_connect (self, "button-release-event",
		G_CALLBACK (e_month_widget_button_release_event_cb), NULL);
}

static void
e_month_widget_finalize (GObject *object)
{
	EMonthWidget *self = E_MONTH_WIDGET (object);

	g_clear_object (&self->priv->css_provider);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_month_widget_parent_class)->finalize (object);
}

static void
e_month_widget_class_init (EMonthWidgetClass *klass)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	widget_class = GTK_WIDGET_CLASS (klass);
	widget_class->style_updated = e_month_widget_style_updated;
	widget_class->show_all = e_month_widget_show_all;

	gtk_widget_class_set_accessible_role (widget_class, ATK_ROLE_CALENDAR);
	gtk_widget_class_set_css_name (widget_class, "EMonthWidget");

	object_class = G_OBJECT_CLASS (klass);
	object_class->get_property = e_month_widget_get_property;
	object_class->set_property = e_month_widget_set_property;
	object_class->constructed = e_month_widget_constructed;
	object_class->finalize = e_month_widget_finalize;

	/**
	 * EMonthWidget:week-start-day:
	 *
	 * A day the week starts with.
	 *
	 * Since: 3.46
	 **/
	obj_props[PROP_WEEK_START_DAY] =
		g_param_spec_int ("week-start-day", NULL, NULL,
			0, 7, G_DATE_SUNDAY,
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

	/**
	 * EMonthWidget:show-week-numbers:
	 *
	 * Whether to show week numbers.
	 *
	 * Since: 3.46
	 **/
	obj_props[PROP_SHOW_WEEK_NUMBERS] =
		g_param_spec_boolean ("show-week-numbers", NULL, NULL,
			FALSE,
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

	/**
	 * EMonthWidget:show-day-names:
	 *
	 * Whether to show day names.
	 *
	 * Since: 3.46
	 **/
	obj_props[PROP_SHOW_DAY_NAMES] =
		g_param_spec_boolean ("show-day-names", NULL, NULL,
			FALSE,
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

	g_object_class_install_properties (object_class, G_N_ELEMENTS (obj_props), obj_props);

	/**
	 * EMonthWidget::changed:
	 * @self: an #EMonthWidget, which sent the signal
	 *
	 * This signal is emitted when the shown date (month or year) changes.
	 *
	 * Since: 3.46
	 **/
	signals[CHANGED] = g_signal_new (
		"changed",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EMonthWidgetClass, changed),
		NULL, NULL, NULL,
		G_TYPE_NONE, 0,
		G_TYPE_NONE);

	/**
	 * EMonthWidget::day-clicked:
	 * @self: an #EMonthWidget, which sent the signal
	 * @event: a #GdkButtonEvent causing this signal; it's always a button release event
	 * @year: the year of the clicked day
	 * @month: the month of the clicked day
	 * @day: the day of the clicked day
	 *
	 * This signal is emitted when a day is clicked. It's identified
	 * as a date split into @year, @month and @day.
	 *
	 * Since: 3.46
	 **/
	signals[DAY_CLICKED] = g_signal_new (
		"day-clicked",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EMonthWidgetClass, day_clicked),
		NULL, NULL, NULL,
		G_TYPE_NONE, 4,
		GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE,
		G_TYPE_UINT,
		G_TYPE_INT,
		G_TYPE_UINT);
}

static void
e_month_widget_init (EMonthWidget *self)
{
	self->priv = e_month_widget_get_instance_private (self);
	self->priv->month = 1;
	self->priv->year = 2000;
	self->priv->week_start_day = G_DATE_SUNDAY;
	self->priv->show_week_numbers = FALSE;
	self->priv->show_day_names = FALSE;
}

/**
 * e_month_widget_new:
 *
 * Creates a new #EMonthWidget
 *
 * Returns: (transfer full): a new #EMonthWidget
 *
 * Since: 3.46
 **/
GtkWidget *
e_month_widget_new (void)
{
	return g_object_new (E_TYPE_MONTH_WIDGET, NULL);
}

/**
 * e_month_widget_set_month:
 * @self: an #EMonthWidget
 * @month: a month to show, as #GDateMonth
 * @year: a year to show
 *
 * Sets the @month of the @year to be shown in the @self.
 *
 * Since: 3.46
 **/
void
e_month_widget_set_month (EMonthWidget *self,
			  GDateMonth month,
			  guint year)
{
	g_return_if_fail (E_IS_MONTH_WIDGET (self));

	if (self->priv->month == month &&
	    self->priv->year == year)
		return;

	self->priv->month = month;
	self->priv->year = year;

	e_month_widget_update (self);

	g_signal_emit (self, signals[CHANGED], 0, NULL);
}

/**
 * e_month_widget_get_month:
 * @self: an #EMonthWidget
 * @out_month: (out) (optioal): an output location to set the shown month to, as #GDateMonth, or %NULL
 * @out_year: (out) (optional): an output location to set the shown year to, or %NULL
 *
 * Retrieve currently shown month and/or year in the @self.
 *
 * Since: 3.46
 **/
void
e_month_widget_get_month (EMonthWidget *self,
			  GDateMonth *out_month,
			  guint *out_year)
{
	g_return_if_fail (E_IS_MONTH_WIDGET (self));

	if (out_month)
		*out_month = self->priv->month;
	if (out_year)
		*out_year = self->priv->year;
}

/**
 * e_month_widget_set_week_start_day:
 * @self: an #EMonthWidget
 * @value: a #GDateWeekday
 *
 * Set which day of week the week starts on.
 *
 * Since: 3.46
 **/
void
e_month_widget_set_week_start_day (EMonthWidget *self,
				   GDateWeekday value)
{
	g_return_if_fail (E_IS_MONTH_WIDGET (self));
	g_return_if_fail (value != G_DATE_BAD_WEEKDAY);

	if (self->priv->week_start_day == value)
		return;

	self->priv->week_start_day = value;

	e_month_widget_update (self);

	g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_WEEK_START_DAY]);
}

/**
 * e_month_widget_get_week_start_day:
 * @self: an #EMonthWidget
 *
 * Returns: which day the week starts with
 *
 * Since: 3.46
 **/
GDateWeekday
e_month_widget_get_week_start_day (EMonthWidget *self)
{
	g_return_val_if_fail (E_IS_MONTH_WIDGET (self), G_DATE_BAD_WEEKDAY);

	return self->priv->week_start_day;
}

/**
 * e_month_widget_set_show_week_numbers:
 * @self: an #EMonthWidget
 * @value: whether to show week numbers
 *
 * Set whether to show the week numbers.
 *
 * Since: 3.46
 **/
void
e_month_widget_set_show_week_numbers (EMonthWidget *self,
				      gboolean value)
{
	guint ii;

	g_return_if_fail (E_IS_MONTH_WIDGET (self));

	if ((self->priv->show_week_numbers ? 1 : 0) == (value ? 1 : 0))
		return;

	self->priv->show_week_numbers = value;

	for (ii = 0; ii < MAX_WEEKS; ii++) {
		GtkWidget *week_number;
		gboolean should_show = self->priv->show_week_numbers;

		week_number = gtk_grid_get_child_at (self->priv->grid, 0, ii + 1);

		if (should_show) {
			guint jj;

			for (jj = 0; jj < 7; jj++) {
				GtkWidget *day_widget;

				day_widget = gtk_grid_get_child_at (self->priv->grid, jj + 1, ii + 1);

				if (gtk_widget_get_visible (day_widget))
					break;
			}

			/* Found a shown day in the week row */
			should_show = jj < 7;
		}

		gtk_widget_set_visible (week_number, should_show);
	}

	g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_SHOW_WEEK_NUMBERS]);
}

/**
 * e_month_widget_get_show_week_numbers:
 * @self: an #EMonthWidget
 *
 * Returns: whether week numbers are shown
 *
 * Since: 3.46
 **/
gboolean
e_month_widget_get_show_week_numbers (EMonthWidget *self)
{
	g_return_val_if_fail (E_IS_MONTH_WIDGET (self), FALSE);

	return self->priv->show_week_numbers;
}

/**
 * e_month_widget_set_show_day_names:
 * @self: an #EMonthWidget
 * @value: whether to show day names
 *
 * Set whether to show day names above the month days.
 *
 * Since: 3.46
 **/
void
e_month_widget_set_show_day_names (EMonthWidget *self,
				   gboolean value)
{
	guint ii;

	g_return_if_fail (E_IS_MONTH_WIDGET (self));

	if ((self->priv->show_day_names ? 1 : 0) == (value ? 1 : 0))
		return;

	self->priv->show_day_names = value;

	for (ii = 0; ii < 7; ii++) {
		GtkWidget *day_name;

		day_name = gtk_grid_get_child_at (self->priv->grid, ii + 1, 0);

		gtk_widget_set_visible (day_name, self->priv->show_day_names);
	}

	g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_SHOW_DAY_NAMES]);
}

/**
 * e_month_widget_get_show_day_names:
 * @self: an #EMonthWidget
 *
 * Returns: whether day names are shown.
 *
 * Since: 3.46
 **/
gboolean
e_month_widget_get_show_day_names (EMonthWidget *self)
{
	g_return_val_if_fail (E_IS_MONTH_WIDGET (self), FALSE);

	return self->priv->show_day_names;
}

/**
 * e_month_widget_set_day_selected:
 * @self: an #EMonthWidget
 * @day: a day of month
 * @selected: whether to select the day
 *
 * Sets the @day as @selected. There can be selected more
 * than one day.
 *
 * Using the @day out of range for the current month and year
 * leads to no change being done.
 *
 * Since: 3.46
 **/
void
e_month_widget_set_day_selected (EMonthWidget *self,
				 guint day,
				 gboolean selected)
{
	GtkWidget *day_widget;

	g_return_if_fail (E_IS_MONTH_WIDGET (self));

	day_widget = e_month_widget_get_day_widget (self, day);

	if (day_widget) {
		GtkStyleContext *style_context;

		style_context = gtk_widget_get_style_context (day_widget);

		if (selected)
			gtk_style_context_add_class (style_context, CSS_CLASS_SELECTED);
		else
			gtk_style_context_remove_class (style_context, CSS_CLASS_SELECTED);
	}
}

/**
 * e_month_widget_get_day_selected:
 * @self: an #EMonthWidget
 * @day: a day of month
 *
 * Returns whether the @day is selected. Using the @day out of range
 * for the current month and year always returns %FALSE.
 *
 * Returns: whether the @day is selected
 *
 * Since: 3.46
 **/
gboolean
e_month_widget_get_day_selected (EMonthWidget *self,
				 guint day)
{
	GtkWidget *day_widget;

	g_return_val_if_fail (E_IS_MONTH_WIDGET (self), FALSE);

	day_widget = e_month_widget_get_day_widget (self, day);

	if (day_widget) {
		GtkStyleContext *style_context;

		style_context = gtk_widget_get_style_context (day_widget);

		return gtk_style_context_has_class (style_context, CSS_CLASS_SELECTED);
	}

	return FALSE;
}

/**
 * e_month_widget_set_day_tooltip_markup:
 * @self: an #EMonthWidget
 * @day: a day of month
 * @tooltip_markup: (nullable): a tooltip to set, or %NULL to unset
 *
 * Sets a tooltip @tooltip_markup for the @day. The @tooltip_markup
 * is expected to be markup.
 *
 * The function does nothing when the @day is out of range.
 *
 * Since: 3.46
 **/
void
e_month_widget_set_day_tooltip_markup (EMonthWidget *self,
				       guint day,
				       const gchar *tooltip_markup)
{
	GtkWidget *day_widget;

	g_return_if_fail (E_IS_MONTH_WIDGET (self));

	day_widget = e_month_widget_get_day_widget (self, day);

	if (day_widget)
		gtk_widget_set_tooltip_markup (day_widget, tooltip_markup);
}

/**
 * e_month_widget_get_day_tooltip_markup:
 * @self: an #EMonthWidget
 * @day: a day of month
 *
 * Returns a tooltip markup for the @day, previously set by e_month_widget_set_day_tooltip_markup(),
 * or %NULL when none is set.
 *
 * The function returns %NULL when the @day is out of range.
 *
 * Returns: (transfer none) (nullable): a tooltip markup for the day, or %NULL
 *
 * Since: 3.46
 **/
const gchar *
e_month_widget_get_day_tooltip_markup (EMonthWidget *self,
				       guint day)
{
	GtkWidget *day_widget;

	g_return_val_if_fail (E_IS_MONTH_WIDGET (self), NULL);

	day_widget = e_month_widget_get_day_widget (self, day);

	if (day_widget)
		return gtk_widget_get_tooltip_markup (day_widget);

	return NULL;
}

/**
 * e_month_widget_clear_day_tooltips:
 * @self: an #EMonthWidget
 *
 * Clear tooltips for all days of the month.
 *
 * Since: 3.46
 **/
void
e_month_widget_clear_day_tooltips (EMonthWidget *self)
{
	gint ii, jj;

	g_return_if_fail (E_IS_MONTH_WIDGET (self));

	for (ii = 0; ii < 7; ii++) {
		for (jj = 0; jj < MAX_WEEKS; jj++) {
			GtkWidget *widget;

			widget = gtk_grid_get_child_at (self->priv->grid, ii + 1, jj + 1);

			gtk_widget_set_tooltip_markup (widget, NULL);
		}
	}
}

/**
 * e_month_widget_add_day_css_class:
 * @self: an #EMonthWidget
 * @day: a day of month
 * @name: a CSS class name to add
 *
 * Add the CSS class @name for the @day.
 *
 * The function does nothing when the @day is out of range.
 *
 * Since: 3.46
 **/
void
e_month_widget_add_day_css_class (EMonthWidget *self,
				  guint day,
				  const gchar *name)
{
	GtkWidget *day_widget;

	g_return_if_fail (E_IS_MONTH_WIDGET (self));

	day_widget = e_month_widget_get_day_widget (self, day);

	if (day_widget) {
		GtkStyleContext *style_context;

		style_context = gtk_widget_get_style_context (day_widget);
		gtk_style_context_add_class (style_context, name);
	}
}

/**
 * e_month_widget_remove_day_css_class:
 * @self: an #EMonthWidget
 * @day: a day of month
 * @name: a CSS class name to remove
 *
 * Add the CSS class @name for the @day.
 *
 * The function does nothing when the @day is out of range.
 *
 * Since: 3.46
 **/
void
e_month_widget_remove_day_css_class (EMonthWidget *self,
				     guint day,
				     const gchar *name)
{
	GtkWidget *day_widget;

	g_return_if_fail (E_IS_MONTH_WIDGET (self));

	day_widget = e_month_widget_get_day_widget (self, day);

	if (day_widget) {
		GtkStyleContext *style_context;

		style_context = gtk_widget_get_style_context (day_widget);
		gtk_style_context_remove_class (style_context, name);
	}
}

/**
 * e_month_widget_clear_day_css_classes:
 * @self: an #EMonthWidget
 *
 * Clear CSS classes for all days of the month. Those considered are @E_MONTH_WIDGET_CSS_CLASS_BOLD,
 * @E_MONTH_WIDGET_CSS_CLASS_ITALIC, @E_MONTH_WIDGET_CSS_CLASS_UNDERLINE
 * and @E_MONTH_WIDGET_CSS_CLASS_HIGHLIGHT. The function also removes
 * selected state from the days, if set.
 *
 * Since: 3.46
 **/
void
e_month_widget_clear_day_css_classes (EMonthWidget *self)
{
	gint ii, jj;

	g_return_if_fail (E_IS_MONTH_WIDGET (self));

	for (ii = 0; ii < 7; ii++) {
		for (jj = 0; jj < MAX_WEEKS; jj++) {
			GtkWidget *day_widget;
			GtkStyleContext *style_context;

			day_widget = gtk_grid_get_child_at (self->priv->grid, ii + 1, jj + 1);
			style_context = gtk_widget_get_style_context (day_widget);

			gtk_style_context_remove_class (style_context, E_MONTH_WIDGET_CSS_CLASS_BOLD);
			gtk_style_context_remove_class (style_context, E_MONTH_WIDGET_CSS_CLASS_ITALIC);
			gtk_style_context_remove_class (style_context, E_MONTH_WIDGET_CSS_CLASS_UNDERLINE);
			gtk_style_context_remove_class (style_context, E_MONTH_WIDGET_CSS_CLASS_HIGHLIGHT);
			gtk_style_context_remove_class (style_context, CSS_CLASS_SELECTED);
		}
	}
}

/**
 * e_month_widget_get_day_at_position:
 * @self: an #EMonthWidget
 * @x_win: window x coordinate
 * @y_win: window y coordinate
 *
 * Returns the day of month above which the @x_win, @y_win is. The position
 * is in the @self widget coordinates. A value 0 is returned when the position
 * doesn't point into any day.
 *
 * Returns: the day of month the @x_win, @y_win points to, or 0 if not any day
 *
 * Since: 3.46
 **/
guint
e_month_widget_get_day_at_position (EMonthWidget *self,
				    gdouble x_win,
				    gdouble y_win)
{
	GtkAllocation allocation;
	GtkWidget *day_widget;
	gint ii, jj;

	g_return_val_if_fail (E_IS_MONTH_WIDGET (self), 0);

	gtk_widget_get_allocation (GTK_WIDGET (self), &allocation);

	if (x_win < 0 || x_win >= allocation.width ||
	    y_win < 0 || y_win >= allocation.height)
		return 0;

	for (jj = 0; jj < MAX_WEEKS; jj++) {
		for (ii = 0; ii < 7; ii++) {
			day_widget = gtk_grid_get_child_at (self->priv->grid, ii + 1, jj + 1);

			if (gtk_widget_is_visible (day_widget)) {
				gtk_widget_get_allocation (day_widget, &allocation);

				if (x_win >= allocation.x && x_win < allocation.x + allocation.width &&
				    y_win >= allocation.y && y_win < allocation.y + allocation.height) {
					EMonthWidgetDayLabel *day_label = E_MONTH_WIDGET_DAY_LABEL (day_widget);

					return day_label->day;
				}
			}
		}
	}

	return 0;
}
