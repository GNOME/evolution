/* Evolution calendar - Meeting editor dialog
 *
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Authors: Jesse Pavel <jpavel@helixcode.com>
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
#include <icaltypes.h>
#include <ical.h>
#include <widgets/meeting-time-sel/e-meeting-time-sel.h>
#include <Evolution-Composer.h>
#include <string.h>
#include "e-meeting-edit.h"

#define E_MEETING_GLADE_XML "e-meeting-dialog.glade"


typedef struct _EMeetingEditorPrivate EMeetingEditorPrivate;

struct _EMeetingEditorPrivate {
	/* These are the widgets to be used in the GUI. */
	GladeXML *xml;
	GtkWidget *meeting_window;
	GtkWidget *attendee_list;
	GtkWidget *address_entry;
	GtkWidget *edit_dialog;
	GtkWidget *organizer_entry;
	GtkWidget *role_entry;
	GtkWidget *rsvp_check;
	GtkWidget *send_button, *schedule_button;
	
	gint changed_signal_id;

	/* Various pieces of information. */
	gint selected_row;
	CalComponent *comp;
	CalClient *client;
	icalcomponent *icalcomp, *vevent;
	EventEditor *ee;

	gint numentries;  /* How many attendees are there? */
	gboolean dirty;  /* Has anything changed? */
}; 

#define NUM_COLUMNS 4  /* The number of columns in our attendee list. */

enum column_names {ADDRESS_COL, ROLE_COL, RSVP_COL, STATUS_COL};

static gchar *partstat_values[] = {
	"Needs action",
	"Accepted",
	"Declined",
	"Tentative",
	"Delegated",
	"Completed",
	"In Progress",
	"Unknown"
};

static gchar *role_values[] = {
	"Chair",
	"Required Participant",
	"Optional Participant",
	"Non-Participant",
	"Other"
};


/* Note that I have to iterate and check myself because
   ical_property_get_xxx_parameter doesn't take into account the
   kind of parameter for which you wish to search! */
static icalparameter *
get_icalparam_by_type (icalproperty *prop, icalparameter_kind kind)
{
	icalparameter *param;

	for (param = icalproperty_get_first_parameter (prop, ICAL_ANY_PARAMETER);
	     param != NULL && icalparameter_isa (param) != kind;
	     param = icalproperty_get_next_parameter (prop, ICAL_ANY_PARAMETER) );

	return param;
}


static gboolean
window_delete_cb (GtkWidget *widget,
		  GdkEvent *event,
		  gpointer data)
{	
	EMeetingEditorPrivate *priv;
	gchar *text;

	priv = (EMeetingEditorPrivate *) ((EMeetingEditor *)data)->priv;

	if (priv->dirty == TRUE) {
		/* FIXME: notify the event editor that our data has changed. 
			For now, I'll just display a dialog box. */
		{
			GtkWidget *dialog;
			icalproperty *prop;
			icalvalue *value;

			/* Save the organizer into the iCAL object. */
        		prop = icalcomponent_get_first_property (priv->vevent, ICAL_ORGANIZER_PROPERTY);

			text = gtk_entry_get_text (GTK_ENTRY (priv->organizer_entry));
			if (strlen (text) > 0) {
				gchar buffer[200];
				g_snprintf (buffer, 190, "MAILTO:%s", text);
				
				if (prop == NULL) {
					/* We need to add an ORGANIZER property. */
					prop = icalproperty_new (ICAL_ORGANIZER_PROPERTY);
					icalcomponent_add_property (priv->vevent, prop);
				}
				value = icalvalue_new_text (buffer);
				icalproperty_set_value (prop, value);
			}
		
			dialog = gnome_warning_dialog_parented ("Note that the meeting has changed,\n"
								"and you should save this event.",
								GTK_WINDOW (priv->meeting_window));
			gnome_dialog_run (GNOME_DIALOG(dialog));
		}
	}

	gtk_entry_set_text (GTK_ENTRY (priv->organizer_entry), "");

	return (FALSE);
}

static void 
window_destroy_cb (GtkWidget *widget,
		   gpointer data)
{
	EMeetingEditorPrivate *priv;

	priv = (EMeetingEditorPrivate *) ((EMeetingEditor *)data)->priv;

	gtk_main_quit ();
	return;
}

