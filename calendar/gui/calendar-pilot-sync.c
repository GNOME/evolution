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

/* Our pi-socket address where we connect to */
struct pi_sockaddr addr;

/* The Pilot DB identifier for DateBook */
int db;

/* If true, enable debug output for alarms */
int debug_alarms = 0;

/* True if you want to dump the flags bits from the records */
int debug_attrs = 0;

/* Default values for alarms */
CalendarAlarm alarm_defaults[4] = {
	{ ALARM_MAIL, 0, 15, ALARM_MINUTES },
	{ ALARM_PROGRAM, 0, 15, ALARM_MINUTES },
	{ ALARM_DISPLAY, 0, 15, ALARM_MINUTES },
	{ ALARM_AUDIO, 0, 15, ALARM_MINUTES }
};

int only_desktop_to_pilot = 0;

int only_pilot_to_desktop = 0;

const struct poptOption calendar_sync_options [] = {
	{ "pilot", 0, POPT_ARG_STRING, &pilot_port, 0,
	  N_("Specifies the port on which the Pilot is"), N_("PORT") },
	{ "debug-attrs", 0, POPT_ARG_NONE, &debug_attrs, 0,
	  N_("If you want to debug the attributes on records"), NULL },
	{ "only-desktop", 0, POPT_ARG_NONE, &only_desktop_to_pilot, 0,
	  N_("Only syncs from desktop to pilot"), NULL },
	{ "only-pilot", 0, POPT_ARG_INT, &only_pilot_to_desktop, 0,
	  N_("Only syncs from pilot to desktop"), NULL },
	{ NULL, '\0', 0, NULL, 0 }
};

static void
conduit_free_Appointment (struct Appointment *a)
{
	/* free_Appointment is brain-dead with respect to guarding against
	   double-frees */
	
	free_Appointment (a);
	a->exception = 0;
	a->description = 0;
	a->note = 0;
}

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

	printf ("requesting %d [%s]\n", id, a->description);
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
		printf (_("\tObject did not exist, creating a new one\n"));
	} else {
		printf ("\tFound\n");
		obj = ical_object_new_from_string (vcal_string);
	}

	if (obj->pilot_status == ICAL_PILOT_SYNC_MOD){
		printf (_("\tObject has been modified on desktop and on the pilot, desktop takes precedence\n"));
		ical_object_destroy (obj);
		return;
	}

	/*
	 * Begin and end
	 */

	if (a->event)
	{
		/* turn day-long events into a full day's appointment */
		a->begin.tm_sec = 0;
		a->begin.tm_min = 0;
		a->begin.tm_hour = 6;

		a->end.tm_sec = 0;
		a->end.tm_min = 0;
		a->end.tm_hour = 10;
	}
	
	obj->dtstart = mktime (&a->begin);
	obj->dtend = mktime (&a->end);

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
				struct tm tm = *localtime (&obj->dtstart);

				obj->recur->weekday = 1 << tm.tm_wday;
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

/*
 * Sets the alarm for Appointment based on @alarm
 */
static int
try_alarm (CalendarAlarm *alarm, struct Appointment *a)
{
	if (!alarm->enabled)
		return 0;

	a->advance = alarm->count;
	switch (alarm->type){
	case ALARM_DAYS:
		a->advanceUnits = advDays;
		break;
		
	case ALARM_HOURS:
		a->advanceUnits = advHours;
		break;

	case ALARM_MINUTES:
		a->advanceUnits = advMinutes;
		break;

	default:
		return 0;
	}
	a->alarm = 1;
	return 1;
}

