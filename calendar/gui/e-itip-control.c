/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-itip-control.c
 *
 * Authors:
 *    Jesse Pavel <jpavel@helixcode.com>
 *
 * Copyright 2000, Helix Code, Inc.
 */

#include <config.h>
#include <gnome.h>
#include <bonobo.h>
#include <glade/glade.h>
#include <icaltypes.h>
#include <ical.h>
#include <time.h>
#include <Evolution-Composer.h>

#include "e-itip-control.h"
#include <cal-util/cal-component.h>
#include <cal-client/cal-client.h>


#define DEFAULT_WIDTH 400
#define DEFAULT_HEIGHT 300

#define COMPOSER_OAFID "OAFIID:evolution-composer:evolution-mail:cd8618ea-53e1-4b9e-88cf-ec578bdb903b"

extern gchar *evolution_dir;

typedef struct _EItipControlPrivate EItipControlPrivate;

struct _EItipControlPrivate {
	GladeXML *xml, *xml2;
	GtkWidget *main_frame;
	GtkWidget *organizer_entry, *dtstart_label, *dtend_label;
	GtkWidget *summary_entry, *description_box, *message_text;
	GtkWidget *button_box;
	GtkWidget *address_entry;
	GtkWidget *add_button;
	GtkWidget *loading_window;
	GtkWidget *loading_progress;

	icalcomponent *main_comp, *comp;
	CalComponent *cal_comp;
	char *vcalendar;
	gchar *from_address, *my_address, *organizer;
};

enum E_ITIP_BONOBO_ARGS {
	FROM_ADDRESS_ARG_ID,
	MY_ADDRESS_ARG_ID
};

static icalparameter *
get_icalparam_by_type (icalproperty *prop, icalparameter_kind kind)
{
	icalparameter *param;

	for (param = icalproperty_get_first_parameter (prop, ICAL_ANY_PARAMETER);
	     param != NULL && icalparameter_isa (param) != kind;
	     param = icalproperty_get_next_parameter (prop, ICAL_ANY_PARAMETER) );

	return param;
}


/********
 * find_attendee() searches through the attendee properties of `comp'
 * and returns the one the value of which is the same as `address' if such
 * a property exists. Otherwise, it will return NULL.
 ********/
static icalproperty *
find_attendee (icalcomponent *comp, char *address)
{
	icalproperty *prop;
	char *attendee, *text;
	icalvalue *value;
	
	for (prop = icalcomponent_get_first_property (comp, ICAL_ATTENDEE_PROPERTY);
	     prop != NULL;
	     prop = icalcomponent_get_next_property (comp, ICAL_ATTENDEE_PROPERTY))
	{
		value = icalproperty_get_value (prop);
		if (!value)
			continue;

		attendee = icalvalue_get_string (value);

		/* Here I strip off the "MAILTO:" if it is present. */
		text = strchr (attendee, ':');
		if (text != NULL)
			text++;
		else
			text = attendee;

		if (strcmp (text, address) == 0) {
			/* We have found the correct property. */
			break;
		}
	}
			
	return prop;
}

static void
itip_control_destroy_cb (GtkObject *object,
		    gpointer data)
{
	EItipControlPrivate *priv = data;

	gtk_object_unref (GTK_OBJECT (priv->xml));
	gtk_object_unref (GTK_OBJECT (priv->xml2));

	if (priv->main_comp != NULL) {
		icalcomponent_free (priv->main_comp);
	}

	if (priv->cal_comp != NULL) {
		gtk_object_unref (GTK_OBJECT (priv->cal_comp));
	}

	if (priv->from_address != NULL)
		g_free (priv->from_address);
	
	if (priv->organizer != NULL)
		g_free (priv->organizer);

	if (priv->vcalendar != NULL)
		g_free (priv->vcalendar);

	g_free (priv);
}
	
static void
itip_control_size_request_cb (GtkWidget *widget, GtkRequisition *requisition)
{

	/* gtk_widget_set (GTK_WIDGET (widget), "width", DEFAULT_WIDTH, NULL); */
	/* gtk_widget_set (GTK_WIDGET (widget), "height", DEFAULT_HEIGHT, NULL); */
	requisition->width = DEFAULT_WIDTH;
	requisition->height = DEFAULT_HEIGHT;
}


