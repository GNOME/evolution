/* $Id$ */

#include <glib.h>
#include <gnome.h>

#include <pi-source.h>
#include <pi-socket.h>
#include <pi-file.h>
#include <pi-dlp.h>
#include <pi-version.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <utime.h>
#include <unistd.h>
#include <pwd.h>
#include <signal.h>
#include <errno.h>

#include <libgnorba/gnorba.h>
#include <libgnorba/gnome-factory.h>

#include <gpilotd/gnome-pilot-conduit.h>
#include <gpilotd/gnome-pilot-conduit-standard-abs.h>

#include "GnomeCal.h"
#include "calobj.h"
#include "calendar.h"
#include "timeutil.h"

#include "calendar-conduit.h"

GnomePilotConduit * conduit_get_gpilot_conduit (guint32);
void conduit_destroy_gpilot_conduit (GnomePilotConduit*);

typedef struct _ConduitData ConduitData;

struct _ConduitData {
  struct AppointmentAppInfo ai;
  Calendar *cal;
};

#define GET_DATA(c) ((ConduitData*)gtk_object_get_data(GTK_OBJECT(c),"conduit_data"))

GNOME_Calendar_Repository calendar;
CORBA_Environment ev;
CORBA_ORB orb;

static GNOME_Calendar_Repository
calendar_server (void)
{
  calendar = goad_server_activate_with_id (NULL, "IDL:GNOME:Calendar:Repository:1.0",
					   0, NULL);
  if (calendar == CORBA_OBJECT_NIL)
    g_error ("Can not communicate with GnomeCalendar server");
  
  if (ev._major != CORBA_NO_EXCEPTION){
    printf ("Exception: %s\n", CORBA_exception_id (&ev));
    return CORBA_OBJECT_NIL;
  }
  
  return calendar;
}


/* Just a stub to link with */
void
calendar_notify (time_t time, CalendarAlarm *which, void *data)
{
}

static void
local_from_ical(CalLocalRecord **local,iCalObject *obj) {
  g_return_if_fail(local!=NULL);
  g_return_if_fail(*local!=NULL);
  g_return_if_fail(obj!=NULL);

  (*local)->ical = obj;
  (*local)->ID = (*local)->ical->pilot_id;
  
  g_message("(*local)->Id = %ld",(*local)->ID);
  switch((*local)->ical->pilot_status) {
  case ICAL_PILOT_SYNC_NONE: (*local)->local.attr = RecordNothing; break;
  case ICAL_PILOT_SYNC_MOD: (*local)->local.attr = RecordNew; break;
  case ICAL_PILOT_SYNC_DEL: (*local)->local.attr = RecordDeleted; break;
  }
  
  (*local)->local.secret = 0;
  if(obj->class!=NULL) 
    if(strcmp(obj->class,"PRIVATE")==0)
      (*local)->local.secret = 1;
 
  (*local)->local.archived = 0;  
  
  /* used by iterations */
  (*local)->list_ptr = NULL;
}

static CalLocalRecord *
match_record_from_repository(PilotRecord *remote) {
  char *vcal_string;
  CalLocalRecord *loc;
  
  g_return_val_if_fail(remote!=NULL,NULL);
  
  printf ("requesting %ld []\n", remote->ID);
 
  /* FIXME: ehm, who frees this string ? */
  vcal_string = 
    GNOME_Calendar_Repository_get_object_by_pilot_id (calendar, remote->ID, &ev);
  
  if (ev._major == CORBA_USER_EXCEPTION){
    printf (_("\tObject did not exist\n"));
    return NULL;
  } else if(ev._major != CORBA_NO_EXCEPTION) {
    printf(_("\tError while communicating with calendar server\n"));
    CORBA_exception_free(&ev); 
    return NULL;
  } else {
    printf ("\tFound\n");
    loc = g_new0(CalLocalRecord,1);
    /* memory allocated in new_from_string is freed in free_match */
    local_from_ical(&loc,ical_object_new_from_string (vcal_string));
    return loc;
  }

  return NULL;
}

/* Code blatantly stolen from
 * calendar-pilot-sync.c:
 *   
 * (C) 1999 International GNOME Support
 *
 * Author:
 *   Miguel de Icaza (miguel@gnome-support.com)
 *
 */
