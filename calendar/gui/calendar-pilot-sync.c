/*
 * calendar-pilot-sync.c:
 *   
 * (C) 1999 International GNOME Support
 *
 * Author:
 *   Miguel de Icaza (miguel@gnome-support.com)
 *
 */
#include <config.h>
#include <gnome.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgnorba/gnome-factory.h>
#include <libgnorba/gnorba.h>
#include "calobj.h"
#include "calendar.h"
#include "timeutil.h"
#include "GnomeCal.h"
#include "pi-source.h"
#include "pi-socket.h"
#include "pi-datebook.h"
#include "pi-dlp.h"

/* the CORBA ORB */
CORBA_ORB orb;

/* The default port to communicate with */
char *pilot_port = "/dev/pilot";

CORBA_Environment ev;

struct pi_sockaddr addr;

const struct poptOption calendar_sync_options [] = {
	{ "pilot", 0, POPT_ARG_STRING, &pilot_port, 0,
	  N_("Specifies the port on which the Pilot is"), N_("PORT") },
	{ NULL, '\0', 0, NULL, 0 }
};

static int
setup_connection (void)
{
	int socket;
	int ret, news;
	
	if (!(socket = pi_socket(PI_AF_SLP, PI_SOCK_STREAM, PI_PF_PADP))) 
		g_error (_("Can not create Pilot socket\n"));

	addr.pi_family = PI_AF_SLP;
	strncpy ((void *) &addr.pi_device, pilot_port, sizeof (addr.pi_device));

	ret = pi_bind (socket, (struct sockaddr *)&addr, sizeof (addr));
	if (ret == -1)
		g_error (_("Can not bind to device %s\n"), pilot_port);

	if (pi_listen (socket, 1) == -1)
		g_error (_("Failed to get a connection from the Pilot device"));

	if ((news = pi_accept (socket, 0, 0)) == -1)
		g_error (_("pi_accept failed"));

	return news;
}

static GNOME_Calendar_Repository
locate_calendar_server (void)
{
	GNOME_Calendar_Repository repo;
	GNOME_stringlist list;
	
	repo = goad_server_activate_with_id (
		NULL, "IDL:GNOME:Calendar:Repository:1.0",
		0, NULL);
	
	if (repo == CORBA_OBJECT_NIL)
		g_error ("Can not communicate with GnomeCalendar server");

	if (ev._major != CORBA_NO_EXCEPTION){
		printf ("Exception: %s\n", CORBA_exception_id (&ev));
		abort ();
	}
	
	return repo;
}

static void
delete_record (GNOME_Calendar_Repository repo, int id)
{
	char *uid;

	uid = GNOME_Calendar_Repository_get_id_from_pilot_id (repo, id, &ev);

	/* The record was already deleted */
	if (ev._major != CORBA_NO_EXCEPTION)
		return;
	
	GNOME_Calendar_Repository_delete_object (repo, uid, &ev);
	CORBA_free (uid);
}

