/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Main file for the GNOME Calendar program
 * Copyright (C) 1998 the Free Software Foundation
 *
 * Authors:
 *          Miguel de Icaza (miguel@kernel.org)
 *          Federico Mena (federico@helixcode.com)
 */

#include <config.h>

#include <pwd.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <gnome.h>
#include <libgnorba/gnorba.h>
#include <bonobo.h>
#include <cal-util/timeutil.h>
#include <gui/alarm.h>
#include <gui/eventedit.h>
#include <gui/gnome-cal.h>
#include <gui/calendar-commands.h>


enum {
	GEOMETRY_KEY = -1,
	USERFILE_KEY = -2,
	VIEW_KEY     = -3,
	HIDDEN_KEY   = -4,
	TODO_KEY     = -5,
	DEBUG_KEY    = -6
};

/* Lists used to startup various GnomeCalendars */
static GList *start_calendars;
static GList *start_geometries;
static GList *start_views;

/* For dumping part of a calendar */
static time_t from_t, to_t;

/* If set, show events for the specified date and quit */
/*static int show_events;*/

/* If set, show todo items quit */
/*static int show_todo;*/

/* If true, do not show our top level window */
int startup_hidden = 0;

/* File to load instead of the user default's file */
static char *load_file;

extern time_t get_date ();

static void
process_dates (void)
{
	if (!from_t)
		from_t = time_day_begin (time (NULL));

	if (!to_t || to_t < from_t)
		to_t = time_add_day (from_t, 1);
}


static int
same_day (struct tm *a, struct tm *b)
{
	return (a->tm_mday == b->tm_mday &&
		a->tm_mon  == b->tm_mon &&
		a->tm_year == b->tm_year);
}


#if 0
static void
dump_events (void)
{
	CalClient *calc;
	gboolean r;
	GList *l;
	time_t now = time (NULL);
	struct tm today = *localtime (&now);

	process_dates ();
	init_calendar ();

	/* FIXME: this is not waiting for the calendar to be loaded */

	/* DELETE
	cal = calendar_new (full_name, CALENDAR_INIT_ALARMS);
	s = calendar_load (cal, load_file ? load_file : user_calendar_file);
	if (s){
		printf ("error: %s\n", s);
		exit (1);
	}
	*/

	r = cal_client_load_calendar (calc,
	       		    load_file ? load_file : user_calendar_file);
	if (r == FALSE) {
		printf ("error: loading %s\n",
			load_file ? load_file : user_calendar_file);
		exit (1);
	}
	l = calendar_get_events_in_range (calc, from_t, to_t);

	for (; l; l = l->next){
		char start [80], end [80];
		CalendarObject *co = l->data;
		struct tm ts, te;
		char *format;

		ts = *localtime (&co->ev_start);
		te = *localtime (&co->ev_end);

		if (same_day (&today, &ts))
			format = N_("%H:%M");
		else
			format = N_("%A %b %d, %H:%M");
		strftime (start, sizeof (start), _(format), &ts);

		if (!same_day (&ts, &te))
			format = N_("%A %b %d, %H:%M");
		strftime (end,   sizeof (start), _(format), &te);

		printf ("%s -- %s\n", start, end);
		printf ("  %s\n", co->ico->summary);
	}
	/* calendar_destroy_event_list (l); DELETE / FIXME */
	/* calendar_destroy (cal); DELETE */
	exit (0);
}
#endif /* 0 */


#if 0
static void
dump_todo (void)
{
	CalClient *calc;
	gboolean r;
	GList *l;

	process_dates ();
	init_calendar ();

	/* FIXME: this is not waiting for the calendar to be loaded */

	/* DELETE
	cal = calendar_new (full_name, CALENDAR_INIT_ALARMS);
	s = calendar_load (cal, load_file ? load_file : user_calendar_file);
	if (s){
		printf ("error: %s\n", s);
		exit (1);
	}
	*/

	r = cal_client_load_calendar (calc,
	       		    load_file ? load_file : user_calendar_file);
	if (r == FALSE) {
		printf ("error: loading %s\n",
			load_file ? load_file : user_calendar_file);
		exit (1);
	}
	l = calendar_get_events_in_range (calc, from_t, to_t);

	for (; l; l = l->next){
		CalendarObject *co = l->data;
		iCalObject *object = co->ico;

		if (object->type != ICAL_TODO)
			continue;

		printf ("[%s]: %s\n",object->organizer->addr, object->summary);
	}
	/* calendar_destroy (cal); DELETE */
	exit (0);
}
#endif /* 0 */