static void
cal_loaded_cb (GtkObject *object, CalClientGetStatus status, gpointer data)
{
	CalClient *client = CAL_CLIENT (object);
	EItipControlPrivate *priv = data;

	gtk_widget_hide (priv->loading_progress);

	if (status == CAL_CLIENT_GET_SUCCESS) {
		if (cal_client_update_object (client, priv->cal_comp) == FALSE) {
			GtkWidget *dialog;
	
			dialog = gnome_warning_dialog("I couldn't update your calendar file!\n");
			gnome_dialog_run (GNOME_DIALOG(dialog));
		}
		else {
			/* We have success. */
			GtkWidget *dialog;
	
			dialog = gnome_ok_dialog("Component successfully updated.");
			gnome_dialog_run (GNOME_DIALOG(dialog));
		}
	}
	else {
		GtkWidget *dialog;

		dialog = gnome_ok_dialog("There was an error loading the calendar file.");
		gnome_dialog_run (GNOME_DIALOG(dialog));
	}

	gtk_object_unref (GTK_OBJECT (client));
	return;
}

static void
update_calendar (EItipControlPrivate *priv)
{
	gchar cal_uri[255];
	CalClient *client;

	sprintf (cal_uri, "%s/local/Calendar/calendar.ics", evolution_dir);
	
	client = cal_client_new ();
	if (cal_client_load_calendar (client, cal_uri) == FALSE) {
		GtkWidget *dialog;

		dialog = gnome_warning_dialog("I couldn't open your calendar file!\n");
		gnome_dialog_run (GNOME_DIALOG(dialog));
		gtk_object_unref (GTK_OBJECT (client));
	
		return;
	}

	gtk_signal_connect (GTK_OBJECT (client), "cal_loaded",
	    	   	    GTK_SIGNAL_FUNC (cal_loaded_cb), priv);
	
	gtk_progress_bar_update (GTK_PROGRESS_BAR (priv->loading_progress), 0.5);
	gtk_widget_show (priv->loading_progress);

	return;
}

static void
add_button_clicked_cb (GtkWidget *widget, gpointer data)
{
	EItipControlPrivate *priv = data;

	update_calendar (priv);

	return;
}

static void
change_my_status (icalparameter_partstat status, EItipControlPrivate *priv)
{
	icalproperty *prop;

	prop = find_attendee (priv->comp, priv->my_address);
	if (prop) {
		icalparameter *param;

		icalproperty_remove_parameter (prop, ICAL_PARTSTAT_PARAMETER);
		param = icalparameter_new_partstat (status);
		icalproperty_add_parameter (prop, param);
	}
}

static void
send_itip_reply (EItipControlPrivate *priv)
{
	BonoboObjectClient *bonobo_server;
	Evolution_Composer composer_server;
	CORBA_Environment ev;
	Evolution_Composer_RecipientList *to_list, *cc_list, *bcc_list;
	Evolution_Composer_Recipient *recipient;
	CORBA_char *subject;
	CalComponentText caltext;
	CORBA_char *content_type, *filename, *description, *attach_data;
	CORBA_boolean show_inline;
	CORBA_char tempstr[200];
	
	CORBA_exception_init (&ev);

	/* First, I obtain an object reference that represents the Composer. */
	bonobo_server = bonobo_object_activate (COMPOSER_OAFID, 0);

	g_return_if_fail (bonobo_server != NULL);

	composer_server = bonobo_object_corba_objref (BONOBO_OBJECT (bonobo_server));

	/* Now I have to make a CORBA sequence that represents a recipient list with
	   one item, for the organizer. */
	to_list = Evolution_Composer_RecipientList__alloc ();
	to_list->_maximum = 1;
	to_list->_length = 1; 
	to_list->_buffer = CORBA_sequence_Evolution_Composer_Recipient_allocbuf (1);

	recipient = &(to_list->_buffer[0]);
	recipient->name = CORBA_string_alloc (0);  /* FIXME: we may want an actual name here. */
	recipient->name[0] = '\0';
	recipient->address = CORBA_string_alloc (strlen (priv->organizer));
	strcpy (recipient->address, priv->organizer);

	cc_list = Evolution_Composer_RecipientList__alloc ();
	cc_list->_maximum = cc_list->_length = 0;
	bcc_list = Evolution_Composer_RecipientList__alloc ();
	bcc_list->_maximum = bcc_list->_length = 0;

	cal_component_get_summary (priv->cal_comp, &caltext);
	subject = CORBA_string_alloc (strlen (caltext.value));
	strcpy (subject, caltext.value);
	
	Evolution_Composer_set_headers (composer_server, to_list, cc_list, bcc_list, subject, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_printerr ("gui/e-meeting-edit.c: I couldn't set the composer headers via CORBA! Aagh.\n");
		CORBA_exception_free (&ev);
		return;
	}

	sprintf (tempstr, "text/calendar;METHOD=REPLY");
	content_type = CORBA_string_alloc (strlen (tempstr));
	strcpy (content_type, tempstr);
	filename = CORBA_string_alloc (0);
	filename[0] = '\0';
	sprintf (tempstr, "Calendar attachment");
	description = CORBA_string_alloc (strlen (tempstr));
	strcpy (description, tempstr);
	show_inline = FALSE;

	/* I need to create an encapsulating iCalendar component, and stuff our reply event
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
		value = icalvalue_new_text ("REPLY");
		icalproperty_set_value (prop, value);
		icalcomponent_add_property (comp, prop);

		icalcomponent_remove_component (priv->main_comp, priv->comp);
		icalcomponent_add_component (comp, priv->comp);

		ical_string = icalcomponent_as_ical_string (comp);
		attach_data = CORBA_string_alloc (strlen (ical_string));
		strcpy (attach_data, ical_string);

		icalcomponent_remove_component (comp, priv->comp);
		icalcomponent_add_component (priv->main_comp, priv->comp);
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

	/* Here is where we free our graciously-allocated memory. */
	if (CORBA_sequence_get_release (to_list) != FALSE)
		CORBA_free (to_list->_buffer);

	CORBA_free (to_list);
	CORBA_free (cc_list);
	CORBA_free (bcc_list);

	CORBA_free (subject);
	CORBA_free (content_type);
	CORBA_free (filename);
	CORBA_free (description);
	CORBA_free (attach_data);

}