/* put_property_in_list() synchronizes the display of row `rownum'
   in our attendee list to the values of `prop'. If rownum < 0,
   then put_property_in_list() will append a new row. 
   If the property doesn't contain certain parameters that we deem
   necessary, it will add them. */
static void
put_property_in_list (icalproperty *prop, gint rownum, gpointer data)
{
	gchar *row_text[NUM_COLUMNS];
	gchar *text, *new_text;
	icalparameter *param;
	icalvalue *value;
	gint enumval;
	gint cntr;

	EMeetingEditorPrivate *priv;

	priv = (EMeetingEditorPrivate *) ((EMeetingEditor *)data)->priv;
	
	value = icalproperty_get_value (prop);

	if (value != NULL) {
		text = strdup (icalvalue_as_ical_string (value));
	
		/* Here I strip off the "MAILTO:" if it is present. */
		new_text = strchr (text, ':');
		if (new_text != NULL)
			new_text++;
		else
			new_text = text;
	
		row_text[ADDRESS_COL] = g_strdup (new_text);
		g_free (text);
	}

	param = get_icalparam_by_type (prop, ICAL_ROLE_PARAMETER);
	if (param == NULL) {
		param = icalparameter_new_role (ICAL_ROLE_REQPARTICIPANT);
		icalproperty_add_parameter (prop, param);
	}
		
	enumval = icalparameter_get_role (param);
	if (enumval < 0 || enumval > 4)
		enumval = 4;
	
	row_text[ROLE_COL] = role_values [enumval];

	param = get_icalparam_by_type (prop, ICAL_RSVP_PARAMETER);
	if (param == NULL) {
		param = icalparameter_new_rsvp (TRUE);
		icalproperty_add_parameter (prop, param);
	}

	if (icalparameter_get_rsvp (param))
		row_text[RSVP_COL] = "Y";
	else
		row_text[RSVP_COL] = "N";

	param = get_icalparam_by_type (prop, ICAL_PARTSTAT_PARAMETER);
	if (param == NULL) {
		param = icalparameter_new_partstat (ICAL_PARTSTAT_NEEDSACTION);
		icalproperty_add_parameter (prop, param);
	}

	enumval = icalparameter_get_partstat (param);
	if (enumval < 0 || enumval > 7) {
		enumval = 7;
	}

	row_text[STATUS_COL] = partstat_values [enumval];

	if (rownum < 0) {
		gtk_clist_append (GTK_CLIST (priv->attendee_list), row_text);
		gtk_clist_set_row_data (GTK_CLIST (priv->attendee_list), priv->numentries, prop);
		priv->numentries++;
	}
	else {
		for (cntr = 0; cntr < NUM_COLUMNS; cntr++) {
			gtk_clist_set_text (GTK_CLIST (priv->attendee_list), 
					    rownum,
					    cntr,
					    row_text[cntr]);
		}
	}

	g_free (row_text[ADDRESS_COL]);
}

	

/********
 * edit_attendee() performs the GUI manipulation and interaction for
 * editing `prop' and returns TRUE if the user indicated that he wants
 * to save the new property information.
 * 
 * Note that it is necessary that the property have parameters of the types
 * RSVP, PARTSTAT, and ROLE already when passed into this function. 
 ********/