static void
update_record (GNOME_Calendar_Repository repo, int id, struct Appointment *a, int attr)
{
	char *vcal_string;
	iCalObject *obj;
	int i;
	char *str;
	
	obj = ical_new (a->note ? a->note : "",
			g_get_user_name (),
			a->description ? a->description : "");

	vcal_string = GNOME_Calendar_Repository_get_object_by_pilot_id (repo, id, &ev);

	if (ev._major == CORBA_USER_EXCEPTION){
		time_t now = time (NULL);
		
		obj->created = now;
		obj->last_mod = now;
		obj->priority = 0;
		obj->transp = 0;
		obj->related = NULL;
		obj->pilot_id = id;
		obj->pilot_status = ICAL_PILOT_SYNC_NONE;
		printf (_("Object did not exist, creating a new one"));
	} else
		obj = ical_object_new_from_string (vcal_string);

	if (obj->pilot_status == ICAL_PILOT_SYNC_MOD){
		printf (_("Object has been modified on desktop and on the pilot, desktop takes precedence"));
		ical_object_destroy (obj);
		return;
	}

	/*
	 * Begin and end
	 */
	obj->dtstart = mktime (&a->begin);
	obj->dtend   = mktime (&a->end);

	/* Special case: daily repetitions are converted to a multi-day event */
	if (a->repeatType == repeatDaily){
		time_t newt = time_add_day (obj->dtend, a->repeatFrequency);

		obj->dtend = newt;
	}

	/*
	 * Alarm
	 */
	if (a->alarm){
		obj->aalarm.type = ALARM_AUDIO;
		obj->aalarm.enabled = 1;
		obj->aalarm.count = a->advance;

		switch (a->advanceUnits){
		case advMinutes:
			obj->aalarm.units = ALARM_MINUTES;
			break;
			
		case advHours:
			obj->aalarm.units = ALARM_HOURS;
			break;
			
		case advDays:
			obj->aalarm.units = ALARM_DAYS;
			break;
		default:
		}
	}

	/*
	 * Recurrence
	 */
	if (a->repeatFrequency && a->repeatType != repeatDaily){
		obj->recur = g_new0 (Recurrence, 1);
		
		switch (a->repeatType){
		case repeatDaily:
			/*
			 * In the Pilot daily repetitions are actually
			 * multi-day events
			 */
			g_warning ("Should not have got here");
			break;
			
		case repeatMonthlyByDate:
			obj->recur->type = RECUR_MONTHLY_BY_DAY;
			obj->recur->u.month_day = a->repeatFrequency;
			break;
			
		case repeatWeekly:
		{
			int wd;

			obj->recur->type = RECUR_WEEKLY;
			for (wd = 0; wd < 7; wd++)
				if (a->repeatDays [wd])
					obj->recur->weekday |= 1 << wd;

			if (obj->recur->weekday == 0){
				struct tm *tm = localtime (&obj->dtstart);

				obj->recur->weekday = 1 << tm->tm_wday;
			}
			break;
		}
		
		case repeatMonthlyByDay:
			obj->recur->type = RECUR_MONTHLY_BY_POS;
			obj->recur->u.month_pos = a->repeatFrequency;
			obj->recur->weekday = (a->repeatDay / 7);
			break;
			
		case repeatYearly:
			obj->recur->type = RECUR_YEARLY_BY_DAY;
			break;

		default:
			g_warning ("Unhandled repeate case");
		}

		if (a->repeatForever)
			obj->recur->duration = 0;
		else
			obj->recur->_enddate = mktime (&a->repeatEnd);
	}

	/*
	 * Load exception dates 
	 */
	obj->exdate = NULL;
	for (i = 0; i < a->exceptions; i++){
		time_t *t = g_new (time_t, 1);

		*t = mktime (&(a->exception [i]));
		obj->exdate = g_list_prepend (obj->exdate, t);
	}

	g_free (obj->class);
	
	if (attr & dlpRecAttrSecret)
		obj->class = g_strdup ("PRIVATE");
	else
		obj->class = g_strdup ("PUBLIC");

	/*
	 * Now, convert the in memory iCalObject to a full vCalendar we can send
	 */
	str = calendar_string_from_object (obj);

	GNOME_Calendar_Repository_update_object (repo, obj->uid, str, &ev);

	free (str);
	
	/*
	 * Shutdown
	 */
	ical_object_destroy (obj);
}

static void
sync_pilot (GNOME_Calendar_Repository repo, int pilot_fd)
{
	struct PilotUser user_info;
	int db,record;
	unsigned char buffer [65536];
	
	printf (_("Syncing with the pilot..."));
	dlp_ReadUserInfo (pilot_fd, &user_info);

	/* This informs the user of the progress on the Pilot */
	dlp_OpenConduit (pilot_fd);

	if (dlp_OpenDB (pilot_fd, 0, 0x80 | 0x40, "DatebookDB", &db) < 0){
		g_warning (_("Could not open DatebookDB on the Pilot"));
		dlp_AddSyncLogEntry (pilot_fd, _("Unable to open DatebookDB"));
		pi_close (pilot_fd);
		exit (1);
	}

	/*
	 * 1. Pull all the records from the Pilot, and make any updates
	 *    required on the desktop side
	 */
	for (record = 0;; record++){
		struct Appointment a;
		int rec_len, attr, size;
		recordid_t id;

		rec_len = dlp_ReadRecordByIndex (pilot_fd, db, record, buffer, &id, &size, &attr, 0);

		if (rec_len < 0)
			break;

		printf ("processing record %d\n", record);
		unpack_Appointment (&a, buffer, rec_len);
		
		/* If the object was deleted, remove it from the database */
		if (attr & dlpRecAttrDeleted){
			delete_record (repo, id);
			continue;
		}

		if (attr & dlpRecAttrDirty){
			printf ("updating record\n");
			update_record (repo, id, &a, attr);
		}

		free_Appointment (&a);
	}
	/*
	 * 2. Pull all the records from the Calendar, and move any new items
	 *    to the pilot
	 */
	dlp_CloseDB (pilot_fd, db);
	dlp_AddSyncLogEntry (pilot_fd, _("Synced DateBook from Pilot to GnomeCal"));
	pi_close (pilot_fd);
}

int
main (int argc, char *argv [])
{
	int link;
	GNOME_Calendar_Repository repository;
	
	CORBA_exception_init (&ev);
	orb = gnome_CORBA_init_with_popt_table (
		"calendar-pilot-sync", VERSION, &argc, argv,
		calendar_sync_options, 0, NULL, 0, &ev);

	printf ("Please, press HotSync button on the palm...");
	fflush (stdout);
	link = setup_connection ();
	printf ("Connected\n");

	printf ("Launching GnomeCal...");
	fflush (stdout);
	repository = locate_calendar_server ();
	printf ("Done\n");

	printf ("Syncing...\n");
	sync_pilot (repository, link);
	printf ("Done Syncing\n");
	
	GNOME_Calendar_Repository_done (repository, &ev);
	
	CORBA_exception_free (&ev);

	return 0;
}

/* Just a stub to link with */
void
calendar_notify (time_t time, CalendarAlarm *which, void *data)
{
}

