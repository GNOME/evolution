/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#include <config.h>
#include <gnome.h>
#include <libgnorba/gnorba.h>
#include <bonobo.h>
#include <bonobo/bonobo-control.h>


#include <cal-util/timeutil.h>
#include <gui/alarm.h>
#include <gui/eventedit.h>
#include <gui/gnome-cal.h>
#include <gui/calendar-commands.h>
/*#include <control/calendar-control.h>*/


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


static BonoboObject *
calendar_factory (BonoboGenericFactory *Factory, void *closure)
{
	BonoboControl      *control;

	/* Create the control. */
	GnomeCalendar *cal = new_calendar ("title", NULL, NULL, NULL, 0);
	gtk_widget_show (GTK_WIDGET (cal));

	control = bonobo_control_new (GTK_WIDGET (cal));

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

	calendar_control_factory =
		bonobo_generic_factory_new ("control-factory:calendar",
					    calendar_factory, NULL);

	if (calendar_control_factory == NULL) {
		g_error ("I could not register a Calendar factory.");
	}
}


static void
init_bonobo (int argc, char **argv)
{
	gnome_CORBA_init_with_popt_table (
		"evolution-calendar", "0.0",
		&argc, argv, NULL, 0, NULL, GNORBA_INIT_SERVER_FUNC, &ev);

	orb = gnome_CORBA_ORB ();

	if (bonobo_init (orb, NULL, NULL) == FALSE)
		g_error (_("Could not initialize Bonobo"));
}

int
main (int argc, char **argv)
{
	alarm_init ();
	init_calendar ();

	CORBA_exception_init (&ev);

	init_bonobo (argc, argv);

	calendar_factory_init ();

	bonobo_main ();

	return 0;
}