static gboolean
edit_attendee (icalproperty *prop, gpointer data)
{
	EMeetingEditorPrivate *priv;
	gint button_num;
	gchar *new_text, *text;
	icalparameter *param;
	icalvalue *value;
	gchar buffer[200];
	gint cntr;
	gint enumval;
	gboolean retval;

	priv = (EMeetingEditorPrivate *) ((EMeetingEditor *)data)->priv;

	g_return_val_if_fail (prop != NULL, FALSE);

	if (priv->edit_dialog == NULL || priv->address_entry == NULL) {
		priv->edit_dialog = glade_xml_get_widget (priv->xml, "edit_dialog");
		priv->address_entry = glade_xml_get_widget (priv->xml, "address_entry");

		gnome_dialog_set_close (GNOME_DIALOG (priv->edit_dialog), TRUE);
		gnome_dialog_editable_enters (GNOME_DIALOG (priv->edit_dialog), 
					      GTK_EDITABLE (priv->address_entry));
		gnome_dialog_close_hides (GNOME_DIALOG (priv->edit_dialog), TRUE);
		gnome_dialog_set_default (GNOME_DIALOG (priv->edit_dialog), 0);
	}

	g_return_val_if_fail (priv->edit_dialog != NULL, FALSE);
	g_return_val_if_fail (priv->address_entry != NULL, FALSE);

	gtk_widget_realize (priv->edit_dialog);
	
	value = icalproperty_get_value (prop);

	if (value != NULL) {
		text = strdup (icalvalue_as_ical_string (value));
	
		/* Here I strip off the "MAILTO:" if it is present. */
		new_text = strchr (text, ':');
		if (new_text != NULL)
			new_text++;
		else
			new_text = text;
	
		gtk_entry_set_text (GTK_ENTRY (priv->address_entry), new_text);
		g_free (text);
	}
	else {
		gtk_entry_set_text (GTK_ENTRY (priv->address_entry), "");
	}
				

	param = get_icalparam_by_type (prop, ICAL_ROLE_PARAMETER);
	enumval = icalparameter_get_role (param);
	if (enumval < 0 || enumval > 4)
		enumval = 4;
	
	text = role_values [enumval];
	gtk_entry_set_text (GTK_ENTRY (priv->role_entry), text);

	param = get_icalparam_by_type (prop, ICAL_RSVP_PARAMETER);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->rsvp_check),
					icalparameter_get_rsvp (param));

	gtk_widget_show (priv->edit_dialog);

	button_num = gnome_dialog_run (GNOME_DIALOG (priv->edit_dialog));

	if (button_num == 0) {
		/* The user pressed the OK button. */
		new_text = gtk_entry_get_text (GTK_ENTRY (priv->address_entry));

		g_snprintf (buffer, 190, "MAILTO:%s", new_text);
		value = icalvalue_new_text (buffer);
		icalproperty_set_value (prop, value);

		/* Take care of the ROLE. */
		icalproperty_remove_parameter (prop, ICAL_ROLE_PARAMETER);

		param = NULL;
		text = gtk_entry_get_text (GTK_ENTRY(priv->role_entry));

		for (cntr = 0; cntr < 5; cntr++) {
			if (strncmp (text, role_values[cntr], 3) == 0) {
				param = icalparameter_new_role (cntr);
				break;
			}
		}

		if (param == NULL) {
			g_print ("e-meeting-edit.c: edit_attendee() the ROLE param was null.\n");
			/* Use this as a default case, if none of the others match. */
			param = icalparameter_new_role (ICAL_ROLE_REQPARTICIPANT);
		}

		icalproperty_add_parameter (prop, param);

		/* Now the RSVP. */
		icalproperty_remove_parameter (prop, ICAL_RSVP_PARAMETER);

		param = icalparameter_new_rsvp 
				(gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->rsvp_check)));
		icalproperty_add_parameter (prop, param);

		retval = TRUE;
	}
	else  /* The user didn't say OK. */
		retval = FALSE;
	
	return retval;
}