static void
accept_button_clicked_cb (GtkWidget *widget, gpointer data)
{
	EItipControlPrivate *priv = data;

	change_my_status (ICAL_PARTSTAT_ACCEPTED, priv);
	send_itip_reply (priv);
	update_calendar (priv);
	
	return;
}

/*
 * Bonobo::PersistStream
 *
 * These two functions implement the Bonobo::PersistStream load and
 * save methods which allow data to be loaded into and out of the
 * BonoboObject.
 */

static char *
stream_read (Bonobo_Stream stream)
{
	Bonobo_Stream_iobuf *buffer;
	CORBA_Environment    ev;
	gchar *data = NULL;
	gint length = 0;

	CORBA_exception_init (&ev);
	do {
#define READ_CHUNK_SIZE 65536
		Bonobo_Stream_read (stream, READ_CHUNK_SIZE,
				    &buffer, &ev);

		if (ev._major != CORBA_NO_EXCEPTION) {
			CORBA_exception_free (&ev);
			return NULL;
		}

		if (buffer->_length <= 0)
			break;

		data = g_realloc (data,
				  length + buffer->_length);

		memcpy (data + length,
			buffer->_buffer, buffer->_length);

		length += buffer->_length;

		CORBA_free (buffer);
	} while (1);

	CORBA_free (buffer);
	CORBA_exception_free (&ev);

	if (data == NULL)
	  data = g_strdup("");

	return data;
} /* stream_read */

/*
 * This function implements the Bonobo::PersistStream:load method.
 */
static void
pstream_load (BonoboPersistStream *ps, const Bonobo_Stream stream,
	      Bonobo_Persist_ContentType type, void *data,
	      CORBA_Environment *ev)
{
	EItipControlPrivate *priv = data;
	gint pos, length, length2;
	icalcomponent_kind comp_kind;
	char message[256];
	

	if (type && g_strcasecmp (type, "text/calendar") != 0 &&	    
	    g_strcasecmp (type, "text/x-calendar") != 0) {	    
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_Bonobo_Persist_WrongDataType, NULL);
		return;
	}

	if ((priv->vcalendar = stream_read (stream)) == NULL) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_Bonobo_Persist_FileNotFound, NULL);
		return;
	}

	/* Do something with the data, here. */

	priv->main_comp = icalparser_parse_string (priv->vcalendar);
	if (priv->main_comp == NULL) {
		g_printerr ("e-itip-control.c: the iCalendar data was invalid!\n");
		return;
	}

	priv->comp = icalcomponent_get_first_component (priv->main_comp,
							ICAL_ANY_COMPONENT);

