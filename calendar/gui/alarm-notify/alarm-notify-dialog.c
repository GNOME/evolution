/* Evolution calendar - alarm notification dialog
 *
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Author: Federico Mena-Quintero <federico@helixcode.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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
#include <gnome.h>
#include <glade/glade.h>
#include "alarm-notify-dialog.h"



/* The useful contents of the alarm notify dialog */
typedef struct {
	GladeXML *xml;

	GtkWidget *dialog;
	GtkWidget *close;
	GtkWidget *snooze;
	GtkWidget *edit;
	GtkWidget *heading;
	GtkWidget *summary;
	GtkWidget *snooze_time;

	AlarmNotifyFunc func;
	gpointer func_data;
} AlarmNotify;



/* Callback used when the notify dialog is destroyed */
static void
dialog_destroy_cb (GtkObject *object, gpointer data)
{
	AlarmNotify *an;

	an = data;
	gtk_object_unref (GTK_OBJECT (an->xml));
	g_free (an);
}

/* Delete_event handler for the alarm notify dialog */
static gint
delete_event_cb (GtkWidget *widget, GdkEvent *event, gpointer data)
{
	AlarmNotify *an;

	an = data;
	g_assert (an->func != NULL);

	(* an->func) (ALARM_NOTIFY_CLOSE, -1, an->func_data);

	gtk_widget_destroy (widget);
	return TRUE;
}

/* Callback for the close button */
static void
close_clicked_cb (GtkWidget *widget, gpointer data)
{
	AlarmNotify *an;

	an = data;
	g_assert (an->func != NULL);

	(* an->func) (ALARM_NOTIFY_CLOSE, -1, an->func_data);

	gtk_widget_destroy (an->dialog);
}

/* Callback for the snooze button */
static void
snooze_clicked_cb (GtkWidget *widget, gpointer data)
{
	AlarmNotify *an;
	int snooze_time;

	an = data;
	g_assert (an->func != NULL);

	snooze_time = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (an->snooze_time));
	(* an->func) (ALARM_NOTIFY_SNOOZE, snooze_time, an->func_data);

	gtk_widget_destroy (an->dialog);
}

/* Callback for the edit button */
static void
edit_clicked_cb (GtkWidget *widget, gpointer data)
{
	AlarmNotify *an;

	an = data;
	g_assert (an->func != NULL);

	(* an->func) (ALARM_NOTIFY_EDIT, -1, an->func_data);

	gtk_widget_destroy (an->dialog);
}

/**
 * alarm_notify_dialog:
 * @trigger: Trigger time for the alarm.
 * @occur: Occurrence time for the event.
 * @comp: Calendar component object which corresponds to the alarm.
 * @func: Function to be called when a dialog action is invoked.
 * @func_data: Closure data for @func.
 * 
 * Runs the alarm notification dialog.  The specified @func will be used to
 * notify the client about result of the actions in the dialog.
 * 
 * Return value: TRUE on success, FALSE if the dialog could not be created.
 **/
gboolean
alarm_notify_dialog (time_t trigger, time_t occur, CalComponent *comp,
		     AlarmNotifyFunc func, gpointer func_data)
{
	AlarmNotify *an;
	char buf[256];
	struct tm tm_trigger;
	struct tm tm_occur;
	CalComponentText summary;

	g_return_val_if_fail (trigger != -1, FALSE);
	g_return_val_if_fail (occur != -1, FALSE);
	g_return_val_if_fail (comp != NULL, FALSE);
	g_return_val_if_fail (IS_CAL_COMPONENT (comp), FALSE);
	g_return_val_if_fail (func != NULL, FALSE);

	an = g_new0 (AlarmNotify, 1);

	an->func = func;
	an->func_data = func_data;

	an->xml = glade_xml_new (EVOLUTION_GLADEDIR "/alarm-notify.glade", NULL);
	if (!an->xml) {
		g_message ("alarm_notify_dialog(): Could not load the Glade XML file!");
		g_free (an);
		return FALSE;
	}

	an->dialog = glade_xml_get_widget (an->xml, "alarm-notify");
	an->close = glade_xml_get_widget (an->xml, "close");
	an->snooze = glade_xml_get_widget (an->xml, "snooze");
	an->edit = glade_xml_get_widget (an->xml, "edit");
	an->heading = glade_xml_get_widget (an->xml, "heading");
	an->summary = glade_xml_get_widget (an->xml, "summary");
	an->snooze_time = glade_xml_get_widget (an->xml, "snooze-time");

	if (!(an->dialog && an->close && an->snooze && an->edit && an->heading && an->summary
	      && an->snooze_time)) {
		g_message ("alarm_notify_dialog(): Could not find all widgets in Glade file!");
		gtk_object_unref (GTK_OBJECT (an->xml));
		g_free (an);
		return FALSE;
	}

	gtk_object_set_data (GTK_OBJECT (an->dialog), "alarm-notify", an);
	gtk_signal_connect (GTK_OBJECT (an->dialog), "destroy",
			    GTK_SIGNAL_FUNC (dialog_destroy_cb), an);

	/* Title */

	/* FIXME: use am_pm_flag or 24-hour time */

	tm_trigger = *localtime (&trigger);
	strftime (buf, sizeof (buf), _("Alarm on %A %b %d %Y %H:%M"), &tm_trigger);
	gtk_window_set_title (GTK_WINDOW (an->dialog), buf);

	/* Heading */

	tm_occur = *localtime (&occur);
	strftime (buf, sizeof (buf),
		  _("Notification about your appointment on %A %b %d %Y %H:%M"),
		  &tm_occur);
	gtk_label_set_text (GTK_LABEL (an->heading), buf);

	/* Summary */

	cal_component_get_summary (comp, &summary);

	if (summary.value)
		gtk_label_set_text (GTK_LABEL (an->summary), summary.value);
	else
		gtk_label_set_text (GTK_LABEL (an->summary), _("No summary available."));

	/* Connect actions */

	gtk_signal_connect (GTK_OBJECT (an->dialog), "delete_event",
			    GTK_SIGNAL_FUNC (delete_event_cb),
			    an);
			    
	gtk_signal_connect (GTK_OBJECT (an->close), "clicked",
			    GTK_SIGNAL_FUNC (close_clicked_cb),
			    an);

	gtk_signal_connect (GTK_OBJECT (an->snooze), "clicked",
			    GTK_SIGNAL_FUNC (snooze_clicked_cb),
			    an);

	gtk_signal_connect (GTK_OBJECT (an->edit), "clicked",
			    GTK_SIGNAL_FUNC (edit_clicked_cb),
			    an);

	/* Run! */

	gtk_widget_show (an->dialog);
	return TRUE;
}