static void
schedule_button_clicked_cb (GtkWidget *widget, gpointer data)
{
	EMeetingEditorPrivate *priv;

	EMeetingTimeSelector *mts;
	EMeetingTimeSelectorAttendeeType type;
	GtkWidget *dialog;
	gchar *attendee;
	gint cntr, row;
	icalproperty *prop;
	icalparameter *param;
	gint button_num;

	priv = (EMeetingEditorPrivate *) ((EMeetingEditor *)data)->priv;

	
	gtk_widget_push_visual (gdk_imlib_get_visual ());
	gtk_widget_push_colormap (gdk_imlib_get_colormap ());
	
	dialog = gnome_dialog_new ("Schedule Meeting", "Set Time", "Cancel", NULL);

	gtk_window_set_default_size (GTK_WINDOW (dialog), 600, 400);
	gtk_window_set_policy (GTK_WINDOW (dialog), FALSE, TRUE, FALSE);

	mts = (EMeetingTimeSelector *)e_meeting_time_selector_new ();
	gtk_container_add (GTK_CONTAINER (GNOME_DIALOG (dialog)->vbox), GTK_WIDGET (mts));
	gtk_window_add_accel_group (GTK_WINDOW (dialog),
				    E_MEETING_TIME_SELECTOR (mts)->accel_group);
	gtk_widget_show (GTK_WIDGET (mts));
	
	gtk_widget_pop_visual ();
	gtk_widget_pop_colormap ();
	

	/* Let's stick all the attendees that we have in our clist, into the
	   meeting time widget. */
	for (cntr = 0; cntr < priv->numentries; cntr++ ) {
		gtk_clist_get_text (GTK_CLIST (priv->attendee_list), cntr, 
				    ADDRESS_COL, &attendee);
		row = e_meeting_time_selector_attendee_add (mts, attendee, NULL);

		prop = (icalproperty *)gtk_clist_get_row_data (GTK_CLIST (priv->attendee_list), cntr);
		param = get_icalparam_by_type (prop, ICAL_ROLE_PARAMETER);

		switch (icalparameter_get_role (param)) {
			case ICAL_ROLE_CHAIR:
			case ICAL_ROLE_REQPARTICIPANT:
			  type = E_MEETING_TIME_SELECTOR_REQUIRED_PERSON;
			  break;
			default:
			  type = E_MEETING_TIME_SELECTOR_OPTIONAL_PERSON;
		}

		e_meeting_time_selector_attendee_set_type (mts, row, type);
	}

	/* I don't want the meeting widget to be destroyed before I can
	   extract information from it; so now the dialog window will just
	   be hidden when the user clicks a button or closes it. */
	gnome_dialog_close_hides (GNOME_DIALOG (dialog), TRUE);

	gnome_dialog_set_close (GNOME_DIALOG (dialog), TRUE);

	button_num = gnome_dialog_run (GNOME_DIALOG (dialog));

	if (button_num == 0) {
		/* The user clicked "Set Time". */
                gint start_year, start_month, start_day, start_hour, start_minute, 
		     end_year, end_month, end_day, end_hour, end_minute;
		CalComponentDateTime cal_dtstart, cal_dtend;


		e_meeting_time_selector_get_meeting_time (mts,
							       &start_year,
							       &start_month,
							       &start_day,
							       &start_hour,
							       &start_minute,
							       &end_year,
							       &end_month,
							       &end_day,
							       &end_hour,
							       &end_minute);
		
		cal_component_get_dtstart (priv->comp, &cal_dtstart);
		cal_component_get_dtend (priv->comp, &cal_dtend);

		cal_dtstart.value->second = 0;
		cal_dtstart.value->minute = start_minute;
		cal_dtstart.value->hour = start_hour;
		cal_dtstart.value->day = start_day;
		cal_dtstart.value->month = start_month;
		cal_dtstart.value->year = start_year;

		cal_dtend.value->second = 0;
		cal_dtend.value->minute = end_minute;
		cal_dtend.value->hour = end_hour;
		cal_dtend.value->day = end_day;
		cal_dtend.value->month = end_month;
		cal_dtend.value->year = end_year;

		cal_component_set_dtstart (priv->comp, &cal_dtstart);
		cal_component_set_dtend (priv->comp, &cal_dtend);

		cal_component_free_datetime (&cal_dtstart);
		cal_component_free_datetime (&cal_dtend);

		event_editor_update_widgets (priv->ee);	

		priv->dirty = TRUE;
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));

	return;
}

#define COMPOSER_OAFID "OAFIID:evolution-composer:evolution-mail:cd8618ea-53e1-4b9e-88cf-ec578bdb903b"


static gchar *itip_methods[] = {
	"REQUEST"
};

enum itip_method_enum {
	METHOD_REQUEST
};

/********
 * This routine is called when the send button is clicked. Duh. 
 * Actually, I'm just testing my commenting macros.
 ********/
