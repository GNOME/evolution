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
#include "e-meeting-edit.h"

#define E_MEETING_GLADE_XML "e-meeting-dialog.glade"

#define E_MEETING_DEBUG

typedef struct _EMeetingEditorPrivate EMeetingEditorPrivate;

struct _EMeetingEditorPrivate {
	/* These are the widgets to be used in the GUI. */
	GladeXML *xml;
	GtkWidget *meeting_window;
	GtkWidget *attendee_list;
	GtkWidget *address_entry;
	GtkWidget *add_dialog;

	/* Various pieces of information. */
	gint selected_row;
	CalComponent *comp;
	CalClient *client;
	icalcomponent *icalcomp, *vevent;

	gint numentries;  /* How many attendees are there? */
	gboolean dirty;  /* Has anything changed? */
}; 


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


static gboolean
window_delete_cb (GtkWidget *widget,
		  GdkEvent *event,
		  gpointer data)
{	
	EMeetingEditorPrivate *priv;

	priv = (EMeetingEditorPrivate *) ((EMeetingEditor *)data)->priv;

#ifdef E_MEETING_DEBUG
	g_printerr ("e-meeting-edit.c: The main window received a delete event.\n");
#endif
	
	if (priv->dirty == TRUE) {
		/* FIXME: notify the event editor that our data has changed. 
			For now, I'll just display a dialog box. */
		{
			GtkWidget *dialog;
		
			dialog = gnome_warning_dialog_parented ("Note that the meeting has changed,\n"
								"and you should save this event.",
								GTK_WINDOW (priv->meeting_window));
			gnome_dialog_run (GNOME_DIALOG(dialog));
		}
	}

	

	return (FALSE);
}

static void 
window_destroy_cb (GtkWidget *widget,
		   gpointer data)
{
	EMeetingEditorPrivate *priv;

#ifdef E_MEETING_DEBUG
	g_printerr ("e-meeting-edit.c: The main window received a destroy event.\n");
#endif

	priv = (EMeetingEditorPrivate *) ((EMeetingEditor *)data)->priv;

	gtk_main_quit ();
	return;
}