#if 0
	{   
		FILE *fp;

		fp = fopen ("evo.debug", "w");

		fputs ("The raw vCalendar data:\n\n", fp);
		fputs (priv->vcalendar, fp);

		fputs ("The main component:\n\n", fp);
		fputs (icalcomponent_as_ical_string (priv->main_comp), fp);

		fputs ("The child component:\n\n", fp);
		fputs (icalcomponent_as_ical_string (priv->comp), fp);

		fclose (fp);
	}
#endif

	if (priv->comp == NULL) {
		g_printerr ("e-itip-control.c: I could not extract a proper component from\n"
			    "     the vCalendar data.\n");
		icalcomponent_free (priv->main_comp);
		return;
	}

	comp_kind = icalcomponent_isa (priv->comp);

	switch (comp_kind) {
	case ICAL_VEVENT_COMPONENT:
	case ICAL_VTODO_COMPONENT:
	case ICAL_VJOURNAL_COMPONENT:
		priv->cal_comp = cal_component_new ();
		if (cal_component_set_icalcomponent (priv->cal_comp, priv->comp) == FALSE) {
			g_printerr ("e-itip-control.c: I couldn't create a CalComponent from the iTip data.\n");
			gtk_object_unref (GTK_OBJECT (priv->cal_comp));
		}
		break;
	case ICAL_VFREEBUSY_COMPONENT:
		/* Take care of busy time information. */
		return;
		break;
	default:
		/* We don't know what this is, so bail. */
		{
		GtkWidget *dialog;

		dialog = gnome_warning_dialog("I don't recognize this type of calendar component.");
		gnome_dialog_run (GNOME_DIALOG(dialog));

		g_free (priv->vcalendar);
		priv->vcalendar = NULL;

		return;
		}
		break;
	} /* End switch. */


	/* Okay, good then; now I will pick apart the component to get
	 all the things I'll show in my control. */
	{
		icalproperty *prop;
		gchar *new_text;
		gchar *organizer, *description, *summary;
		struct icaltimetype dtstart, dtend;
		time_t tstart, tend;

		prop = icalcomponent_get_first_property (priv->comp, ICAL_ORGANIZER_PROPERTY);
		if (prop) {
			organizer = icalproperty_get_organizer (prop);
		
			/* Here I strip off the "MAILTO:" if it is present. */
			new_text = strchr (organizer, ':');
			if (new_text != NULL)
				new_text++;
			else
				new_text = organizer;

			priv->organizer = g_strdup (new_text);
			gtk_entry_set_text (GTK_ENTRY (priv->organizer_entry), new_text);
		}

		prop = icalcomponent_get_first_property (priv->comp, ICAL_SUMMARY_PROPERTY);
		if (prop) {
			summary = icalproperty_get_summary (prop);
			gtk_entry_set_text (GTK_ENTRY (priv->summary_entry), summary);
		}

		prop = icalcomponent_get_first_property (priv->comp, ICAL_DESCRIPTION_PROPERTY);
		if (prop) {
			description = icalproperty_get_summary (prop);
	
			pos = 0;
			length = strlen (description);
			length2 = strlen (gtk_editable_get_chars 
						(GTK_EDITABLE (priv->description_box), 0, -1));
		
			if (length2 > 0)
				gtk_editable_delete_text (GTK_EDITABLE (priv->description_box), 0, length2);
		
			gtk_editable_insert_text (GTK_EDITABLE (priv->description_box),
						  description,
						  length,
						  &pos);
		}

		prop = icalcomponent_get_first_property (priv->comp, ICAL_DTSTART_PROPERTY);
		dtstart = icalproperty_get_dtstart (prop);
		prop = icalcomponent_get_first_property (priv->comp, ICAL_DTEND_PROPERTY);
		dtend = icalproperty_get_dtend (prop);

		tstart = icaltime_as_timet (dtstart);
		tend = icaltime_as_timet (dtend);

		gtk_label_set_text (GTK_LABEL (priv->dtstart_label), ctime (&tstart));
		gtk_label_set_text (GTK_LABEL (priv->dtend_label), ctime (&tend));

		/* Clear out any old-assed text that's been lying around in my message box. */
		gtk_editable_delete_text (GTK_EDITABLE (priv->message_text), 0, -1);

		prop = icalcomponent_get_first_property (priv->main_comp, ICAL_METHOD_PROPERTY);
		switch (icalproperty_get_method (prop)) {
		case ICAL_METHOD_PUBLISH:
			{
			GtkWidget *button;

			sprintf (message, "%s has published calendar information, "
				 	  "which you can add to your own calendar. "
				 	  "No reply is necessary.", 
				 priv->from_address);
			
			button =  gtk_button_new_with_label ("Add to Calendar");
			gtk_box_pack_start (GTK_BOX (priv->button_box), button, FALSE, FALSE, 3);
			gtk_widget_show (button);

			gtk_signal_connect (GTK_OBJECT (button), "clicked",
					    GTK_SIGNAL_FUNC (add_button_clicked_cb), priv);
			
			break;
			}
		case ICAL_METHOD_REQUEST:
			{
			/* I'll check if I have to rsvp. */
			icalproperty *prop;
			icalparameter *param;
			int rsvp = FALSE;

			prop = find_attendee (priv->comp, priv->my_address);
			if (prop) {
				param = get_icalparam_by_type (prop, ICAL_RSVP_PARAMETER);
	
				if (param) {
					if (icalparameter_get_rsvp (param))
						rsvp = TRUE;
				}
			}

			sprintf (message, "This is a meeting organized by %s, "
					  "who indicated that you %s RSVP.",
				 (priv->organizer ? priv->organizer : "an unknown person"), 
				 (rsvp ? "should" : "don't have to") );

			if (rsvp) {
				GtkWidget *accept_button, *decline_button, *tentative_button;

				accept_button = gtk_button_new_with_label ("Accept");
				decline_button = gtk_button_new_with_label ("Decline");
				tentative_button = gtk_button_new_with_label ("Tentative");

				gtk_box_pack_start (GTK_BOX (priv->button_box), accept_button, FALSE, FALSE, 3);
				gtk_box_pack_start (GTK_BOX (priv->button_box), decline_button, FALSE, FALSE, 3);
				gtk_box_pack_start (GTK_BOX (priv->button_box), tentative_button, FALSE, FALSE, 3);

				gtk_signal_connect (GTK_OBJECT (accept_button), "clicked",
						    GTK_SIGNAL_FUNC (accept_button_clicked_cb), priv);

				gtk_widget_show (accept_button);
				gtk_widget_show (decline_button);
				gtk_widget_show (tentative_button);
			}

			}
			break;
		default:
			sprintf (message, "I haven't the slightest notion what this calendar "
					  "object represents. Sorry.");
		}

		{
		int pos = 0;

		gtk_editable_insert_text (GTK_EDITABLE (priv->message_text), message,
					  strlen (message), &pos);
		}
	}

} /* pstream_load */