static void
send_button_clicked_cb (GtkWidget *widget, gpointer data)
{
	EMeetingEditorPrivate *priv;
	BonoboObjectClient *bonobo_server;
	Evolution_Composer composer_server;
	CORBA_Environment ev;
	Evolution_Composer_RecipientList *to_list, *cc_list, *bcc_list;
	Evolution_Composer_Recipient *recipient;
	gchar *cell_text;
	CORBA_char *subject;
	gint cntr;
	gint len;
	CalComponentText caltext;
	CORBA_char *content_type, *filename, *description, *attach_data;
	CORBA_boolean show_inline;
	CORBA_char tempstr[200];


	/********
	 * CODE
	 ********/

	priv = (EMeetingEditorPrivate *) ((EMeetingEditor *)data)->priv;

	CORBA_exception_init (&ev);

	/* First, I obtain an object reference that represents the Composer. */
	bonobo_server = bonobo_object_activate (COMPOSER_OAFID, 0);

	g_return_if_fail (bonobo_server != NULL);

	composer_server = bonobo_object_corba_objref (BONOBO_OBJECT (bonobo_server));

	/* All right, now I have to convert my list of recipients into one of those
	   CORBA sequences. */
	to_list = Evolution_Composer_RecipientList__alloc ();
	to_list->_maximum = priv->numentries;
	to_list->_length = priv->numentries; 
	to_list->_buffer = CORBA_sequence_Evolution_Composer_Recipient_allocbuf (priv->numentries);

	for (cntr = 0; cntr < priv->numentries; cntr++) {
		gtk_clist_get_text (GTK_CLIST (priv->attendee_list),
				    cntr, ADDRESS_COL,
				    &cell_text);
		len = strlen (cell_text);

		recipient = &(to_list->_buffer[cntr]);
		recipient->name = CORBA_string_alloc (0);  /* FIXME: we may want an actual name here. */
		recipient->address = CORBA_string_alloc (len);
		strcpy (recipient->address, cell_text);
	}

	cc_list = Evolution_Composer_RecipientList__alloc ();
	cc_list->_maximum = cc_list->_length = 0;
	bcc_list = Evolution_Composer_RecipientList__alloc ();
	bcc_list->_maximum = bcc_list->_length = 0;

	cal_component_get_summary (priv->comp, &caltext);
	subject = CORBA_string_alloc (strlen (caltext.value));
	strcpy (subject, caltext.value);

	Evolution_Composer_set_headers (composer_server, to_list, cc_list, bcc_list, subject, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_printerr ("gui/e-meeting-edit.c: I couldn't set the composer headers via CORBA! Aagh.\n");
		CORBA_exception_free (&ev);
		return;
	}

	sprintf (tempstr, "text/calendar;METHOD=%s", itip_methods[METHOD_REQUEST]);
	content_type = CORBA_string_alloc (strlen (tempstr));
	strcpy (content_type, tempstr);
	filename = CORBA_string_alloc (0);
	sprintf (tempstr, "Calendar attachment");
	description = CORBA_string_alloc (strlen (tempstr));
	strcpy (description, tempstr);
	show_inline = FALSE;

	/* I need to create an encapsulating iCalendar component, and stuff our vEvent
	   into it. */
	{
		icalcomponent *comp;
		icalproperty *prop;
		icalvalue *value;
		gchar *ical_string;

		comp = icalcomponent_new (ICAL_VCALENDAR_COMPONENT);
		
		prop = icalproperty_new (ICAL_PRODID_PROPERTY);
		value = icalvalue_new_text ("-//HelixCode/Evolution//EN");
		icalproperty_set_value (prop, value);
		icalcomponent_add_property (comp, prop);

		prop = icalproperty_new (ICAL_VERSION_PROPERTY);
		value = icalvalue_new_text ("2.0");
		icalproperty_set_value (prop, value);
		icalcomponent_add_property (comp, prop);

		prop = icalproperty_new (ICAL_METHOD_PROPERTY);
		value = icalvalue_new_text ("REQUEST");
		icalproperty_set_value (prop, value);
		icalcomponent_add_property (comp, prop);

		icalcomponent_add_component (comp, priv->vevent);

		ical_string = icalcomponent_as_ical_string (comp);
		attach_data = CORBA_string_alloc (strlen (ical_string));
		strcpy (attach_data, ical_string);

		icalcomponent_remove_component (comp, priv->vevent);
		icalcomponent_free (comp);
	}

	Evolution_Composer_attach_data (composer_server, 
					content_type, filename, description,
					show_inline, attach_data,
					&ev);
	
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_printerr ("gui/e-meeting-edit.c: I couldn't attach data to the composer via CORBA! Aagh.\n");
		CORBA_exception_free (&ev);
		return;
	}
	
	Evolution_Composer_show (composer_server, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_printerr ("gui/e-meeting-edit.c: I couldn't show the composer via CORBA! Aagh.\n");
		CORBA_exception_free (&ev);
		return;
	}
	
	CORBA_exception_free (&ev);

	/* Let's free shit up. */
	for (cntr = 0; cntr < priv->numentries; cntr++) {
		recipient = &(to_list->_buffer[cntr]);
		CORBA_free (recipient->name);
		CORBA_free (recipient->address);
	}

	CORBA_free (to_list->_buffer);
	CORBA_free (to_list);
	CORBA_free (cc_list);
	CORBA_free (bcc_list);

	CORBA_free (subject);
	CORBA_free (content_type);
	CORBA_free (filename);
	CORBA_free (description);
	CORBA_free (attach_data);

	bonobo_object_unref (BONOBO_OBJECT (bonobo_server));

}

	
static void 
add_button_clicked_cb (GtkWidget *widget, gpointer data)
{
	EMeetingEditorPrivate *priv;
	icalproperty *prop;
	icalparameter *param;

	priv = (EMeetingEditorPrivate *) ((EMeetingEditor *)data)->priv;

	prop = icalproperty_new (ICAL_ATTENDEE_PROPERTY);
	param = icalparameter_new_role (ICAL_ROLE_REQPARTICIPANT); 
	icalproperty_add_parameter (prop, param);
	param = icalparameter_new_rsvp (TRUE); 
	icalproperty_add_parameter (prop, param);
	param = icalparameter_new_partstat (ICAL_PARTSTAT_NEEDSACTION);
	icalproperty_add_parameter (prop, param);

	if (edit_attendee (prop, data) == TRUE) {
		/* Let's add this property to our component and to the CList. */
		icalcomponent_add_property (priv->vevent, prop);

		/* The -1 indicates that we should add a new row. */
		put_property_in_list (prop, -1, data);

		priv->dirty = TRUE;
	}
	else {
		icalproperty_free (prop);
	}
}	