static void 
add_button_clicked_cb (GtkWidget *widget, gpointer data)
{
	EMeetingEditorPrivate *priv;
	gint button_num;
	gchar buffer[200];

#ifdef E_MEETING_DEBUG
	g_printerr ("e-meeting-edit.c: the add button was clicked.\n");
#endif

	priv = (EMeetingEditorPrivate *) ((EMeetingEditor *)data)->priv;

	if (priv->add_dialog == NULL || priv->address_entry == NULL) {
		priv->add_dialog = glade_xml_get_widget (priv->xml, "add_dialog");
		priv->address_entry = glade_xml_get_widget (priv->xml, "address_entry");

		gnome_dialog_set_close (GNOME_DIALOG (priv->add_dialog), TRUE);
		gnome_dialog_editable_enters (GNOME_DIALOG (priv->add_dialog), 
					      GTK_EDITABLE (priv->address_entry));
		gnome_dialog_close_hides (GNOME_DIALOG (priv->add_dialog), TRUE);
		gnome_dialog_set_default (GNOME_DIALOG (priv->add_dialog), 0);
	}

	g_return_if_fail (priv->add_dialog != NULL);
	g_return_if_fail (priv->address_entry != NULL);

	gtk_widget_show (priv->add_dialog);

	button_num = gnome_dialog_run (GNOME_DIALOG (priv->add_dialog));

	if (button_num == 0) {
		/* The user pressed Okay--let's add it to our list. */
		icalproperty *prop;
		icalparameter *param;
		icalvalue *value;
		
		gchar *address;
		gchar * row_text[2];

		address = gtk_entry_get_text (GTK_ENTRY (priv->address_entry));
		
		prop = icalproperty_new (ICAL_ATTENDEE_PROPERTY);
		g_snprintf (buffer, 190, "MAILTO:%s", address);
		value = icalvalue_new_text (buffer);
		icalproperty_set_value (prop, value);

		param = icalparameter_new_partstat (ICAL_PARTSTAT_PARAMETER);
		icalparameter_set_partstat (param, ICAL_PARTSTAT_NEEDSACTION);
		icalproperty_add_parameter (prop, param);

		icalcomponent_add_property (priv->vevent, prop);
		
		row_text[0] = address;
		row_text[1] = partstat_values[icalparameter_get_partstat (param)];
		
		gtk_clist_append (GTK_CLIST (priv->attendee_list), row_text);
		gtk_clist_set_row_data (GTK_CLIST (priv->attendee_list), priv->numentries, prop);

		priv->numentries++;
		priv->dirty = TRUE;
	}
	
	gtk_entry_set_text (GTK_ENTRY (priv->address_entry), "");
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
		gchar *text[2];
		gint cntr;
		gint button_num;

		for (cntr = 0; cntr < 2; cntr++) {
			gtk_clist_get_text (GTK_CLIST (priv->attendee_list),
						   priv->selected_row,
						   cntr,
						   &text[cntr]);
		}
		
		if (priv->add_dialog == NULL || priv->address_entry == NULL) {
	                priv->add_dialog = glade_xml_get_widget (priv->xml, "add_dialog");
        	        priv->address_entry = glade_xml_get_widget (priv->xml, "address_entry");
			
			gnome_dialog_set_close (GNOME_DIALOG (priv->add_dialog), TRUE);
			gnome_dialog_editable_enters (GNOME_DIALOG (priv->add_dialog), 
						      GTK_EDITABLE (priv->address_entry));
			gnome_dialog_close_hides (GNOME_DIALOG (priv->add_dialog), TRUE);
			gnome_dialog_set_default (GNOME_DIALOG (priv->add_dialog), 0);
		}
		
		gtk_entry_set_text (GTK_ENTRY (priv->address_entry), text[0]);

		gtk_widget_show (priv->add_dialog);

		button_num = gnome_dialog_run (GNOME_DIALOG (priv->add_dialog));

		if (button_num == 0) {
			gchar *new_text;
			icalproperty *prop;
			icalparameter *param;
			icalvalue *value;
			gchar buffer[200];

			new_text = gtk_entry_get_text (GTK_ENTRY (priv->address_entry));

			gtk_clist_set_text (GTK_CLIST (priv->attendee_list), 
					    priv->selected_row,
					    0,
					    new_text);
			
			prop = (icalproperty *)gtk_clist_get_row_data (GTK_CLIST (priv->attendee_list),
				 					priv->selected_row);
			g_snprintf (buffer, 190, "MAILTO:%s", new_text);
			value = icalvalue_new_text (buffer);
			icalproperty_set_value (prop, value);

			priv->dirty = TRUE;
		}

		gtk_entry_set_text (GTK_ENTRY (priv->address_entry), "");
	}
}



static void list_row_select_cb  (GtkWidget *widget,
                          gint row,
                          gint column,
                          GdkEventButton *event,
                          gpointer data)
{
	EMeetingEditorPrivate *priv;

	priv = (EMeetingEditorPrivate *) ((EMeetingEditor *)data)->priv;
	
	priv->selected_row = row;
}
			  

/* ------------------------------------------------------------ */
/* --------------------- Exported Functions ------------------- */
/* ------------------------------------------------------------ */

