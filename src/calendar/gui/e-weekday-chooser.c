/*
 * e-weekday-chooser.c
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
 */

#include "evolution-config.h"

#include <string.h>

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include <gdk/gdkkeysyms.h>

#include <libgnomecanvas/libgnomecanvas.h>
#include <e-util/e-util.h>

#include "e-weekday-chooser.h"

#define PADDING 2

struct _EWeekdayChooser {
	GtkBox parent_instance;

	/* Day that defines the start of the week. */
	GDateWeekday week_start_day;

	GtkWidget *buttons[7];
};

enum {
	PROP_0,
	PROP_WEEK_START_DAY,
	N_PROPERTIES
};

enum {
	CHANGED,
	LAST_SIGNAL
};

static guint chooser_signals[LAST_SIGNAL];
static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

G_DEFINE_FINAL_TYPE_WITH_CODE (EWeekdayChooser, e_weekday_chooser, GTK_TYPE_BOX,
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL))

static void
on_button_toggled (EWeekdayChooser *chooser,
                   G_GNUC_UNUSED GtkToggleButton *button)
{
	g_signal_emit (chooser, chooser_signals[CHANGED], 0);
}

static void
weekday_chooser_set_property (GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_WEEK_START_DAY:
			e_weekday_chooser_set_week_start_day (
				E_WEEKDAY_CHOOSER (object),
				g_value_get_enum (value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
weekday_chooser_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_WEEK_START_DAY:
			g_value_set_enum (
				value,
				e_weekday_chooser_get_week_start_day (
				E_WEEKDAY_CHOOSER (object)));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
weekday_chooser_constructed (GObject *object)
{
	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_weekday_chooser_parent_class)->constructed (object);

	e_extensible_load_extensions (E_EXTENSIBLE (object));
}

static void
e_weekday_chooser_class_init (EWeekdayChooserClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = weekday_chooser_set_property;
	object_class->get_property = weekday_chooser_get_property;
	object_class->constructed = weekday_chooser_constructed;

	obj_properties[PROP_WEEK_START_DAY] =
		g_param_spec_enum (
			"week-start-day",
			"Week Start Day",
			NULL,
			E_TYPE_DATE_WEEKDAY,
			G_DATE_MONDAY,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS |
			G_PARAM_EXPLICIT_NOTIFY);

	g_object_class_install_properties (object_class,
		N_PROPERTIES,
		obj_properties);

	chooser_signals[CHANGED] = g_signal_new (
		"changed",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_FIRST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
e_weekday_chooser_init (EWeekdayChooser *chooser)
{
	guint ii;

	chooser->week_start_day = G_DATE_MONDAY;
	gtk_box_set_homogeneous (GTK_BOX (chooser), TRUE);
	gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (chooser)), "linked");
	for (ii = 0; ii < 7; ii++) {
		chooser->buttons[ii] = gtk_toggle_button_new_with_label (e_get_weekday_name (G_DATE_MONDAY + ii, TRUE));
		gtk_widget_set_visible (chooser->buttons[ii], TRUE);
		gtk_container_add (GTK_CONTAINER (chooser), chooser->buttons[ii]);
		g_signal_connect_swapped (
			chooser->buttons[ii],
			"toggled",
			G_CALLBACK (on_button_toggled),
			chooser
		);
	}
}

/**
 * e_weekday_chooser_new:
 *
 * Creates a new #EWeekdayChooser.
 *
 * Returns: an #EWeekdayChooser
 **/
GtkWidget *
e_weekday_chooser_new (void)
{
	return g_object_new (E_TYPE_WEEKDAY_CHOOSER, NULL);
}

/**
 * e_weekday_chooser_get_days:
 * @chooser: an #EWeekdayChooser
 * @weekday: a #GDateWeekday
 *
 * Returns whether @weekday is selected.
 *
 * Returns: whether @weekday is selected
 **/
gboolean
e_weekday_chooser_get_selected (EWeekdayChooser *chooser,
                                GDateWeekday weekday)
{
	g_return_val_if_fail (E_IS_WEEKDAY_CHOOSER (chooser), FALSE);
	g_return_val_if_fail (g_date_valid_weekday (weekday), FALSE);

	return gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (chooser->buttons[weekday - 1]));
}

/**
 * e_weekday_chooser_set_selected:
 * @chooser: an #EWeekdayChooser
 * @weekday: a #GDateWeekday
 * @selected: selected flag
 *
 * Selects or deselects @weekday.
 **/
void
e_weekday_chooser_set_selected (EWeekdayChooser *chooser,
                                GDateWeekday weekday,
                                gboolean selected)
{
	g_return_if_fail (E_IS_WEEKDAY_CHOOSER (chooser));
	g_return_if_fail (g_date_valid_weekday (weekday));

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (chooser->buttons[weekday - 1])) != selected) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (chooser->buttons[weekday - 1]), selected);

		g_signal_emit (chooser, chooser_signals[CHANGED], 0);
	}
}

/**
 * e_weekday_chooser_get_week_start_day:
 * @chooser: an #EWeekdayChooser
 *
 * Queries the day that defines the start of the week in @chooser.
 *
 * Returns: a #GDateWeekday
 **/
GDateWeekday
e_weekday_chooser_get_week_start_day (EWeekdayChooser *chooser)
{
	g_return_val_if_fail (E_IS_WEEKDAY_CHOOSER (chooser), G_DATE_BAD_WEEKDAY);

	return chooser->week_start_day;
}

/**
 * e_weekday_chooser_set_week_start_day:
 * @chooser: an #EWeekdayChooser
 * @week_start_day: a #GDateWeekday
 *
 * Sets the day that defines the start of the week for @chooser.
 **/
void
e_weekday_chooser_set_week_start_day (EWeekdayChooser *chooser,
                                      GDateWeekday week_start_day)
{
	gint ii;

	g_return_if_fail (E_IS_WEEKDAY_CHOOSER (chooser));
	g_return_if_fail (g_date_valid_weekday (week_start_day));

	if (week_start_day == chooser->week_start_day)
		return;

	chooser->week_start_day = week_start_day;
	for (ii = 0; ii < 7; ii++) {
		gtk_container_child_set (GTK_CONTAINER (chooser), chooser->buttons[week_start_day - 1], "position", ii, NULL);
		week_start_day = e_weekday_get_next (week_start_day);
	}

	g_object_notify_by_pspec (G_OBJECT (chooser), obj_properties[PROP_WEEK_START_DAY]);
}