static void 
delete_button_clicked_cb (GtkWidget *widget, gpointer data)
{
	EMeetingEditorPrivate *priv;

	priv = (EMeetingEditorPrivate *) ((EMeetingEditor *)data)->priv;

	if (priv->selected_row < 0) {
		GtkWidget *dialog;

		dialog = gnome_warning_dialog_parented ("You must select an entry to delete.",
							GTK_WINDOW (priv->meeting_window));
		gnome_dialog_run (GNOME_DIALOG(dialog));
	}
	else {
		/* Delete the associated property from the iCAL object. */
		icalproperty *prop;

		prop = (icalproperty *)gtk_clist_get_row_data (GTK_CLIST (priv->attendee_list),
							       priv->selected_row);
		icalcomponent_remove_property (priv->vevent, prop);
		icalproperty_free (prop);

		gtk_clist_remove (GTK_CLIST (priv->attendee_list), priv->selected_row);
		priv->selected_row = -1;
		priv->numentries--;
		priv->dirty = TRUE;
	}
}

static void 
edit_button_clicked_cb (GtkWidget *widget, gpointer data)
{
	EMeetingEditorPrivate *priv;

	priv = (EMeetingEditorPrivate *) ((EMeetingEditor *)data)->priv;

	
	if (priv->selected_row < 0) {
		GtkWidget *dialog;

		dialog = gnome_warning_dialog_parented ("You must select an entry to edit.",
							GTK_WINDOW (priv->meeting_window));
		gnome_dialog_run (GNOME_DIALOG(dialog));
		return;
	}
	else {
		icalproperty *prop, *new_prop;
		icalparameter *param;
		icalvalue *value;
		
		prop = (icalproperty *)gtk_clist_get_row_data (GTK_CLIST (priv->attendee_list),
							       priv->selected_row);

		g_assert (prop != NULL);

		new_prop = icalproperty_new_clone (prop);

		if (edit_attendee (new_prop, data)) {
			/* The user hit Okay. */
			/*We need to synchronize the old property with the newly edited one.*/
			value = icalvalue_new_clone (icalproperty_get_value (new_prop));
			icalproperty_set_value (prop, value);

			icalproperty_remove_parameter (prop, ICAL_ROLE_PARAMETER);
			icalproperty_remove_parameter (prop, ICAL_RSVP_PARAMETER);
			icalproperty_remove_parameter (prop, ICAL_PARTSTAT_PARAMETER);

			param = icalparameter_new_clone (get_icalparam_by_type (new_prop, ICAL_ROLE_PARAMETER));
			g_assert (param != NULL);
			icalproperty_add_parameter (prop, param);
			param = icalparameter_new_clone (get_icalparam_by_type (new_prop, ICAL_RSVP_PARAMETER));
			g_assert (param != NULL);
			icalproperty_add_parameter (prop, param);
			param = icalparameter_new_clone (get_icalparam_by_type (new_prop, ICAL_PARTSTAT_PARAMETER));
			g_assert (param != NULL);
			icalproperty_add_parameter (prop, param);
			
			put_property_in_list (prop, priv->selected_row, data);
			priv->dirty = TRUE;

		}
		icalproperty_free (new_prop);
	}
}



