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

#include "e-itip-control.h"
#include <cal-util/cal-component.h>
#include <cal-client/cal-client.h>


#define DEFAULT_WIDTH 500
#define DEFAULT_HEIGHT 400

extern gchar *evolution_dir;

typedef struct _EItipControlPrivate EItipControlPrivate;

struct _EItipControlPrivate {
	GladeXML *xml;
	GtkWidget *main_frame;
	GtkWidget *text_box;
	GtkWidget *organizer_entry, *dtstart_label, *dtend_label;
	GtkWidget *summary_entry, *description_box;
	GtkWidget *add_button;
	GtkWidget *loading_window;
	GtkWidget *loading_progress;

	icalcomponent *main_comp, *comp;
	CalComponent *cal_comp;
};


#if 0
static icalparameter *
get_icalparam_by_type (icalproperty *prop, icalparameter_kind kind)
{
	icalparameter *param;

	for (param = icalproperty_get_first_parameter (prop, ICAL_ANY_PARAMETER);
	     param != NULL && icalparameter_isa (param) != kind;
	     param = icalproperty_get_next_parameter (prop, ICAL_ANY_PARAMETER) );

	return param;
}
#endif

static void
itip_control_destroy_cb (GtkObject *object,
		    gpointer data)
{
	EItipControlPrivate *priv = data;

	gtk_object_unref (GTK_OBJECT (priv->xml));
	if (priv->main_comp != NULL) {
		icalcomponent_free (priv->main_comp);
	}

	if (priv->cal_comp != NULL) {
		gtk_object_unref (GTK_OBJECT (priv->cal_comp));
	}
	g_free (priv);
}
	
static void
itip_control_size_request_cb (GtkWidget *widget, GtkRequisition *requisition)
{
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
	
			dialog = gnome_ok_dialog("Component successfully added.");
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
add_button_clicked_cb (GtkWidget *widget, gpointer data)
{
	EItipControlPrivate *priv = data;
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
	gchar *vcalendar;
	gint pos, length, length2;

	if (type && g_strcasecmp (type, "text/calendar") != 0 &&	    
	    g_strcasecmp (type, "text/x-calendar") != 0) {	    
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_Bonobo_Persist_WrongDataType, NULL);
		return;
	}

	if ((vcalendar = stream_read (stream)) == NULL) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_Bonobo_Persist_FileNotFound, NULL);
		return;
	}

	/* Do something with the data, here. */
	pos = 0;
	length = strlen (vcalendar);
	length2 = strlen (gtk_editable_get_chars (GTK_EDITABLE (priv->text_box), 0, -1));
	

	if (length2 > 0) 
		gtk_editable_delete_text (GTK_EDITABLE (priv->text_box), 0, length2);

	gtk_editable_insert_text (GTK_EDITABLE (priv->text_box),
				  vcalendar,
				  length,
				  &pos);

	priv->main_comp = icalparser_parse_string (vcalendar);
	if (priv->main_comp == NULL) {
		g_printerr ("e-itip-control.c: the iCalendar data was invalid!\n");
		return;
	}

	priv->comp = icalcomponent_get_first_component (priv->main_comp,
							ICAL_ANY_COMPONENT);
	if (priv->comp == NULL) {
		g_printerr ("e-itip-control.c: I could not extract a proper component from\n"
			    "     the vCalendar data.\n");
		icalcomponent_free (priv->main_comp);
		return;
	}

	priv->cal_comp = cal_component_new ();
	if (cal_component_set_icalcomponent (priv->cal_comp, priv->comp) == FALSE) {
		g_printerr ("e-itip-control.c: I couldn't create a CalComponent from the iTip data.\n");
		gtk_object_unref (GTK_OBJECT (priv->cal_comp));
	}

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
	}
	
	pos = 0;
	length = strlen (vcalendar);
	length2 = strlen (gtk_editable_get_chars (GTK_EDITABLE (priv->text_box), 0, -1));
	

	if (length2 > 0) 
		gtk_editable_delete_text (GTK_EDITABLE (priv->text_box), 0, length2);

	gtk_editable_insert_text (GTK_EDITABLE (priv->text_box),
				  vcalendar,
				  length,
				  &pos);


	g_free (vcalendar);

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
	gchar                *vcalendar;
	int                  length;

	if (type && g_strcasecmp (type, "text/calendar") != 0 &&	    
	    g_strcasecmp (type, "text/x-calendar") != 0) {	    
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_Bonobo_Persist_WrongDataType, NULL);
		return;
	}

	/* Put something into vcalendar here. */
	length = gtk_text_get_length (GTK_TEXT (priv->text_box));
	vcalendar = gtk_editable_get_chars (GTK_EDITABLE (priv->text_box), 0, -1);

	bonobo_stream_client_write (stream, vcalendar, length, ev);
	g_free (vcalendar);
} /* pstream_save */

static CORBA_long
pstream_get_max_size (BonoboPersistStream *ps, void *data,
		      CORBA_Environment *ev)
{
	EItipControlPrivate *priv = data;
	gint length;
  
	length = gtk_text_get_length (GTK_TEXT (priv->text_box));

	return length;
}

static Bonobo_Persist_ContentTypeList *
pstream_get_content_types (BonoboPersistStream *ps, void *closure,
			   CORBA_Environment *ev)
{
	return bonobo_persist_generate_content_types (2, "text/calendar", "text/x-calendar");
}

static BonoboObject *
e_itip_control_factory (BonoboGenericFactory *Factory, void *closure)
{
	BonoboControl      *control;
	BonoboPersistStream *stream;
	EItipControlPrivate *priv;

	priv = g_new0 (EItipControlPrivate, 1);

	priv->xml = glade_xml_new (EVOLUTION_GLADEDIR "/" "e-itip-control.glade", "main_frame");

	/* Create the control. */
	priv->main_frame = glade_xml_get_widget (priv->xml, "main_frame");
	priv->text_box = glade_xml_get_widget (priv->xml, "text_box");
	priv->organizer_entry = glade_xml_get_widget (priv->xml, "organizer_entry");
	priv->dtstart_label = glade_xml_get_widget (priv->xml, "dtstart_label");
	priv->dtend_label = glade_xml_get_widget (priv->xml, "dtend_label");
	priv->summary_entry = glade_xml_get_widget (priv->xml, "summary_entry");
	priv->description_box = glade_xml_get_widget (priv->xml, "description_box");
	priv->add_button = glade_xml_get_widget (priv->xml, "add_button");
	priv->loading_progress = glade_xml_get_widget (priv->xml, "loading_progress");
	priv->loading_window = glade_xml_get_widget (priv->xml, "loading_window");

	gtk_text_set_editable (GTK_TEXT (priv->text_box), FALSE);
	
	gtk_signal_connect (GTK_OBJECT (priv->main_frame), "destroy",
			    GTK_SIGNAL_FUNC (itip_control_destroy_cb), priv);
	gtk_signal_connect (GTK_OBJECT (priv->main_frame), "size_request",
			    GTK_SIGNAL_FUNC (itip_control_size_request_cb), priv);
	gtk_signal_connect (GTK_OBJECT (priv->add_button), "clicked",
			    GTK_SIGNAL_FUNC (add_button_clicked_cb), priv);

	gtk_widget_show (priv->text_box);
	gtk_widget_show (priv->main_frame);

	control = bonobo_control_new (priv->main_frame);

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