static void
session_die (void)
{
	quit_cmd (NULL, NULL, NULL);
}

/*
 * Save the session callback
 */
static int
session_save_state (GnomeClient *client, gint phase, GnomeRestartStyle save_style, gint shutdown,
		    GnomeInteractStyle  interact_style, gint fast, gpointer client_data)
{
	char *sess_id;
	char **argv = (char **) g_malloc (sizeof (char *) * ((active_calendars * 6) + 3));
	GList *l, *free_list = 0;
	int   i;

	sess_id = gnome_client_get_id (client);

	argv [0] = client_data;
	for (i = 1, l = all_calendars; l; l = l->next){
		GnomeCalendar *gcal = GNOME_CALENDAR (l->data);
		char *geometry;

		geometry = gnome_geometry_string (GTK_WIDGET (gcal)->window);

		/* FIX ME
		if (strcmp (gcal->client->filename, user_calendar_file) == 0)
			argv [i++] = "--userfile";
		else {
			argv [i++] = "--file";
			argv [i++] = gcal->client->filename;
		}
		*/

		argv [i++] = "--geometry";
		argv [i++] = geometry;
		argv [i++] = "--view";
		argv [i++] = gnome_calendar_get_current_view_name (gcal);
		free_list = g_list_append (free_list, geometry);
	}
	argv [i] = NULL;
	gnome_client_set_clone_command (client, i, argv);
	gnome_client_set_restart_command (client, i, argv);

	for (l = free_list; l; l = l->next)
		g_free (l->data);
	g_list_free (free_list);

	return 1;
}

static void
parse_an_arg (poptContext ctx,
	      enum poptCallbackReason reason,
	      const struct poptOption *opt,
	      char *arg, void *data)
{
	switch (opt->val){
	case 'f':
		from_t = get_date (arg, NULL);
		break;

	case 't':
		to_t = get_date (arg, NULL);
		break;

	case GEOMETRY_KEY:
		start_geometries = g_list_append (start_geometries, arg);
		break;

	case USERFILE_KEY:
		/* This is a special key that tells the program to load
		 * the user's calendar file.  This allows session management
		 * to work even if the User's home directory changes location
		 * (ie, on a networked setup).
		 */
		arg = COOKIE_USER_HOME_DIR;
		/* fall through */
		break;
		
	case VIEW_KEY:
		start_views = g_list_append (start_views, arg);
		break;

	case 'F':
		start_calendars = g_list_append (start_calendars, arg);
		break;

		/*
	case TODO_KEY:
		show_todo = 1;
		break;
		*/

		/*
	case 'e':
		show_events = 1;
		break;
		*/

	case HIDDEN_KEY:
		startup_hidden = 1;
		break;

	case DEBUG_KEY:
		if (!g_strcasecmp (arg, "alarms"))
			debug_alarms = 1;
		break;
		
	default:
	}
}