static void 
list_row_select_cb  (GtkWidget *widget,
                     gint row,
                     gint column,
                     GdkEventButton *event,
                     gpointer data)
{
	EMeetingEditorPrivate *priv;

	priv = (EMeetingEditorPrivate *) ((EMeetingEditor *)data)->priv;
	
	priv->selected_row = row;
}

static void
organizer_changed_cb (GtkWidget *widget, gpointer data)
{
	EMeetingEditorPrivate *priv;

	priv = (EMeetingEditorPrivate *) ((EMeetingEditor *)data)->priv;

	gtk_signal_disconnect (GTK_OBJECT (priv->organizer_entry), priv->changed_signal_id);

	priv->dirty = TRUE;
}


/* ------------------------------------------------------------ */
/* --------------------- Exported Functions ------------------- */
/* ------------------------------------------------------------ */

EMeetingEditor * 
e_meeting_editor_new (CalComponent *comp, CalClient *client, EventEditor *ee)
{
	EMeetingEditor *object;
	EMeetingEditorPrivate *priv;

	object = (EMeetingEditor *)g_new(EMeetingEditor, 1);
	
	priv = (EMeetingEditorPrivate *) g_new0(EMeetingEditorPrivate, 1);
	priv->selected_row = -1;
	priv->comp = comp;
	priv->client = client;
	priv->icalcomp = cal_component_get_icalcomponent (comp);
	priv->ee = ee;
	
	object->priv = priv;

	return object;	
}

void
e_meeting_editor_free (EMeetingEditor *editor)
{
	if (editor == NULL)
		return;
		
	if (editor->priv != NULL)
		g_free (editor->priv);
	
	g_free (editor);
}
	