/*
 * This function implements the Bonobo::PersistStream:save method.
 */
static void
pstream_save (BonoboPersistStream *ps, const Bonobo_Stream stream,
	      Bonobo_Persist_ContentType type, void *data,
	      CORBA_Environment *ev)
{
	EItipControlPrivate *priv = data;
	int                  length;

	if (type && g_strcasecmp (type, "text/calendar") != 0 &&	    
	    g_strcasecmp (type, "text/x-calendar") != 0) {	    
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_Bonobo_Persist_WrongDataType, NULL);
		return;
	}

	/* Put something into vcalendar here. */
	length = strlen (priv->vcalendar);

	bonobo_stream_client_write (stream, priv->vcalendar, length, ev);
} /* pstream_save */

static CORBA_long
pstream_get_max_size (BonoboPersistStream *ps, void *data,
		      CORBA_Environment *ev)
{
	EItipControlPrivate *priv = data;
  
  	if (priv->vcalendar)
		return strlen (priv->vcalendar);
	else
		return 0L;
}

static Bonobo_Persist_ContentTypeList *
pstream_get_content_types (BonoboPersistStream *ps, void *closure,
			   CORBA_Environment *ev)
{
	return bonobo_persist_generate_content_types (2, "text/calendar", "text/x-calendar");
}

static void
get_prop ( BonoboPropertyBag *bag, BonoboArg *arg,
	   guint arg_id, gpointer user_data )
{
	EItipControlPrivate *priv = user_data;

	if (arg_id == FROM_ADDRESS_ARG_ID) {
		BONOBO_ARG_SET_STRING (arg, priv->from_address);
	}
	else if (arg_id == MY_ADDRESS_ARG_ID) {
		BONOBO_ARG_SET_STRING (arg, priv->my_address);
	}
}