static void
sync_object_to_pilot (GNOME_Calendar_Repository repo, iCalObject *obj, int pilot_fd)
{
	char buffer [65536];
	struct Appointment *a;
	int wd, i, idx, attr, cat, rec_len;
	recordid_t new_id;
	GList *l;
	
	a = g_new0 (struct Appointment, 1);

	attr = 0;
	cat = 0;
	idx = 0;
	
	if (obj->pilot_id){
		rec_len = dlp_ReadRecordById (pilot_fd, db, obj->pilot_id,
					      buffer, &idx, &rec_len, &attr, &cat);

		if (rec_len > 0)
			unpack_Appointment (a, buffer, rec_len);
	} else {
		attr = 0;
		cat = 0;
	}
	
	/* a contains the appointment either cleared or with the data from the Pilot */
	a->begin = *localtime (&obj->dtstart);
	a->end   = *localtime (&obj->dtend);

	/* FIXME: add support for timeless */
	a->event = 0;

	/* Alarms, try the various ones.  Probably we should only do Audio?
	 * Otherwise going gnomecal->pilot->gnomecal would get the gnomecal
	 * with *possibly* an alarm that was not originally defined.
	 */
	a->alarm = 0;
	if (try_alarm (&obj->aalarm, a) == 0)
		if (try_alarm (&obj->dalarm, a) == 0)
			try_alarm (&obj->palarm, a);

	/* Recurrence */
	if (obj->recur){
		a->repeatFrequency = obj->recur->interval;
		
		switch (obj->recur->type){
		case RECUR_MONTHLY_BY_POS:
			a->repeatType = repeatMonthlyByDay;
			a->repeatFrequency = obj->recur->u.month_pos;
			a->repeatDay = obj->recur->weekday * 7;
			break;
			
		case RECUR_MONTHLY_BY_DAY:
			a->repeatType = repeatMonthlyByDate;
			a->repeatFrequency = obj->recur->u.month_day;
			break;
		
		case RECUR_YEARLY_BY_DAY:
			a->repeatType = repeatYearly;
			break;
			
		case RECUR_WEEKLY:
			for (wd = 0; wd < 7; wd++)
				if (obj->recur->weekday & (1 << wd))
					a->repeatDays [wd] = 1;
			a->repeatType = repeatWeekly;
			break;
		case RECUR_DAILY:
			
		default:
			a->repeatType = repeatNone;
			break;
		}
		if (obj->recur->enddate == 0){
			a->repeatForever = 1;
		} else
			a->repeatEnd = *localtime (&obj->recur->enddate);
	}
	
	/*
	 * Pilot uses a repeat-daily for a multi-day event, adjust for that case
	 */
	if ((a->end.tm_mday != a->begin.tm_mday) ||
	    (a->end.tm_mon != a->begin.tm_mon) ||
	    (a->end.tm_year != a->begin.tm_year)){
		a->event = 1;
		a->begin.tm_sec = 0;
		a->begin.tm_min = 0;
		a->begin.tm_hour = 0;

		a->end.tm_sec = 0;
		a->end.tm_min = 0;
		a->end.tm_hour = 0;

		a->repeatEnd = a->end;
		a->repeatForever = 0;
		a->repeatFrequency = 1;
		a->repeatType = repeatDaily;
	}
	   
	/*
	 * Exceptions
	 */
	a->exceptions = g_list_length (obj->exdate);
	a->exception = (struct tm *) malloc (sizeof (struct tm) * a->exceptions);
	for (i = 0, l = obj->exdate; l; l = l->next, i++){
		time_t *exdate = l->data;

		a->exception [i] = *localtime (exdate);
	}

	/*
	 * Description and note.
	 *
	 * We use strdup to be correct.  free_Appointment assumes we used
	 * malloc.
	 */
	if (obj->comment)
		a->note = strdup (obj->comment);
	else
		a->note = 0;

	if (obj->summary)
		a->description = strdup (obj->summary);
	else
		a->description = strdup (_("No description"));

	if (strcmp (obj->class, "PUBLIC") != 0)
		attr |= dlpRecAttrSecret;
	else
		attr &= ~dlpRecAttrSecret;

	/*
	 * Send the appointment to the pilot
	 */
	rec_len = pack_Appointment (a, buffer, sizeof (buffer));
	attr &= ~dlpRecAttrDirty;
	
	dlp_WriteRecord (
		pilot_fd, db, 0,
		obj->pilot_id, 0, buffer, rec_len, &new_id);
	
	GNOME_Calendar_Repository_update_pilot_id (repo, obj->uid, new_id, ICAL_PILOT_SYNC_NONE, &ev);
	
	conduit_free_Appointment (a);
	g_free (a);
}

