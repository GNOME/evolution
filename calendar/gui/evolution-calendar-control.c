/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#include <config.h>
#include <gnome.h>
#include <glade/glade.h>
#include <bonobo.h>
#include <bonobo/bonobo-control.h>
#include <glade/glade.h>

#ifdef USING_OAF
#include <liboaf/liboaf.h>
#else
#include <libgnorba/gnorba.h>
#endif

#include <cal-util/timeutil.h>
#include <gui/alarm.h>
#include <gui/eventedit.h>
#include <gui/gnome-cal.h>
#include <gui/calendar-commands.h>

#define PROPERTY_CALENDAR_URI "folder_uri"

#define PROPERTY_CALENDAR_URI_IDX 1

#ifdef USING_OAF
#define CONTROL_FACTORY_ID "OAFIID:control-factory:calendar:f4f90989-0f50-4af2-ad94-8bbdf331f0bc"
#else
#define CONTROL_FACTORY_ID "control-factory:calendar"
#endif

CORBA_Environment ev;
CORBA_ORB orb;


static void
control_activate_cb (BonoboControl *control, 
		     gboolean activate, 
		     gpointer user_data)
{
	if (activate)
		calendar_control_activate (control, user_data);
	else
		calendar_control_deactivate (control);
}



static void
init_bonobo (int *argc, char **argv)
{
#ifdef USING_OAF
	/* FIXME: VERSION instead of "0.0".  */
	gnome_init_with_popt_table ("evolution-calendar", "0.0",
				    *argc, argv, oaf_popt_options,
				    0, NULL);
	oaf_init (*argc, argv);
#else
	gnome_CORBA_init_with_popt_table (
		"evolution-calendar", "0.0",
		argc, argv, NULL, 0, NULL, GNORBA_INIT_SERVER_FUNC, &ev);
#endif

	if (bonobo_init (CORBA_OBJECT_NIL, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL) == FALSE)
		g_error (_("Could not initialize Bonobo"));

	glade_gnome_init ();
}



static void
get_prop (BonoboPropertyBag *bag,
	  BonoboArg         *arg,
	  guint              arg_id,
	  gpointer           user_data)
{
	/*GnomeCalendar *gcal = user_data;*/

	switch (arg_id) {

	case PROPERTY_CALENDAR_URI_IDX:
		/*
		if (fb && fb->uri)
			BONOBO_ARG_SET_STRING (arg, fb->uri);
		else
			BONOBO_ARG_SET_STRING (arg, "");
		*/
		break;

	default:
		g_warning ("Unhandled arg %d\n", arg_id);
	}
}

static void
set_prop (BonoboPropertyBag *bag,
	  const BonoboArg   *arg,
	  guint              arg_id,
	  gpointer           user_data)
{
	GnomeCalendar *gcal = user_data;
	char *filename;

	switch (arg_id) {

	case PROPERTY_CALENDAR_URI_IDX:
		printf ("set_prop: '%s'\n", BONOBO_ARG_GET_STRING (arg));
		filename = g_strdup_printf ("%s/calendar.vcf",
					    BONOBO_ARG_GET_STRING (arg));
		calendar_set_uri (gcal, filename);
		g_free (filename);
		break;

	default:
		g_warning ("Unhandled arg %d\n", arg_id);
		break;
	}
}


static void
calendar_properties_init (GnomeCalendar *gcal)
{
	gcal->properties = bonobo_property_bag_new (get_prop, set_prop, gcal);

	bonobo_property_bag_add (gcal->properties,
				 PROPERTY_CALENDAR_URI,
				 PROPERTY_CALENDAR_URI_IDX,
				 BONOBO_ARG_STRING,
				 NULL,
				 _("The URI that the calendar will display"),
				 0);

	bonobo_control_set_property_bag (gcal->control, gcal->properties);
}



static BonoboObject *
calendar_factory (BonoboGenericFactory *Factory, void *closure)
{
	BonoboControl *control;

	/* Create the control. */
	GnomeCalendar *cal = new_calendar (full_name, NULL, NULL, 0);

	gtk_widget_show (GTK_WIDGET (cal));

	control = bonobo_control_new (GTK_WIDGET (cal));
	cal->control = control;

	calendar_properties_init (cal);

	gtk_signal_connect (GTK_OBJECT (control), "activate",
			    control_activate_cb, cal);

	return BONOBO_OBJECT (control);
}


static void
calendar_factory_init (void)
{
	static BonoboGenericFactory *calendar_control_factory = NULL;

	if (calendar_control_factory != NULL)
		return;

	puts ("XXXXXX - initializing calendar factory!!!");

	calendar_control_factory =
		bonobo_generic_factory_new (CONTROL_FACTORY_ID,
					    calendar_factory, NULL);

	if (calendar_control_factory == NULL) {
		g_error ("I could not register a Calendar factory.");
	}
}


int
main (int argc, char **argv)
{
	init_bonobo (&argc, argv);
	glade_gnome_init ();
	alarm_init ();

	init_calendar ();

	//g_log_set_always_fatal ((GLogLevelFlags) 0xFFFF);
	g_log_set_always_fatal (G_LOG_LEVEL_ERROR |
				G_LOG_LEVEL_CRITICAL |
				G_LOG_LEVEL_WARNING);

	CORBA_exception_init (&ev);

	calendar_factory_init ();

	bonobo_main ();

	return 0;
}