static void
set_prop ( BonoboPropertyBag *bag, const BonoboArg *arg,
	   guint arg_id, gpointer user_data )
{
	EItipControlPrivate *priv = user_data;

	if (arg_id == FROM_ADDRESS_ARG_ID) {
		if (priv->from_address)
			g_free (priv->from_address);
	
		
		priv->from_address = g_strdup (BONOBO_ARG_GET_STRING (arg));

		/* Let's set the widget here, though I'm not sure if
		   it will work. */
		gtk_entry_set_text (GTK_ENTRY (priv->address_entry), priv->from_address);
		
	}
	else if (arg_id == MY_ADDRESS_ARG_ID) {
		if (priv->my_address)
			g_free (priv->my_address);
		
		priv->my_address = g_strdup (BONOBO_ARG_GET_STRING (arg));
	}
}


static BonoboObject *
e_itip_control_factory (BonoboGenericFactory *Factory, void *closure)
{
	BonoboControl      *control;
	BonoboPropertyBag *prop_bag;
	BonoboPersistStream *stream;
	EItipControlPrivate *priv;

	priv = g_new0 (EItipControlPrivate, 1);

	priv->xml = glade_xml_new (EVOLUTION_GLADEDIR "/" "e-itip-control.glade", "main_frame");

	/* Create the control. */
	priv->main_frame = glade_xml_get_widget (priv->xml, "main_frame");
	priv->organizer_entry = glade_xml_get_widget (priv->xml, "organizer_entry");
	priv->dtstart_label = glade_xml_get_widget (priv->xml, "dtstart_label");
	priv->dtend_label = glade_xml_get_widget (priv->xml, "dtend_label");
	priv->summary_entry = glade_xml_get_widget (priv->xml, "summary_entry");
	priv->description_box = glade_xml_get_widget (priv->xml, "description_box");
	/* priv->add_button = glade_xml_get_widget (priv->xml, "add_button"); */
	priv->button_box = glade_xml_get_widget (priv->xml, "button_box");
	priv->address_entry = glade_xml_get_widget (priv->xml, "address_entry");
	priv->message_text = glade_xml_get_widget (priv->xml, "message_text");

	gtk_text_set_word_wrap (GTK_TEXT (priv->message_text), TRUE);
	
	priv->xml2 = glade_xml_new (EVOLUTION_GLADEDIR "/" "e-itip-control.glade", "loading_window");
	priv->loading_progress = glade_xml_get_widget (priv->xml2, "loading_progress");
	priv->loading_window = glade_xml_get_widget (priv->xml2, "loading_window");

	gtk_signal_connect (GTK_OBJECT (priv->main_frame), "destroy",
			    GTK_SIGNAL_FUNC (itip_control_destroy_cb), priv);
	gtk_signal_connect (GTK_OBJECT (priv->main_frame), "size_request",
			    GTK_SIGNAL_FUNC (itip_control_size_request_cb), priv);

	gtk_widget_show (priv->main_frame);

	control = bonobo_control_new (priv->main_frame);

	/* create a property bag */
	prop_bag = bonobo_property_bag_new ( get_prop, set_prop, priv );
	bonobo_control_set_properties (control, prop_bag);

	bonobo_property_bag_add (prop_bag, "from_address", FROM_ADDRESS_ARG_ID, BONOBO_ARG_STRING, NULL,
				 "from_address", 0 );
	bonobo_property_bag_add (prop_bag, "my_address", MY_ADDRESS_ARG_ID, BONOBO_ARG_STRING, NULL,
				 "my_address", 0 );

	bonobo_control_set_automerge (control, TRUE);	

	stream = bonobo_persist_stream_new (pstream_load, pstream_save,
					    pstream_get_max_size,
					    pstream_get_content_types,
					    priv);

	if (stream == NULL) {
		bonobo_object_unref (BONOBO_OBJECT (control));
		return NULL;
	}

	bonobo_object_add_interface (BONOBO_OBJECT (control),
				    BONOBO_OBJECT (stream));

	return BONOBO_OBJECT (control);
}

void
e_itip_control_factory_init (void)
{
	static BonoboGenericFactory *factory = NULL;

	if (factory != NULL)
		return;

	factory =
		bonobo_generic_factory_new (
		        "OAFIID:control-factory:e_itipview:10441fcf-9a4f-4bf9-a026-d50b5462d45a",
			e_itip_control_factory, NULL);

	if (factory == NULL)
		g_error ("I could not register an iTip control factory.");
}

