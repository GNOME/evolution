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
#include "e-meeting-edit.h"

#define E_MEETING_GLADE_XML "e-meeting-dialog.glade"

#define E_MEETING_DEBUG

/* These are the widgets to be used in the GUI. */
static GladeXML *xml;
static GtkWidget *meeting_window;
static GtkWidget *attendee_list;
static GtkWidget *address_entry;
static GtkWidget *add_dialog;
static gint selected_row;


static gboolean
window_delete_cb (GtkWidget *widget,
		  GdkEvent *event,
		  gpointer data)
{	

#ifdef E_MEETING_DEBUG
	g_print ("e-meeting-edit.c: The main window received a delete event.\n");
#endif

	return (FALSE);
}

static void 
window_destroy_cb (GtkWidget *widget,
		   gpointer data)
{
#ifdef E_MEETING_DEBUG
	g_print ("e-meeting-edit.c: The main window received a destroy event.\n");
#endif
	gtk_main_quit ();
	return;
}

static void 
add_button_clicked_cb (GtkWidget *widget, gpointer data)
{
	gint button_num;

#ifdef E_MEETING_DEBUG
	g_print ("e-meeting-edit.c: the add button was clicked.\n");
#endif
	if (add_dialog == NULL || address_entry == NULL) {
		add_dialog = glade_xml_get_widget (xml, "add_dialog");
		address_entry = glade_xml_get_widget (xml, "address_entry");

		gnome_dialog_set_close (GNOME_DIALOG (add_dialog), TRUE);
		gnome_dialog_editable_enters (GNOME_DIALOG (add_dialog), GTK_EDITABLE (address_entry));
		gnome_dialog_close_hides (GNOME_DIALOG (add_dialog), TRUE);
		gnome_dialog_set_default (GNOME_DIALOG (add_dialog), 0);
	}

	g_return_if_fail (add_dialog != NULL);
	g_return_if_fail (address_entry != NULL);

	gtk_widget_show (add_dialog);

	button_num = gnome_dialog_run (GNOME_DIALOG (add_dialog));

	if (button_num == 0) {
		/* The user pressed Okay--let's add it to our list. */
		gchar temp_stat[] = "Needs action";
		gchar *address;
		gchar * row_text[2];

		address = gtk_entry_get_text (GTK_ENTRY (address_entry));
		row_text[0] = address;
		row_text[1] = temp_stat;
		
		gtk_clist_append (GTK_CLIST (attendee_list), row_text);
	}
	
	gtk_entry_set_text (GTK_ENTRY (address_entry), "");
}	

static void 
delete_button_clicked_cb (GtkWidget *widget, gpointer data)
{
	if (selected_row < 0) {
		GtkWidget *dialog;

		dialog = gnome_warning_dialog_parented ("You must select an entry to delete.",
							GTK_WINDOW (add_dialog));
		gnome_dialog_run (GNOME_DIALOG(dialog));
	}
	else {
		gtk_clist_remove (GTK_CLIST (attendee_list), selected_row);
		selected_row = -1;
	}
}

static void 
edit_button_clicked_cb (GtkWidget *widget, gpointer data)
{
	if (selected_row < 0) {
		GtkWidget *dialog;

		dialog = gnome_warning_dialog_parented ("You must select an entry to edit.",
							GTK_WINDOW (add_dialog));
		gnome_dialog_run (GNOME_DIALOG(dialog));
		return;
	}
	else {
		gchar *text[2];
		gint cntr;
		gint button_num;

		for (cntr = 0; cntr < 2; cntr++) {
			gtk_clist_get_text (GTK_CLIST (attendee_list),
						   selected_row,
						   cntr,
						   &text[cntr]);
		}
		
		if (add_dialog == NULL || address_entry == NULL) {
	                add_dialog = glade_xml_get_widget (xml, "add_dialog");
        	        address_entry = glade_xml_get_widget (xml, "address_entry");
			
			gnome_dialog_set_close (GNOME_DIALOG (add_dialog), TRUE);
			gnome_dialog_editable_enters (GNOME_DIALOG (add_dialog), GTK_EDITABLE (address_entry));
			gnome_dialog_close_hides (GNOME_DIALOG (add_dialog), TRUE);
			gnome_dialog_set_default (GNOME_DIALOG (add_dialog), 0);
		}
		
		gtk_entry_set_text (GTK_ENTRY (address_entry), text[0]);

		gtk_widget_show (add_dialog);

		button_num = gnome_dialog_run (GNOME_DIALOG (add_dialog));

		if (button_num == 0) {
			gchar *new_text;

			new_text = gtk_entry_get_text (GTK_ENTRY (address_entry));

			gtk_clist_set_text (GTK_CLIST (attendee_list), 
					    selected_row,
					    0,
					    new_text);
		}

		gtk_entry_set_text (GTK_ENTRY (address_entry), "");
	}
}



	


static void list_row_select_cb  (GtkWidget *widget,
                          gint row,
                          gint column,
                          GdkEventButton *event,
                          gpointer data)
{
	selected_row = row;
}
			  
static void
reset_widgets (void)
{
	xml = NULL;
	meeting_window = NULL;
	attendee_list = NULL;
	address_entry = NULL;
	add_dialog = NULL;
	selected_row = -1;
}

void 
e_meeting_edit (CalComponent *comp, CalClient *client)
{
	GtkWidget *add_button, *delete_button, *edit_button;

	reset_widgets ();

	xml = glade_xml_new (EVOLUTION_GLADEDIR "/" E_MEETING_GLADE_XML, NULL);
	
	meeting_window =  glade_xml_get_widget (xml, "meeting_window");
	attendee_list = glade_xml_get_widget (xml, "attendee_list");

	gtk_clist_set_column_justification (GTK_CLIST (attendee_list), 1, GTK_JUSTIFY_CENTER);

        gtk_signal_connect (GTK_OBJECT (meeting_window), "delete_event",
                            GTK_SIGNAL_FUNC (window_delete_cb), NULL);

	gtk_signal_connect_after (GTK_OBJECT (meeting_window), "delete_event",
				  GTK_SIGNAL_FUNC (window_destroy_cb), NULL);

        gtk_signal_connect (GTK_OBJECT (meeting_window), "destroy_event",
                            GTK_SIGNAL_FUNC (window_destroy_cb), NULL);

	gtk_signal_connect (GTK_OBJECT (attendee_list), "select_row",
			    GTK_SIGNAL_FUNC (list_row_select_cb), NULL);

	add_button = glade_xml_get_widget (xml, "add_button");
	delete_button = glade_xml_get_widget (xml, "delete_button");
	edit_button = glade_xml_get_widget (xml, "edit_button");

	gtk_signal_connect (GTK_OBJECT (add_button), "clicked",
			    GTK_SIGNAL_FUNC (add_button_clicked_cb), NULL);
	
	gtk_signal_connect (GTK_OBJECT (delete_button), "clicked",
			    GTK_SIGNAL_FUNC (delete_button_clicked_cb), NULL);

	gtk_signal_connect (GTK_OBJECT (edit_button), "clicked",
			    GTK_SIGNAL_FUNC (edit_button_clicked_cb), NULL);

	gtk_widget_show (meeting_window);

	gtk_main ();

#ifdef E_MEETING_DEBUG
	g_print ("e-meeting-edit.c: We've terminated the subsidiary gtk_main().\n");
#endif

	if (meeting_window != NULL)
		gtk_widget_destroy (meeting_window);

	if (add_dialog != NULL)
		gtk_widget_destroy (add_dialog);



	gtk_object_destroy (GTK_OBJECT (xml));
}