static const struct poptOption options [] = {
	{ NULL, '\0', POPT_ARG_CALLBACK, parse_an_arg, 0, NULL, NULL },
	{ "events", 'e', POPT_ARG_NONE, NULL, 'e', N_("Show events and quit"),
	  NULL },
	{ "todo",   0,   POPT_ARG_NONE, NULL, TODO_KEY, N_("Show TO-DO items and quit"),
	  NULL },
	{ "from", 'f', POPT_ARG_STRING, NULL, 'f', N_("Specifies start date [for --events]"), N_("DATE") },
	{ "file", 'F', POPT_ARG_STRING, NULL, 'F', N_("File to load calendar from"), N_("FILE") },
	{ "userfile", '\0', POPT_ARG_NONE, NULL, USERFILE_KEY, N_("Load the user calendar"), NULL },
	{ "geometry", '\0', POPT_ARG_STRING, NULL, GEOMETRY_KEY, N_("Geometry for starting up"), N_("GEOMETRY") },
	{ "view", '\0', POPT_ARG_STRING, NULL, VIEW_KEY, N_("The startup view mode (dayview, weekview, monthview, yearview)"), N_("VIEW") },
	{ "to", 't', POPT_ARG_STRING, NULL, 't', N_("Specifies ending date [for --events]"), N_("DATE") },
	{ "hidden", 0, POPT_ARG_NONE, NULL, HIDDEN_KEY, N_("If used, starts in iconic mode"), NULL },
	{ "debug", 'd', POPT_ARG_STRING, NULL, DEBUG_KEY, N_("Enable debugging output of TYPE (alarms)"), N_("TYPE") },
	{ NULL, '\0', 0, NULL, 0}
};


int
main (int argc, char **argv)
{
	GnomeClient *client;
	GtkWidget *cal_window;
	GnomeCalendar *cal_frame;

	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	textdomain (PACKAGE);

#ifdef USING_OAF
	gnome_init_with_popt_table ("calendar", VERSION, argc, argv, oaf_popt_options,
				    0, NULL);
	oaf_init (argc, argv);
#else
	{
		CORBA_Environment ev;

		CORBA_exception_init (&ev);
		gnome_CORBA_init_with_popt_table ("calendar", VERSION, &argc, argv,
						  options, 0, NULL, 0, &ev);
		if (ev._major != CORBA_NO_EXCEPTION) {
			g_message ("main(): could not initialize the ORB");
			CORBA_exception_free (&ev);
			exit (EXIT_FAILURE);
		}
		CORBA_exception_free (&ev);
	}
#endif

	if (!bonobo_init (CORBA_OBJECT_NIL, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL)) {
		g_message ("main(): could not initialize Bonobo");
		exit (EXIT_FAILURE);
	}
	
	process_dates ();

#       if 0
	if (show_events)
		dump_events ();

	if (show_todo)
		dump_todo ();
#       endif /* 0 */
	
	client = gnome_master_client ();
	if (client) {
		gtk_signal_connect (GTK_OBJECT (client), "save_yourself",
				    GTK_SIGNAL_FUNC (session_save_state), argv [0]);
		gtk_signal_connect (GTK_OBJECT (client), "die",
				    GTK_SIGNAL_FUNC (session_die), NULL);
	}

	alarm_init ();
	init_calendar ();

	/* FIXME: the following is broken-ish, since geometries/views are not matched
	 * to calendars, but they are just picked in whatever order they came in
	 * from the command line.
	 */

	/* Load all of the calendars specified in the command line with the
	 * geometry specified, if any.
	 */
	if (start_calendars) {
		GList *p, *g, *v;
		char *title;

		p = start_calendars;
		g = start_geometries;
		v = start_views;

		while (p) {
			char *file = p->data;
			char *geometry = g ? g->data : NULL;
			char *page_name = v ? v->data : NULL;

			if (file == COOKIE_USER_HOME_DIR)
				file = user_calendar_file;

			if (strcmp (file, user_calendar_file) == 0)
				title = full_name;
			else
				title = file;

			cal_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
			cal_frame = new_calendar (title, file, geometry, page_name, startup_hidden);
			gtk_container_add (GTK_CONTAINER (cal_window), GTK_WIDGET (cal_frame));
			gtk_widget_show (cal_window);

			p = p->next;
			if (g)
				g = g->next;
			if (v)
				v = v->next;
		}
		g_list_free (p);
	} else {
		char *geometry = start_geometries ? start_geometries->data : NULL;
		char *page_name = start_views ? start_views->data : NULL;

		cal_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
		cal_frame = new_calendar (full_name, user_calendar_file, geometry, page_name, startup_hidden);
		gtk_container_add (GTK_CONTAINER (cal_window), GTK_WIDGET (cal_frame));
		gtk_widget_show (cal_window);
	}

	bonobo_main ();
	return 0;
}