EMeetingEditor * 
e_meeting_editor_new (CalComponent *comp, CalClient *client)
{
	EMeetingEditor *object;
	EMeetingEditorPrivate *priv;

	object = (EMeetingEditor *)g_new(EMeetingEditor, 1);
	
	priv = (EMeetingEditorPrivate *) g_new0(EMeetingEditorPrivate, 1);
	priv->selected_row = -1;
	priv->comp = comp;
	priv->client = client;
	priv->icalcomp = cal_component_get_icalcomponent (comp);
	
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
	icalparameter *param;
	icalvalue *value;
	gchar *text;
	gchar *row_text[2];


	g_return_if_fail (editor != NULL);

	priv = (EMeetingEditorPrivate *)editor->priv;

	g_return_if_fail (priv != NULL);


	priv->xml = glade_xml_new (EVOLUTION_GLADEDIR "/" E_MEETING_GLADE_XML, NULL);
	
	priv->meeting_window =  glade_xml_get_widget (priv->xml, "meeting_window");
	priv->attendee_list = glade_xml_get_widget (priv->xml, "attendee_list");

	gtk_clist_set_column_justification (GTK_CLIST (priv->attendee_list), 1, GTK_JUSTIFY_CENTER);

        gtk_signal_connect (GTK_OBJECT (priv->meeting_window), "delete_event",
                            GTK_SIGNAL_FUNC (window_delete_cb), editor);

	gtk_signal_connect_after (GTK_OBJECT (priv->meeting_window), "delete_event",
				  GTK_SIGNAL_FUNC (window_destroy_cb), editor);

        gtk_signal_connect (GTK_OBJECT (priv->meeting_window), "destroy_event",
                            GTK_SIGNAL_FUNC (window_destroy_cb), editor);

	gtk_signal_connect (GTK_OBJECT (priv->attendee_list), "select_row",
			    GTK_SIGNAL_FUNC (list_row_select_cb), editor);

	add_button = glade_xml_get_widget (priv->xml, "add_button");
	delete_button = glade_xml_get_widget (priv->xml, "delete_button");
	edit_button = glade_xml_get_widget (priv->xml, "edit_button");

	gtk_signal_connect (GTK_OBJECT (add_button), "clicked",
			    GTK_SIGNAL_FUNC (add_button_clicked_cb), editor);
	
	gtk_signal_connect (GTK_OBJECT (delete_button), "clicked",
			    GTK_SIGNAL_FUNC (delete_button_clicked_cb), editor);

	gtk_signal_connect (GTK_OBJECT (edit_button), "clicked",
			    GTK_SIGNAL_FUNC (edit_button_clicked_cb), editor);

	if (icalcomponent_isa (priv->icalcomp) != ICAL_VEVENT_COMPONENT)
		priv->vevent = icalcomponent_get_first_component(priv->icalcomp,ICAL_VEVENT_COMPONENT);
	else
		priv->vevent = priv->icalcomp;

	g_assert (priv->vevent != NULL);

	/* Let's go through the iCAL object, and create a list entry
	   for each ATTENDEE property. */
        for (prop = icalcomponent_get_first_property (priv->vevent, ICAL_ATTENDEE_PROPERTY);
             prop != NULL;
             prop = icalcomponent_get_next_property (priv->vevent, ICAL_ATTENDEE_PROPERTY))
	{
 		value = icalproperty_get_value (prop);
		text = g_strdup (icalvalue_as_ical_string (value));

		/* Strip off the MAILTO: from the property value. */
		row_text[0] = strchr (text, ':');
		if (row_text[0] != NULL) 
			row_text[0]++;
		else
			row_text[0] = text;
		
		for (param = icalproperty_get_first_parameter (prop, ICAL_ANY_PARAMETER);
		     param != NULL && icalparameter_isa (param) != ICAL_PARTSTAT_PARAMETER;
		     param = icalproperty_get_next_parameter (prop, ICAL_ANY_PARAMETER) );
		
		if (param == NULL) {
			/* We need to add a PARTSTAT parameter to this property. */
			param = icalparameter_new_partstat (ICAL_PARTSTAT_PARAMETER);
			icalparameter_set_partstat (param, ICAL_PARTSTAT_NEEDSACTION);
			icalproperty_add_parameter (prop, param);
		}

		/* row_text[1] corresponds to the `Status' column in the CList. */
		row_text[1] = partstat_values[icalparameter_get_partstat (param)];
		gtk_clist_append (GTK_CLIST (priv->attendee_list), row_text);

		/* The property to which each row in the list refers will be stored
			as the data for that row. */
		gtk_clist_set_row_data (GTK_CLIST (priv->attendee_list), priv->numentries, prop);
		priv->numentries++;

		g_free (text);
	}
	

	gtk_widget_show (priv->meeting_window);

	gtk_main ();

#ifdef E_MEETING_DEBUG
	g_printerr ("e-meeting-edit.c: We've terminated the subsidiary gtk_main().\n");
#endif

	if (priv->meeting_window != NULL)
		gtk_widget_destroy (priv->meeting_window);

	if (priv->add_dialog != NULL)
		gtk_widget_destroy (priv->add_dialog);

	gtk_object_unref (GTK_OBJECT (priv->xml));
}