void 
e_meeting_edit (EMeetingEditor *editor)
{
	EMeetingEditorPrivate *priv;
	GtkWidget *add_button, *delete_button, *edit_button;
	icalproperty *prop;
	icalvalue *value;
	gchar *text;


	g_return_if_fail (editor != NULL);

	priv = (EMeetingEditorPrivate *)editor->priv;

	g_return_if_fail (priv != NULL);


	priv->xml = glade_xml_new (EVOLUTION_GLADEDIR "/" E_MEETING_GLADE_XML, NULL);
	
	priv->meeting_window =  glade_xml_get_widget (priv->xml, "meeting_window");
	priv->attendee_list = glade_xml_get_widget (priv->xml, "attendee_list");
	priv->role_entry = glade_xml_get_widget (priv->xml, "role_entry");
	priv->rsvp_check = glade_xml_get_widget (priv->xml, "rsvp_check");
	priv->schedule_button = glade_xml_get_widget (priv->xml, "schedule_button");
	priv->send_button = glade_xml_get_widget (priv->xml, "send_button");

	gtk_clist_set_column_justification (GTK_CLIST (priv->attendee_list), ROLE_COL, GTK_JUSTIFY_CENTER);
	gtk_clist_set_column_justification (GTK_CLIST (priv->attendee_list), RSVP_COL, GTK_JUSTIFY_CENTER);
	gtk_clist_set_column_justification (GTK_CLIST (priv->attendee_list), STATUS_COL, GTK_JUSTIFY_CENTER);

        gtk_signal_connect (GTK_OBJECT (priv->meeting_window), "delete_event",
                            GTK_SIGNAL_FUNC (window_delete_cb), editor);

	gtk_signal_connect_after (GTK_OBJECT (priv->meeting_window), "delete_event",
				  GTK_SIGNAL_FUNC (window_destroy_cb), editor);

        gtk_signal_connect (GTK_OBJECT (priv->meeting_window), "destroy_event",
                            GTK_SIGNAL_FUNC (window_destroy_cb), editor);

	gtk_signal_connect (GTK_OBJECT (priv->attendee_list), "select_row",
			    GTK_SIGNAL_FUNC (list_row_select_cb), editor);
	
	gtk_signal_connect (GTK_OBJECT (priv->schedule_button), "clicked",
			    GTK_SIGNAL_FUNC (schedule_button_clicked_cb), editor);
	
	gtk_signal_connect (GTK_OBJECT (priv->send_button), "clicked",
			    GTK_SIGNAL_FUNC (send_button_clicked_cb), editor);


	add_button = glade_xml_get_widget (priv->xml, "add_button");
	delete_button = glade_xml_get_widget (priv->xml, "delete_button");
	edit_button = glade_xml_get_widget (priv->xml, "edit_button");

	gtk_signal_connect (GTK_OBJECT (add_button), "clicked",
			    GTK_SIGNAL_FUNC (add_button_clicked_cb), editor);
	
	gtk_signal_connect (GTK_OBJECT (delete_button), "clicked",
			    GTK_SIGNAL_FUNC (delete_button_clicked_cb), editor);

	gtk_signal_connect (GTK_OBJECT (edit_button), "clicked",
			    GTK_SIGNAL_FUNC (edit_button_clicked_cb), editor);

	priv->organizer_entry = glade_xml_get_widget (priv->xml, "organizer_entry");

	if (icalcomponent_isa (priv->icalcomp) != ICAL_VEVENT_COMPONENT)
		priv->vevent = icalcomponent_get_first_component(priv->icalcomp,ICAL_VEVENT_COMPONENT);
	else
		priv->vevent = priv->icalcomp;

	g_assert (priv->vevent != NULL);

	/* Let's extract the organizer, if there is one. */
        prop = icalcomponent_get_first_property (priv->vevent, ICAL_ORGANIZER_PROPERTY);

	if (prop != NULL) {
		gchar *buffer;

	        value = icalproperty_get_value (prop);
		buffer = g_strdup (icalvalue_as_ical_string (value));
	        if (buffer != NULL) {
			/* Strip off the MAILTO:, if it is present. */
	        	text = strchr (buffer, ':');
			if (text == NULL)
				text = buffer;
			else
				text++;
				
			gtk_entry_set_text (GTK_ENTRY (priv->organizer_entry), text);
			g_free (buffer);
		}
		
	}

	priv->changed_signal_id = gtk_signal_connect (GTK_OBJECT (priv->organizer_entry), "changed",
						      GTK_SIGNAL_FUNC (organizer_changed_cb), editor);


	/* Let's go through the iCAL object, and create a list entry
	   for each ATTENDEE property. */
        for (prop = icalcomponent_get_first_property (priv->vevent, ICAL_ATTENDEE_PROPERTY);
             prop != NULL;
             prop = icalcomponent_get_next_property (priv->vevent, ICAL_ATTENDEE_PROPERTY))
	{
		put_property_in_list (prop, -1, editor);
	}
	

	gtk_widget_show (priv->meeting_window);

	gtk_main ();

	if (priv->meeting_window != NULL)
		gtk_widget_destroy (priv->meeting_window);

	if (priv->edit_dialog != NULL)
		gtk_widget_destroy (priv->edit_dialog);

	gtk_object_unref (GTK_OBJECT (priv->xml));
}
