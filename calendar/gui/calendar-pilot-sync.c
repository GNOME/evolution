/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * calendar-pilot-sync.c:
 *   
 * (C) 1999 International GNOME Support
 *
 * Author:
 *   Miguel de Icaza (miguel@gnome-support.com)
 *
 */


/*
 *
 * this only works in a monogamous pilot/desktop situation.
 *
 */



#include <config.h>
#include <gnome.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bonobo.h>
#include <bonobo/bonobo-control.h>

#ifdef USING_OAF
#include <liboaf/liboaf.h>
#else
#include <libgnorba/gnorba.h>
#endif

#include <cal-client/cal-client.h>
#include "cal-util/calobj.h"
#include "cal-util/timeutil.h"
#include "pi-source.h"
#include "pi-socket.h"
#include "pi-datebook.h"
#include "pi-dlp.h"


char *calendar_file;

/* The default port to communicate with */
char *pilot_port = "/dev/pilot";

/* Our pi-socket address where we connect to */
struct pi_sockaddr addr;

/* The Pilot DB identifier for DateBook */
int db;

/* If true, enable debug output for alarms */
int debug_alarms = 0;

/* True if you want to dump the flags bits from the records */
int debug_attrs = 0;

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
	strncpy ((void *)&addr.pi_device, pilot_port, sizeof (addr.pi_device));

	ret = pi_bind (socket, (struct sockaddr *)&addr, sizeof (addr));
	if (ret == -1)
		g_error (_("Can not bind to device %s\n"), pilot_port);

	if (pi_listen (socket, 1) == -1)
		g_error (_("Failed to get a connection "
			   "from the Pilot device"));

	if ((news = pi_accept (socket, 0, 0)) == -1)
		g_error (_("pi_accept failed"));

	return news;
}


static void
init_bonobo (int *argc, char **argv)
{
#       ifdef USING_OAF
	/* FIXME: VERSION instead of "0.0".  */
	gnome_init_with_popt_table ("evolution-calendar", "0.0",
				    *argc, argv, oaf_popt_options,
				    0, NULL);
	oaf_init (*argc, argv);
#      else
	CORBA_Environment ev;

	CORBA_exception_init (&ev);
	gnome_CORBA_init_with_popt_table (
		"evolution-calendar", "0.0",
		argc, argv, NULL, 0, NULL, GNORBA_INIT_SERVER_FUNC, &ev);
	CORBA_exception_free (&ev);
#       endif

	if (bonobo_init (CORBA_OBJECT_NIL,
			 CORBA_OBJECT_NIL, CORBA_OBJECT_NIL) == FALSE)
		g_error (_("Could not initialize Bonobo"));
}


static void
delete_record_from_desktop (CalClient *client, int id)
{
	char *uid;
	CalClientGetStatus status;

	status = cal_client_get_uid_by_pilot_id (client, id, &uid);

	if (status == CAL_CLIENT_GET_SUCCESS) {
		cal_client_remove_object (client, uid);
		g_free (uid);
	}
}



static void
dump_attr (int flags)
{
	if (flags & dlpRecAttrDeleted)
		printf (" Deleted");
	if (flags & dlpRecAttrDirty)
		printf (" Dirty");
	if (flags & dlpRecAttrBusy)
		printf (" Busy");
	if (flags & dlpRecAttrSecret)
		printf (" Secret");
	if (flags & dlpRecAttrArchived)
		printf (" Archive");
	printf ("\n");
}



/* take a record retrieved from a pilot and merge it into the desktop cal */

