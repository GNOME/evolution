/* Evolution calendar - alarm notification dialog
 *
 * Copyright (C) 2000 Ximian, Inc.
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Author: Federico Mena-Quintero <federico@ximian.com>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <stdio.h>
#include <string.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkimage.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkspinbutton.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkwindow.h>
#include <libgnome/gnome-i18n.h>
#if 0 
#  include <libgnomeui/gnome-winhints.h>
#endif
#include <glade/glade.h>
#include <e-util/e-time-utils.h>
#include <libecal/e-cal-time-util.h>
#include "alarm-notify-dialog.h"
#include "config-data.h"
#include "util.h"
#include <e-util/e-icon-factory.h>


/* The useful contents of the alarm notify dialog */
typedef struct {
	GladeXML *xml;

	GtkWidget *dialog;
	GtkWidget *title;
	GtkWidget *snooze_time;
	GtkWidget *minutes_label;
	GtkWidget *description;
	GtkWidget *location;
	GtkWidget *start;
	GtkWidget *end;

	AlarmNotifyFunc func;
	gpointer func_data;
} AlarmNotify;

enum {
        AN_RESPONSE_EDIT = 0,
	AN_RESPONSE_SNOOZE = 1
};



static void
an_update_minutes_label (GtkSpinButton *sb, gpointer data)
{
	AlarmNotify *an;
	char *new_label;
	int snooze_timeout;

	an = (AlarmNotify *) data;

	snooze_timeout = gtk_spin_button_get_value_as_int (sb);
	new_label = g_strdup (ngettext ("minute", "minutes", snooze_timeout));
	gtk_label_set_text (GTK_LABEL (an->minutes_label), new_label);
	g_free (new_label);
}

/**
 * alarm_notify_dialog:
 * @trigger: Trigger time for the alarm.
 * @occur_start: Start of occurrence time for the event.
 * @occur_end: End of occurrence time for the event.
 * @vtype: Type of the component which corresponds to the alarm.
 * @summary: Short summary of the appointment
 * @description: Long description of the appointment
 * @location: Location of the appointment
 * @func: Function to be called when a dialog action is invoked.
 * @func_data: Closure data for @func.
 *
 * Runs the alarm notification dialog.  The specified @func will be used to
 * notify the client about result of the actions in the dialog.
 *
 * Return value: a pointer to the dialog structure if successful or NULL if an error occurs.
 **/
void
alarm_notify_dialog (time_t trigger, time_t occur_start, time_t occur_end,
		     ECalComponentVType vtype, const char *summary,
		     const char *description, const char *location,
		     AlarmNotifyFunc func, gpointer func_data)
{
	AlarmNotify *an;
	GtkWidget *image;
	icaltimezone *current_zone;
	char *title;
	char *start, *end;
	char *icon_path;
	GList *icon_list;
	int snooze_timeout;

	g_return_if_fail (trigger != -1);

	/* Only VEVENTs or VTODOs can have alarms */
	g_return_if_fail (vtype == E_CAL_COMPONENT_EVENT
			  || vtype == E_CAL_COMPONENT_TODO);
	g_return_if_fail (summary != NULL);
	g_return_if_fail (description != NULL);
	g_return_if_fail (location != NULL);
	g_return_if_fail (func != NULL);

	an = g_new0 (AlarmNotify, 1);

	an->func = func;
	an->func_data = func_data;

	an->xml = glade_xml_new (EVOLUTION_GLADEDIR "/alarm-notify.glade", NULL, NULL);
	if (!an->xml) {
		g_message ("alarm_notify_dialog(): Could not load the Glade XML file!");
		g_free (an);
		return;
	}

	an->dialog = glade_xml_get_widget (an->xml, "alarm-notify");
	an->title = glade_xml_get_widget (an->xml, "title-label");
	an->snooze_time = glade_xml_get_widget (an->xml, "snooze-time");
	an->minutes_label = glade_xml_get_widget (an->xml, "minutes-label");
	an->description = glade_xml_get_widget (an->xml, "description-label");
	an->location = glade_xml_get_widget (an->xml, "location-label");
	an->start = glade_xml_get_widget (an->xml, "start-label");
	an->end = glade_xml_get_widget (an->xml, "end-label");

	if (!(an->dialog && an->title && an->snooze_time
	      && an->description && an->location && an->start && an->end)) {
		g_message ("alarm_notify_dialog(): Could not find all widgets in Glade file!");
		g_object_unref (an->xml);
		g_free (an);
		return;
	}

	gtk_widget_realize (an->dialog);
	gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (an->dialog)->vbox), 0);
	gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (an->dialog)->action_area), 12);

	image = glade_xml_get_widget (an->xml, "alarm-image");
	icon_path = e_icon_factory_get_icon_filename ("stock_alarm", E_ICON_SIZE_DIALOG);
	gtk_image_set_from_file (GTK_IMAGE (image), icon_path);
	g_free (icon_path);

	/* Title */

	gtk_window_set_title (GTK_WINDOW (an->dialog), summary);

	/* Set the widget contents */

	title = g_strdup_printf ("<big><b>%s</b></big>", summary);
	gtk_label_set_markup (GTK_LABEL (an->title), title);
	g_free (title);

	gtk_label_set_text (GTK_LABEL (an->description), description);
	gtk_label_set_text (GTK_LABEL (an->location), location);

	/* Stringize the times */

	current_zone = config_data_get_timezone ();

	start = timet_to_str_with_zone (occur_start, current_zone);
	gtk_label_set_text (GTK_LABEL (an->start), start);

	end = timet_to_str_with_zone (occur_end, current_zone);
	gtk_label_set_text (GTK_LABEL (an->end), end);

	/* Set callback for updating the snooze "minutes" label */
	g_signal_connect (G_OBJECT (an->snooze_time), "value_changed",
			  G_CALLBACK (an_update_minutes_label), an);
	/* Run! */

	if (!GTK_WIDGET_REALIZED (an->dialog))
		gtk_widget_realize (an->dialog);

	icon_list = e_icon_factory_get_icon_list ("stock_alarm");
	if (icon_list) {
		gtk_window_set_icon_list (GTK_WINDOW (an->dialog), icon_list);
		g_list_foreach (icon_list, (GFunc) g_object_unref, NULL);
		g_list_free (icon_list);
	}

	switch (gtk_dialog_run (GTK_DIALOG (an->dialog))) {
	case AN_RESPONSE_EDIT:
	  (* an->func) (ALARM_NOTIFY_EDIT, -1, an->func_data);
	  break;
	case AN_RESPONSE_SNOOZE:
	  snooze_timeout = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (an->snooze_time));
	  (* an->func) (ALARM_NOTIFY_SNOOZE, snooze_timeout, an->func_data);
	  break;
	case GTK_RESPONSE_CLOSE:
	case GTK_RESPONSE_DELETE_EVENT:
	  break;
	}
	gtk_widget_destroy (an->dialog);
	
	g_object_unref (an->xml);
	g_free (an);
}
