/*
 * e-interval-chooser.c
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

#include <glib/gi18n-lib.h>

#include "e-util-enums.h"
#include "e-misc-utils.h"

#include "e-interval-chooser.h"

#define MINUTES_PER_HOUR	(60)
#define MINUTES_PER_DAY		(MINUTES_PER_HOUR * 24)

struct _EIntervalChooserPrivate {
	GtkComboBox *combo_box;		/* not referenced */
	GtkSpinButton *spin_button;	/* not referenced */
};

enum {
	PROP_0,
	PROP_INTERVAL_MINUTES
};

G_DEFINE_TYPE_WITH_PRIVATE (EIntervalChooser, e_interval_chooser, GTK_TYPE_BOX)

static void
interval_chooser_notify_interval (GObject *object)
{
	g_object_notify (object, "interval-minutes");
}

static void
interval_chooser_set_property (GObject *object,
                               guint property_id,
                               const GValue *value,
                               GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_INTERVAL_MINUTES:
			e_interval_chooser_set_interval_minutes (
				E_INTERVAL_CHOOSER (object),
				g_value_get_uint (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
interval_chooser_get_property (GObject *object,
                               guint property_id,
                               GValue *value,
                               GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_INTERVAL_MINUTES:
			g_value_set_uint (
				value,
				e_interval_chooser_get_interval_minutes (
				E_INTERVAL_CHOOSER (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_interval_chooser_class_init (EIntervalChooserClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = interval_chooser_set_property;
	object_class->get_property = interval_chooser_get_property;

	g_object_class_install_property (
		object_class,
		PROP_INTERVAL_MINUTES,
		g_param_spec_uint (
			"interval-minutes",
			"Interval in Minutes",
			"Refresh interval in minutes",
			0, G_MAXUINT, 60,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));
}

static void
e_interval_chooser_init (EIntervalChooser *chooser)
{
	GtkWidget *widget;

	chooser->priv = e_interval_chooser_get_instance_private (chooser);

	gtk_orientable_set_orientation (
		GTK_ORIENTABLE (chooser), GTK_ORIENTATION_HORIZONTAL);

	gtk_box_set_spacing (GTK_BOX (chooser), 6);

	widget = gtk_spin_button_new_with_range (0, G_MAXUINT, 1);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (widget), TRUE);
	gtk_spin_button_set_update_policy (
		GTK_SPIN_BUTTON (widget), GTK_UPDATE_IF_VALID);
	gtk_box_pack_start (GTK_BOX (chooser), widget, TRUE, TRUE, 0);
	chooser->priv->spin_button = GTK_SPIN_BUTTON (widget);
	gtk_widget_show (widget);

	e_signal_connect_notify_swapped (
		widget, "notify::value",
		G_CALLBACK (interval_chooser_notify_interval), chooser);

	widget = gtk_combo_box_text_new ();
	gtk_combo_box_text_append_text (
		GTK_COMBO_BOX_TEXT (widget), _("minutes"));
	gtk_combo_box_text_append_text (
		GTK_COMBO_BOX_TEXT (widget), _("hours"));
	gtk_combo_box_text_append_text (
		GTK_COMBO_BOX_TEXT (widget), _("days"));
	gtk_box_pack_start (GTK_BOX (chooser), widget, FALSE, FALSE, 0);
	chooser->priv->combo_box = GTK_COMBO_BOX (widget);
	gtk_widget_show (widget);

	e_signal_connect_notify_swapped (
		widget, "notify::active",
		G_CALLBACK (interval_chooser_notify_interval), chooser);
}

GtkWidget *
e_interval_chooser_new (void)
{
	return g_object_new (E_TYPE_INTERVAL_CHOOSER, NULL);
}

guint
e_interval_chooser_get_interval_minutes (EIntervalChooser *chooser)
{
	EDurationType units;
	gdouble interval_minutes;

	g_return_val_if_fail (E_IS_INTERVAL_CHOOSER (chooser), 0);

	units = gtk_combo_box_get_active (chooser->priv->combo_box);

	interval_minutes = gtk_spin_button_get_value (
		chooser->priv->spin_button);

	switch (units) {
		case E_DURATION_HOURS:
			interval_minutes *= MINUTES_PER_HOUR;
			break;
		case E_DURATION_DAYS:
			interval_minutes *= MINUTES_PER_DAY;
			break;
		default:
			break;
	}

	return (guint) interval_minutes;
}

void
e_interval_chooser_set_interval_minutes (EIntervalChooser *chooser,
                                         guint interval_minutes)
{
	EDurationType units;

	g_return_if_fail (E_IS_INTERVAL_CHOOSER (chooser));

	if (interval_minutes == 0) {
		units = E_DURATION_MINUTES;
	} else if (interval_minutes % MINUTES_PER_DAY == 0) {
		interval_minutes /= MINUTES_PER_DAY;
		units = E_DURATION_DAYS;
	} else if (interval_minutes % MINUTES_PER_HOUR == 0) {
		interval_minutes /= MINUTES_PER_HOUR;
		units = E_DURATION_HOURS;
	} else {
		units = E_DURATION_MINUTES;
	}

	g_object_freeze_notify (G_OBJECT (chooser));

	gtk_combo_box_set_active (chooser->priv->combo_box, units);

	gtk_spin_button_set_value (
		chooser->priv->spin_button, interval_minutes);

	g_object_thaw_notify (G_OBJECT (chooser));
}