static void
update_record (PilotRecord *remote) 
{
	char *vcal_string;
	iCalObject *obj;
	int i;
	char *str;
	struct Appointment a;

	g_return_if_fail(remote!=NULL);

	unpack_Appointment(&a,remote->record,remote->length);
	
	obj = ical_new (a.note ? a.note : "",
			g_get_user_name (),
			a.description ? a.description : "");

	printf ("requesting %ld [%s]\n", remote->ID, a.description);
	/* FIXME: ehm, who frees this string ? */
	vcal_string = GNOME_Calendar_Repository_get_object_by_pilot_id (calendar, remote->ID, &ev);

	if (ev._major == CORBA_USER_EXCEPTION){
		time_t now = time (NULL);
		
		obj->created = now;
		obj->last_mod = now;
		obj->priority = 0;
		obj->transp = 0;
		obj->related = NULL;
		obj->pilot_id = remote->ID;
		obj->pilot_status = ICAL_PILOT_SYNC_NONE;
		printf (_("\tObject did not exist, creating a new one\n"));
	} else if(ev._major != CORBA_NO_EXCEPTION) {
	  printf(_("\tError while communicating with calendar server\n"));
	  printf("\texception id = %s\n",CORBA_exception_id(&ev));
	  CORBA_exception_free(&ev); 
	  ical_object_destroy (obj); 
	  return;
	} else {
		printf ("\tFound\n");
		ical_object_destroy (obj);
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

	if (a.event)
	{
		/* turn day-long events into a full day's appointment */
		a.begin.tm_sec = 0;
		a.begin.tm_min = 0;
		a.begin.tm_hour = 6;

		a.end.tm_sec = 0;
		a.end.tm_min = 0;
		a.end.tm_hour = 10;
	}
	
	obj->dtstart = mktime (&a.begin);
	obj->dtend = mktime (&a.end);

	/* Special case: daily repetitions are converted to a multi-day event */
	if (a.repeatType == repeatDaily){
		time_t newt = time_add_day (obj->dtend, a.repeatFrequency);

		obj->dtend = newt;
	}

	/*
	 * Alarm
	 */
	if (a.alarm){
		obj->aalarm.type = ALARM_AUDIO;
		obj->aalarm.enabled = 1;
		obj->aalarm.count = a.advance;

		switch (a.advanceUnits){
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
	if (a.repeatFrequency && a.repeatType != repeatDaily){
		obj->recur = g_new0 (Recurrence, 1);
		
		switch (a.repeatType){
		case repeatDaily:
			/*
			 * In the Pilot daily repetitions are actually
			 * multi-day events
			 */
			g_warning ("Should not have got here");
			break;
			
		case repeatMonthlyByDate:
			obj->recur->type = RECUR_MONTHLY_BY_DAY;
			obj->recur->u.month_day = a.repeatFrequency;
			break;
			
		case repeatWeekly:
		{
			int wd;

			obj->recur->type = RECUR_WEEKLY;
			for (wd = 0; wd < 7; wd++)
				if (a.repeatDays [wd])
					obj->recur->weekday |= 1 << wd;

			if (obj->recur->weekday == 0){
				struct tm *tm = localtime (&obj->dtstart);

				obj->recur->weekday = 1 << tm->tm_wday;
			}
			break;
		}
		
		case repeatMonthlyByDay:
			obj->recur->type = RECUR_MONTHLY_BY_POS;
			obj->recur->u.month_pos = a.repeatFrequency;
			obj->recur->weekday = (a.repeatDay / 7);
			break;
			
		case repeatYearly:
			obj->recur->type = RECUR_YEARLY_BY_DAY;
			break;

		default:
			g_warning ("Unhandled repeate case");
		}

		if (a.repeatForever)
			obj->recur->duration = 0;
		else
			obj->recur->_enddate = mktime (&a.repeatEnd);
	}

	/*
	 * Load exception dates 
	 */
	obj->exdate = NULL;
	for (i = 0; i < a.exceptions; i++){
		time_t *t = g_new (time_t, 1);

		*t = mktime (&(a.exception [i]));
		obj->exdate = g_list_prepend (obj->exdate, t);
	}

	g_free (obj->class);
	
	if (remote->attr & dlpRecAttrSecret)
		obj->class = g_strdup ("PRIVATE");
	else
		obj->class = g_strdup ("PUBLIC");

	/*
	 * Now, convert the in memory iCalObject to a full vCalendar we can send
	 */
	str = calendar_string_from_object (obj);

	GNOME_Calendar_Repository_update_object (calendar, obj->uid, str, &ev);

	free (str);
	
	/*
	 * Shutdown
	 */
	ical_object_destroy (obj);
}

static gint
load_records(GnomePilotConduit *c)
{
  char *vcalendar_string;
  char *error;
  ConduitData *cd;

  vcalendar_string = 
    GNOME_Calendar_Repository_get_objects (calendar, &ev);

  cd = GET_DATA(c);
  g_assert(cd!=NULL);
  cd->cal = calendar_new("Temporary");

  error = calendar_load_from_memory(cd->cal,vcalendar_string);

  return 0;
}

static gint
pre_sync(GnomePilotConduit *c, GnomePilotDBInfo *dbi) 
{
  int l;
  unsigned char *buf;
  
  calendar = CORBA_OBJECT_NIL;
  calendar = calendar_server();
  if(calendar == CORBA_OBJECT_NIL) {
    return 0;
  }
  
  gtk_object_set_data(GTK_OBJECT(c),"dbinfo",dbi);
  
  load_records(c);

  buf = (unsigned char*)g_malloc(0xffff);
  if((l=dlp_ReadAppBlock(dbi->pilot_socket,dbi->db_handle,0,(unsigned char *)buf,0xffff))<0) {
    return 0;
  }
  unpack_AppointmentAppInfo(&(GET_DATA(c)->ai),buf,l);
  g_free(buf);

  return 1;
}

/**
 * Find (if possible) the local record which matches
 * the given PilotRecord.
 * if successfull, return non-zero and set *local to
 * a non-null value (the located local record),
 * otherwise return 0 and set *local = NULL;
 */

static gint
match_record	(GnomePilotConduitStandardAbs *conduit,
		 CalLocalRecord **local,
		 PilotRecord *remote,
		 gpointer data)
{
  g_return_val_if_fail(remote!=NULL,0);
  g_print ("in match_record\n");

  *local = match_record_from_repository(remote);
  
  return 1;
}

/**
 * Free the data allocated by a previous match_record call.
 * If successfull, return non-zero and ser *local=NULL, otherwise
 * return 0.
 */
static gint
free_match	(GnomePilotConduitStandardAbs *conduit,
		 CalLocalRecord **local,
		 gpointer data)
{
        g_print ("entering free_match\n");
	ical_object_destroy (CALLOCALRECORD(*local)->ical); 
	g_free(*local);
	
        *local = NULL;
	return 1;
}


static gint
archive_local (GnomePilotConduitStandardAbs *conduit,
	       CalLocalRecord *local,
	       gpointer data)
{
	g_print ("entering archive_local\n");
	return 1;

}

static gint
archive_remote (GnomePilotConduitStandardAbs *conduit,
		CalLocalRecord *local,
		PilotRecord *remote,
		gpointer data)
{
	g_print ("entering archive_remote\n");
	return 1;
}

static gint
store_remote (GnomePilotConduitStandardAbs *conduit,
	      PilotRecord *remote,
	      gpointer data)
{
  g_print ("entering store_remote\n");
  g_return_val_if_fail(remote!=NULL,0);

  update_record(remote);
  
  return 1;
}

static gint
clear_status_archive_local (GnomePilotConduitStandardAbs *conduit,
			    CalLocalRecord *local,
			    gpointer data)
{
	g_print ("entering clear_status_archive_local\n");
        return 1;
}

static gint
iterate (GnomePilotConduitStandardAbs *conduit,
	 CalLocalRecord **local,
	 gpointer data)
{
  g_return_val_if_fail(local!=NULL,0);
  
  if(*local==NULL) {
    g_message("calconduit: beginning iteration");
    if(GET_DATA(conduit)->cal->events!=NULL) {
      *local = g_new0(CalLocalRecord,1);

      local_from_ical(local,(iCalObject*)GET_DATA(conduit)->cal->events->data);
      (*local)->list_ptr = GET_DATA(conduit)->cal->events;
    } else {
      g_message("calconduit: no events");
      (*local) = NULL;
    }
  } else {
    g_message("calconduit: continuing iteration");
    if(g_list_next((*local)->list_ptr)==NULL) {
      g_message("calconduit: ending");
      g_free((*local));
      (*local) = NULL; /* ends iteration */
    } else {
      local_from_ical(local,(iCalObject*)(g_list_next((*local)->list_ptr)->data));
      (*local)->list_ptr = g_list_next((*local)->list_ptr);
    }
  }
  return 1;
}

static gint
iterate_specific (GnomePilotConduitStandardAbs *conduit,
		  CalLocalRecord **local,
		  gint flag,
		  gint archived,
		  gpointer data)
{
  g_return_val_if_fail(local!=NULL,0);

  g_print ("entering iterate_specific\n");
  do {
    gnome_pilot_conduit_standard_abs_iterate(conduit,(LocalRecord**)local);
    if((*local)==NULL) break;
    if(archived && ((*local)->local.archived==archived)) break;
    if((*local)->local.attr == flag) break;
  } while((*local)!=NULL);

  return 1;
}

static gint
purge (GnomePilotConduitStandardAbs *conduit,
       gpointer data)
{
	g_print ("entering purge\n");
        return 1;
}

static gint
set_status (GnomePilotConduitStandardAbs *conduit,
	    CalLocalRecord *local,
	    gint status,
	    gpointer data)
{
	g_print ("entering set_status\n");
        return 0;
}

static gint
set_archived (GnomePilotConduitStandardAbs *conduit,
	      CalLocalRecord *local,
	      gint archived,
	      gpointer data)
{
	g_print ("entering set_archived\n");
        return 1;
}

static gint
set_pilot_id (GnomePilotConduitStandardAbs *conduit,
	      CalLocalRecord *local,
	      guint32 ID,
	      gpointer data)
{
	g_print ("entering set_pilot_id\n");
        return 1;
}

static gint
compare (GnomePilotConduitStandardAbs *conduit,
	 CalLocalRecord *local,
	 PilotRecord *remote,
	 gpointer data)
{
	g_print ("entering compare\n");
        return 1;
}

static gint
compare_backup (GnomePilotConduitStandardAbs *conduit,
		CalLocalRecord *local,
		PilotRecord *remote,
		gpointer data)
{
	g_print ("entering compare_backup\n");
        return 1;
}

static gint
free_transmit (GnomePilotConduitStandardAbs *conduit,
	       CalLocalRecord *local,
	       PilotRecord *remote,
	       gpointer data)
{
	g_print ("entering free_transmit\n");
        return 1;
}

static gint
delete_all (GnomePilotConduitStandardAbs *conduit,
	    gpointer data)
{
	g_print ("entering delete_all\n");
        return 1;
}

static PilotRecord *
transmit (GnomePilotConduitStandardAbs *conduit,
	  CalLocalRecord *local,
	  gpointer data)
{
	g_print ("entering transmit\n");
	return NULL;
}

GnomePilotConduit *
conduit_get_gpilot_conduit (guint32 pilotId)
{
	GtkObject *retval;
	ConduitCfg *cfg;
	ConduitData *cdata;

	CORBA_exception_init (&ev);

	retval = gnome_pilot_conduit_standard_abs_new ("DatebookDB", 0x64617465);
	g_assert (retval != NULL);
	gnome_pilot_conduit_construct(GNOME_PILOT_CONDUIT(retval),"calendar");

	cfg = g_new0(ConduitCfg,1);
	g_assert(cfg != NULL);
	gtk_object_set_data(retval,"conduit_cfg",cfg);

	cdata = g_new0(ConduitData,1);
	g_assert(cdata != NULL);
	gtk_object_set_data(retval,"conduit_data",cdata);

	gtk_signal_connect (retval, "match_record", (GtkSignalFunc) match_record, NULL);
	gtk_signal_connect (retval, "free_match", (GtkSignalFunc) free_match, NULL);
	gtk_signal_connect (retval, "archive_local", (GtkSignalFunc) archive_local, NULL);
	gtk_signal_connect (retval, "archive_remote", (GtkSignalFunc) archive_remote, NULL);
	gtk_signal_connect (retval, "store_remote", (GtkSignalFunc) store_remote, NULL);
	gtk_signal_connect (retval, "clear_status_archive_local", (GtkSignalFunc) clear_status_archive_local, NULL);
	gtk_signal_connect (retval, "iterate", (GtkSignalFunc) iterate, NULL);
	gtk_signal_connect (retval, "iterate_specific", (GtkSignalFunc) iterate_specific, NULL);
	gtk_signal_connect (retval, "purge", (GtkSignalFunc) purge, NULL);
	gtk_signal_connect (retval, "set_status", (GtkSignalFunc) set_status, NULL);
	gtk_signal_connect (retval, "set_archived", (GtkSignalFunc) set_archived, NULL);
	gtk_signal_connect (retval, "set_pilot_id", (GtkSignalFunc) set_pilot_id, NULL);
	gtk_signal_connect (retval, "compare", (GtkSignalFunc) compare, NULL);
	gtk_signal_connect (retval, "compare_backup", (GtkSignalFunc) compare_backup, NULL);
	gtk_signal_connect (retval, "free_transmit", (GtkSignalFunc) free_transmit, NULL);
	gtk_signal_connect (retval, "delete_all", (GtkSignalFunc) delete_all, NULL);
	gtk_signal_connect (retval, "transmit", (GtkSignalFunc) transmit, NULL);
	gtk_signal_connect (retval, "pre_sync", (GtkSignalFunc) pre_sync, NULL);

	load_configuration(&cfg,pilotId);

	return GNOME_PILOT_CONDUIT (retval);
}

void
conduit_destroy_gpilot_conduit (GnomePilotConduit *conduit)
{ 
        ConduitCfg *cc;
	ConduitData *cd;

	GNOME_Calendar_Repository_done (calendar, &ev);

        cc = GET_CONFIG(conduit);
        destroy_configuration(&cc);

	cd = GET_DATA(conduit);
	if(cd->cal!=NULL) calendar_destroy(cd->cal);

	gtk_object_destroy (GTK_OBJECT (conduit));

}