static void
sync_cal_to_pilot (GNOME_Calendar_Repository repo, Calendar *cal, int pilot_fd)
{
	GList *l;
	int c = g_list_length (cal->events);
	int i;

	printf ("\n");
	for (i = 0, l = cal->events; l; l = l->next, i++){
		iCalObject *obj = l->data;

		printf ("Syncing desktop to pilot: %d/%d\r", i + 1, c);
		fflush (stdout);
		if (obj->pilot_status != ICAL_PILOT_SYNC_MOD){
			g_warning ("Strange, we were supposed to get only a dirty object");
			continue;
		}
		
		sync_object_to_pilot (repo, obj, pilot_fd);
	}
	printf ("\n");
}

static void
dump_attr (int flags)
{
	if (flags & dlpRecAttrDeleted)
		fprintf(stderr, " Deleted");
	if (flags & dlpRecAttrDirty)
		fprintf(stderr, " Dirty");
	if (flags & dlpRecAttrBusy)
		fprintf(stderr, " Busy");
	if (flags & dlpRecAttrSecret)
		fprintf(stderr, " Secret");
	if (flags & dlpRecAttrArchived)
		fprintf(stderr, " Archive");
	fprintf (stderr, "\n");
}

static void
sync_pilot (GNOME_Calendar_Repository repo, int pilot_fd)
{
	struct PilotUser user_info;
	int record;
	unsigned char buffer [65536];
	Calendar *dirty_cal;
	char *vcalendar_string;
	char *error;
	
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
	if (!only_desktop_to_pilot){
		for (record = 0;; record++){
			struct Appointment a;
			int rec_len, attr, size;
			recordid_t id;
			
			rec_len = dlp_ReadRecordByIndex (
				pilot_fd, db,
				record, buffer, &id, &size, &attr, 0);

			if (rec_len < 0)
				break;
			
			printf ("processing record %d\n", record);
			unpack_Appointment (&a, buffer, rec_len);
			
			if (debug_attrs)
				dump_attr (attr);
			
			/* If the object was deleted, remove it from the database */
			if (attr & dlpRecAttrDeleted){
				printf ("Deleting id %ld\n", id);
				delete_record (repo, id);
				conduit_free_Appointment (&a);
				dlp_DeleteRecord (pilot_fd, db, 0, id);
				continue;
			}

			if (attr & dlpRecAttrArchived)
				continue;
			
			printf ("updating record\n");
			update_record (repo, id, &a, attr);

			conduit_free_Appointment (&a);
		}
	}
	
	/*
	 * 2. Pull all the records from the Calendar, and move any new items
	 *    to the pilot
	 */
	if (!only_pilot_to_desktop){
		vcalendar_string = GNOME_Calendar_Repository_get_updated_objects (repo, &ev);
		dirty_cal = calendar_new ("Temporal",CALENDAR_INIT_NIL);
		error = calendar_load_from_memory (dirty_cal, vcalendar_string);
		if (!error)
			sync_cal_to_pilot (repo, dirty_cal, pilot_fd);
		calendar_destroy (dirty_cal);
	}
	
	
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

	printf ("<Syncing>\n");
	sync_pilot (repository, link);
	printf ("</Syncing>\n");
	
	GNOME_Calendar_Repository_done (repository, &ev);
	
	CORBA_exception_free (&ev);

	return 0;
}

/* Just a stub to link with */
void
calendar_notify (time_t time, CalendarAlarm *which, void *data)
{
}