static void
update_record (CalClient *client, int id, struct Appointment *a, int attr)
{
	iCalObject *obj;
	int i;
	CalClientGetStatus status;
	char *uid = NULL;
	gboolean success;
	
	printf ("pilot->cal: %d, ", id);

	status = cal_client_get_uid_by_pilot_id (client, id, &uid);
	if (status == CAL_CLIENT_GET_SUCCESS)
		status = cal_client_get_object (client, uid, &obj);

	if (status != CAL_CLIENT_GET_SUCCESS) {
		/* Object did not exist, creating a new one */
		time_t now = time (NULL);

		obj = ical_new (a->note ? a->note : "",
				g_get_user_name (),
				a->description ? a->description : "");
		
		obj->created = now;
		obj->last_mod = now;
		obj->priority = 0;
		obj->transp = 0;
		obj->related = NULL;
		obj->pilot_id = id;
		obj->pilot_status = ICAL_PILOT_SYNC_NONE;
	}

	if (obj->pilot_status == ICAL_PILOT_SYNC_MOD) {
		printf (_("\tObject has been modified on desktop and on "
			  "the pilot, desktop takes precedence\n"));
		ical_object_unref (obj);
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

	if (a->note)
		obj->comment = g_strdup (a->note);

	if (a->description)
		obj->summary = g_strdup (a->description);

	
	obj->dtstart = mktime (&a->begin);
	obj->dtend = mktime (&a->end);

	/* Special case: daily repetitions are converted to a multiday event */
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
	 * Now, convert the in memory iCalObject to a full vCalendar
	 * we can send
	 */
	success = cal_client_update_object (client, obj);
	/* set the pilot_status to sync_none so we don't send
	   this event right back to the pilot */
	cal_client_update_pilot_id (client, obj->uid, obj->pilot_id,
				    ICAL_PILOT_SYNC_NONE);


	dump_attr (attr);
	printf (" but not used.\n");

	/*
	 * Shutdown
	 */
	ical_object_unref (obj);
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
sync_object_to_pilot (CalClient *client, iCalObject *obj, int pilot_fd)
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

	printf ("cal->pilot: pilotid=%d, ", obj->pilot_id);

	if (obj->pilot_id) {
		rec_len = dlp_ReadRecordById (pilot_fd, db, obj->pilot_id,
					      buffer,
					      &idx, &rec_len, &attr, &cat);

		if (rec_len > 0)
			unpack_Appointment (a, buffer, rec_len);
	} else {
		attr = 0;
		cat = 0;
	}
	
	/* a contains the appointment either cleared or with
	   the data from the Pilot */
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
	 * Pilot uses a repeat-daily for a multi-day event, adjust
	 * for that case
	 */
	if ((a->end.tm_mday != a->begin.tm_mday) ||
	    (a->end.tm_mon != a->begin.tm_mon) ||
	    (a->end.tm_year != a->begin.tm_year)) {
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
	a->exception = (struct tm *)malloc (sizeof(struct tm) * a->exceptions);
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

	dump_attr (attr);
	printf ("\n");

	dlp_WriteRecord (pilot_fd, db, attr,
			 obj->pilot_id, 0, buffer, rec_len, &new_id);

	cal_client_update_pilot_id (client, obj->uid, new_id,
				    ICAL_PILOT_SYNC_NONE);

	
	conduit_free_Appointment (a);
	g_free (a);
}

static void
sync_cal_to_pilot (CalClient *client, int pilot_fd)
{
	int c;
	int i;
	GList *uids;
	GList *cur;


	uids = cal_client_get_uids (client, CALOBJ_TYPE_ANY);

	c = g_list_length (uids);


	for (cur=uids, i=0; cur; cur=cur->next, i++) {
		const char *uid = cur->data;
		CalClientGetStatus status;
		iCalObject *ico;

		status = cal_client_get_object (client, uid, &ico);
		if (status == CAL_CLIENT_GET_SUCCESS &&
		    ico->pilot_status == ICAL_PILOT_SYNC_MOD) {
			printf ("uid='%s', pilot_status=%d\n", uid,
				ico->pilot_status);
			sync_object_to_pilot (client, ico, pilot_fd);
		}
		/*
		else {
			warn
		}
		*/
	}
}


static void
sync_pilot_to_cal (CalClient *client, int pilot_fd)
{
	int record;
	unsigned char buffer [65536];

	for (record = 0;; record++) {
		struct Appointment a;
		int rec_len, attr, size;
		recordid_t id;
			
		rec_len = dlp_ReadRecordByIndex (pilot_fd, db,
						 record, buffer,
						 &id, &size, &attr, 0);
		if (rec_len < 0)
			break;
			
		unpack_Appointment (&a, buffer, rec_len);
			
		if (debug_attrs)
			dump_attr (attr);
			
		/* If the object was deleted, remove it from
		   the desktop database */
		if (attr & dlpRecAttrDeleted) {
			delete_record_from_desktop (client, id);
			conduit_free_Appointment (&a);
			dlp_DeleteRecord (pilot_fd, db, 0, id);
			continue;
		}

		if (attr & dlpRecAttrArchived)
			continue;

		if (attr & dlpRecAttrDirty) {
			update_record (client, id, &a, attr);
		} else {
			/* if the dirty flag is clear yet we have
			   no copy of it in the desktop database, then
			   we deleted it from the desktop database, so
			   delete it from the pilot */

			char *uid;
			CalClientGetStatus status;
			status = cal_client_get_uid_by_pilot_id (client,
								 id, &uid);
			if (status == CAL_CLIENT_GET_NOT_FOUND) {
				printf ("deleting %ld from pilot\n", id);
				dlp_DeleteRecord (pilot_fd, db, 0, id);
			}
			else
				g_free (uid);
		}

		attr &= ~dlpRecAttrDirty;

		conduit_free_Appointment (&a);
	}
}


static void
sync_pilot (CalClient *client, int pilot_fd)
{
	struct PilotUser user_info;
	struct SysInfo sys_info;
	unsigned char buffer [300];
	

	/* Get the pilot's system information.  FIX ME check return */
	dlp_ReadSysInfo (pilot_fd, &sys_info);

	/* Ask the pilot who it is.  FIX ME check return */
	dlp_ReadUserInfo (pilot_fd, &user_info);


	printf ("---------sys info--------------\n");
	printf ("romVersion=%ld\n", sys_info.romVersion);
	printf ("locale=%ld\n", sys_info.locale);
	strncpy (buffer, sys_info.name, sys_info.nameLength);
	printf ("name='%s'\n", buffer);
	printf ("---------user info--------------\n");
	printf ("userID=%ld\n", user_info.userID);
	printf ("viewerID=%ld\n", user_info.viewerID);
	printf ("lastSyncPC=%ld\n", user_info.lastSyncPC);
	printf ("successfulSyncDate=%s",
		ctime (& user_info.successfulSyncDate));
	printf ("lastSyncDate=%s",
		ctime (& user_info.lastSyncDate));
	printf ("username='%s'\n", user_info.username);
	strncpy (buffer, user_info.password, user_info.passwordLength);
	printf ("password='%s'\n", buffer);
	printf ("--------------------------------\n");


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
	if (! only_desktop_to_pilot)
		sync_pilot_to_cal (client, pilot_fd);


	/*
	 * 2. Push all changes on the desktop to the pilot.
	 *
	 */

	if (! only_pilot_to_desktop)
		sync_cal_to_pilot (client, pilot_fd);	

	/*
	 * 3. Clear the dirty bits on all the pilot's events.
	 *
	 */

	dlp_ResetSyncFlags (pilot_fd, db);

	/*
	 * 4. Close down.
	 *
	 */
	
	dlp_CloseDB (pilot_fd, db);
	dlp_AddSyncLogEntry (pilot_fd,
			     _("Synced DateBook from Pilot to GnomeCal"));
	pi_close (pilot_fd);

	/*
	 * 5. Dump Core.
	 *
	 */
}


static void
gnome_calendar_load_cb (GtkWidget *cal_client,
			CalClientLoadStatus status,
			int *link)
{
	CalClient *client = CAL_CLIENT (cal_client);
	static int tried = 0;

	if (status == CAL_CLIENT_LOAD_SUCCESS) {
		printf ("<Syncing>\n");
		sync_pilot (client, *link);
		printf ("</Syncing>\n");
	} else {
		if (tried) {
			printf ("load and create of calendar failed\n");
			return;
		}

		cal_client_create_calendar (client, calendar_file);
		tried = 1;
	}

	gtk_main_quit ();
}


int
main (int argc, char *argv [])
{
	int link;
	CalClient *client;

	init_bonobo (&argc, argv);

	g_log_set_always_fatal (G_LOG_LEVEL_ERROR |
				G_LOG_LEVEL_CRITICAL |
				G_LOG_LEVEL_WARNING);


	/* FIX ME */
	calendar_file = g_concat_dir_and_file (g_get_home_dir (),
			       "evolution/local/Calendar/calendar.vcf");


	for (;;) {
		printf ("Please, press HotSync button on the palm...\n");
		fflush (stdout);
		link = setup_connection ();
		printf ("Connected\n");

		printf ("Contacting calendar server...\n");
		fflush (stdout);
		client = cal_client_new ();

		gtk_signal_connect (GTK_OBJECT (client), "cal_loaded",
				    gnome_calendar_load_cb, &link);

		cal_client_load_calendar (client, calendar_file);

		bonobo_main ();

		gtk_object_unref (GTK_OBJECT (client));
		pi_close (link);
	}

	return 0;
}

/* Just a stub to link with */

void calendar_notify (time_t time, CalendarAlarm *which, void *data);

void
calendar_notify (time_t time, CalendarAlarm *which, void *data)
{
}

