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
void local_record_from_icalobject(CalLocalRecord *local,iCalObject *obj); 

typedef struct _ConduitData ConduitData;

struct _ConduitData {
	struct AppointmentAppInfo ai;
};

#define CONDUIT_VERSION "0.8.0"
#define DEBUG_CALCONDUIT

#ifdef DEBUG_CALCONDUIT
#define show_exception(e) g_warning ("Exception: %s\n", CORBA_exception_id (e))
#define LOG(e...) g_log(G_LOG_DOMAIN,G_LOG_LEVEL_MESSAGE, "calconduit: "##e)
#else
#define show_exception(e)
#define LOG(e...)
#endif 

#define WARN(e...) g_log(G_LOG_DOMAIN,G_LOG_LEVEL_WARNING, e)
#define INFO(e...) g_log(G_LOG_DOMAIN,G_LOG_LEVEL_MESSAGE, "calconduit: "##e)

#define GET_DATA(c) ((ConduitData*)gtk_object_get_data(GTK_OBJECT(c),"conduit_data"))

GNOME_Calendar_Repository calendar;
CORBA_Environment ev;
CORBA_ORB orb;

static GNOME_Calendar_Repository
start_calendar_server (GnomePilotConduitStandardAbs *conduit) 
{
	g_return_val_if_fail(conduit!=NULL,CORBA_OBJECT_NIL);

	calendar = goad_server_activate_with_id (NULL, 
						 "IDL:GNOME:Calendar:Repository:1.0",
						 0, NULL);
	if (calendar == CORBA_OBJECT_NIL)
		g_error ("Can not communicate with GnomeCalendar server");
  
	if (ev._major != CORBA_NO_EXCEPTION){
		show_exception(&ev);
		CORBA_exception_free(&ev);
		return CORBA_OBJECT_NIL;
	}
  
	return calendar;
}


/* Just a stub to link with */
void
calendar_notify (time_t time, CalendarAlarm *which, void *data)
{
}

static GList * 
get_calendar_objects(GnomePilotConduitStandardAbs *conduit) 
{
	GList *result;
	GNOME_Calendar_Repository_String_Sequence *uids;

	g_return_val_if_fail(conduit!=NULL,NULL);

	result = NULL;
	uids = GNOME_Calendar_Repository_get_object_id_list (calendar, &ev);

  	if (ev._major == CORBA_USER_EXCEPTION){
		INFO ("Object did not exist");
		show_exception(&ev);
		CORBA_exception_free(&ev); 
		return NULL;
	} else if(ev._major != CORBA_NO_EXCEPTION) {
		WARN (_("Error while communicating with calendar server"));
		show_exception(&ev);
		CORBA_exception_free(&ev); 
		return NULL;
	} 

	if(uids->_length>0) {
		int i;
		for(i=0;i<uids->_length;i++) {
			result = g_list_prepend(result,g_strdup(uids->_buffer[i]));
		}
	} else
		INFO ("No entries found");
	
	CORBA_free(uids);

	return result;
}

#if 0
static GList * 
get_calendar_objects(GnomePilotConduitStandardAbs *conduit) 
{
	char *vcalendar_string;
	char *error;
	GList *retval,*l;
	Calendar *cal;

	g_return_val_if_fail(conduit!=NULL,NULL);

	vcalendar_string = 
		GNOME_Calendar_Repository_get_objects (calendar, &ev);

	cal = calendar_new("Temporary");

	error = calendar_load_from_memory(cal,vcalendar_string);

  	if (ev._major == CORBA_USER_EXCEPTION){
		INFO ("Object did not exist");
		show_exception(&ev);
		CORBA_exception_free(&ev); 
		return;
	} else if(ev._major != CORBA_NO_EXCEPTION) {
		WARN (_("Error while communicating with calendar server"));
		show_exception(&ev);
		CORBA_exception_free(&ev); 
		return;
	} 

	if(error != NULL) {
		WARN ("Error while converting records");
		WARN ("Error : %s",error);
		return NULL;
	}
	retval = NULL;
	for(l=cal->events ; l ; l=l->next) {
		LOG ("duping %d [%s]",
			((iCalObject*)l->data)->pilot_id,
			((iCalObject*)l->data)->summary);
		retval = g_list_prepend(retval,ical_object_duplicate(l->data));
	}

	/* g_free(vcalendar_string); FIXME: this coredumps, but won't it leak without ? */
	calendar_destroy(cal);

	return retval;  
}
#endif

static void 
local_record_from_ical_uid(CalLocalRecord *local,
			   char *uid)
{
	iCalObject *obj;
	char *vcalendar_string;

	g_assert(local!=NULL);
	
	vcalendar_string = GNOME_Calendar_Repository_get_object(calendar, uid, &ev);

  	if (ev._major == CORBA_USER_EXCEPTION){
		INFO ("Object did not exist");
		show_exception(&ev);
		CORBA_exception_free(&ev); 
		return;
	} else if(ev._major != CORBA_NO_EXCEPTION) {
		WARN (_("Error while communicating with calendar server"));
		show_exception(&ev);
		CORBA_exception_free(&ev); 
		return;
	} 
	g_return_if_fail(vcalendar_string!=NULL);
	
	obj = ical_object_new_from_string (vcalendar_string);

	local_record_from_icalobject(local,obj);

	return;
}


/*
 * converts a iCalObject to a CalLocalRecord
 */

void
local_record_from_icalobject(CalLocalRecord *local,
			     iCalObject *obj) 
{
	g_return_if_fail(local!=NULL);
	g_return_if_fail(obj!=NULL);

	local->ical = obj;
	local->local.ID = local->ical->pilot_id;
  
	LOG ("local->Id = %ld [%s], status = %d",
		  local->local.ID,obj->summary,local->ical->pilot_status);

	switch(local->ical->pilot_status) {
	case ICAL_PILOT_SYNC_NONE: 
		local->local.attr = GnomePilotRecordNothing; 
		break;
	case ICAL_PILOT_SYNC_MOD: 
		local->local.attr = GnomePilotRecordModified; 
		break;
	case ICAL_PILOT_SYNC_DEL: 
		local->local.attr = GnomePilotRecordDeleted; 
		break;
	}

	/* Records without a pilot_id are new */
	if(local->local.ID == 0) 
		local->local.attr = GnomePilotRecordNew; 
  
	local->local.secret = 0;
	if(obj->class!=NULL) 
		if(strcmp(obj->class,"PRIVATE")==0)
			local->local.secret = 1;
 
	local->local.archived = 0;  
}

/*
 * Given a PilotRecord, find the matching record in
 * the calendar repository. If no match, return NULL
 */
static CalLocalRecord *
find_record_in_repository(GnomePilotConduitStandardAbs *conduit,
			  PilotRecord *remote) 
{
	char *vcal_string;
	CalLocalRecord *loc;
  
	g_return_val_if_fail(conduit!=NULL,NULL);
	g_return_val_if_fail(remote!=NULL,NULL);
  
 
	vcal_string = 
		GNOME_Calendar_Repository_get_object_by_pilot_id (calendar, remote->ID, &ev);
  
	if (ev._major == CORBA_USER_EXCEPTION){
		INFO ("Object did not exist");
		show_exception(&ev);
		CORBA_exception_free(&ev); 
		return NULL;
	} else if(ev._major != CORBA_NO_EXCEPTION) {
		WARN (_("Error while communicating with calendar server"));
		show_exception(&ev);
		CORBA_exception_free(&ev); 
		return NULL;
	} else {
		LOG ("Found");
		loc = g_new0(CalLocalRecord,1);
		/* memory allocated in new_from_string is freed in free_match */
		local_record_from_icalobject(loc,
					     ical_object_new_from_string (vcal_string));
		/* g_free(vcal_string); FIXME: this coredumps, but won't it leak without ? */
		return loc;
	}

	return NULL;
}

/* 
 * updates an given iCalObject in the repository
 */
static void
update_calendar_entry_in_repository(GnomePilotConduitStandardAbs *conduit,
				    iCalObject *obj) 
{
	char *str;

	g_return_if_fail(conduit!=NULL);
	g_return_if_fail(obj!=NULL);

	str = calendar_string_from_object (obj);
  
	GNOME_Calendar_Repository_update_object (calendar, obj->uid, str, &ev);

  	if (ev._major == CORBA_USER_EXCEPTION){
		INFO ("Object did not exist");
		show_exception(&ev);
		CORBA_exception_free(&ev); 
		return;
	} else if(ev._major != CORBA_NO_EXCEPTION) {
		WARN (_("Error while communicating with calendar server"));
		show_exception(&ev);
		CORBA_exception_free(&ev); 
		return;
	} 

	free (str);
}

static iCalObject *
ical_from_remote_record(GnomePilotConduitStandardAbs *conduit,
			PilotRecord *remote,
			iCalObject *in_obj)
{
	iCalObject *obj;
	int i;
	struct Appointment a;
	time_t now;

	now = time (NULL);

	g_return_val_if_fail(remote!=NULL,NULL);
	memset(&a,0,sizeof(struct Appointment));
	unpack_Appointment(&a,remote->record,remote->length);
	
	if (in_obj == NULL)
		obj = ical_new (a.note ? a.note : "",
				g_get_user_name (),
				a.description ? a.description : "");
	else 
		obj = in_obj;
	
	if (a.note) {
		g_free(obj->comment);
		obj->comment = g_strdup(a.note);
	}
	if (a.description) {
		g_free(obj->summary);
		obj->summary = g_strdup(a.description);
	}
	
	obj->created = now;
	obj->last_mod = now;
	obj->priority = 0;
	obj->transp = 0;
	obj->related = NULL;
	obj->pilot_id = remote->ID;
	obj->pilot_status = ICAL_PILOT_SYNC_NONE;

	/*
	 * Begin and end
	 */

	if (a.event)
	{
		/* turn day-long events into a full day's appointment
		   FIXME: get settings from gnomecal */
		a.begin.tm_sec = 0;
		a.begin.tm_min = 0;
		a.begin.tm_hour = 0;

		a.end.tm_sec = 0;
		a.end.tm_min =59;
		a.end.tm_hour = 23;
	}
	
	obj->dtstart = mktime (&a.begin);
	obj->dtend = mktime (&a.end);

	/* Special case: daily repetitions are converted to a multi-day event */
	/* This sucketh, a pilot event scheduled for dailyRepeat, freq 1, end on 
	   whatever is cleary converted wrong 
	if (a.repeatType == repeatDaily){
		time_t newt = time_add_day (obj->dtend, a.repeatFrequency);

		obj->dtend = newt;
	}
	*/

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
	if (a.repeatFrequency){
		obj->recur = g_new0 (Recurrence, 1);
		
		switch (a.repeatType){
		case repeatDaily:
			/*
			 * In the Pilot daily repetitions are actually
			 * multi-day events
			 */
			obj->recur->type = RECUR_DAILY;
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
				struct tm tm = *localtime (&obj->dtstart);

				obj->recur->weekday = 1 << tm.tm_wday;
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
			g_assert_not_reached();
		}

		if (a.repeatForever)
			obj->recur->duration = 0;
		else
			obj->recur->_enddate = mktime (&a.repeatEnd);

		obj->recur->interval = a.repeatFrequency;
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


	free_Appointment(&a);

	return obj;
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
static gint
update_record (GnomePilotConduitStandardAbs *conduit,
	       PilotRecord *remote)
{
	char *vcal_string;
	iCalObject *obj;
	struct Appointment a;

	g_return_val_if_fail(remote!=NULL,-1);

	memset(&a,0,sizeof(struct Appointment));
	unpack_Appointment(&a,remote->record,remote->length);
	
	obj = ical_new (a.note ? a.note : "",
			g_get_user_name (),
			a.description ? a.description : "");

	LOG ("requesting %ld [%s]", remote->ID, a.description);
	vcal_string = GNOME_Calendar_Repository_get_object_by_pilot_id (calendar, remote->ID, &ev);

	if (ev._major == CORBA_USER_EXCEPTION){
		WARN (_("Object did not exist, creating a new one"));
		show_exception(&ev);
		CORBA_exception_free(&ev); 
		ical_from_remote_record(conduit,remote,obj);
	} else if(ev._major != CORBA_NO_EXCEPTION) {
	        WARN (_("Error while communicating with calendar server"));
		show_exception(&ev);
		CORBA_exception_free(&ev); 
		ical_object_destroy (obj); 
		free_Appointment(&a);
		return -1;
	} else {
	        LOG ("Found");
		ical_object_destroy (obj);
		obj = ical_object_new_from_string (vcal_string);
		ical_from_remote_record(conduit,remote,obj);
		CORBA_free(vcal_string);
	}

	/* update record on server */
	
	update_calendar_entry_in_repository(conduit,obj);

	/*
	 * Shutdown
	 */
	ical_object_destroy (obj);
	free_Appointment(&a);

	return 0;
}


static gint
pre_sync(GnomePilotConduit *c, GnomePilotDBInfo *dbi) 
{
	int l;
	unsigned char *buf;
  
	INFO (CONDUIT_VERSION);

	calendar = CORBA_OBJECT_NIL;
	calendar = start_calendar_server(GNOME_PILOT_CONDUIT_STANDARD_ABS(c));
	if(calendar == CORBA_OBJECT_NIL) {
		return -1;
	}
  
	gtk_object_set_data(GTK_OBJECT(c),"dbinfo",dbi);
  
	/* load_records(c); */

	buf = (unsigned char*)g_malloc(0xffff);
	if((l=dlp_ReadAppBlock(dbi->pilot_socket,dbi->db_handle,0,(unsigned char *)buf,0xffff))<0) {
		return -1;
	}
	unpack_AppointmentAppInfo(&(GET_DATA(c)->ai),buf,l);
	g_free(buf);

	return 0;
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
	LOG ("in match_record");

	g_return_val_if_fail(local!=NULL,-1);
	g_return_val_if_fail(remote!=NULL,-1);

	*local = find_record_in_repository(conduit,remote);
  
	if (*local==NULL) return -1;
	return 0;
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
	LOG ("entering free_match");

	g_return_val_if_fail(local!=NULL,-1);
	g_return_val_if_fail(*local!=NULL,-1);

	ical_object_destroy (CALLOCALRECORD(*local)->ical); 
	g_free(*local);
	
        *local = NULL;
	return 0;
}

/*
  Move to archive and set status to Nothing
 */
static gint
archive_local (GnomePilotConduitStandardAbs *conduit,
	       CalLocalRecord *local,
	       gpointer data)
{
	LOG ("entering archive_local");

	g_return_val_if_fail(local!=NULL,-1);

	return -1;
}

/*
  Store in archive and set status to Nothing
 */
static gint
archive_remote (GnomePilotConduitStandardAbs *conduit,
		CalLocalRecord *local,
		PilotRecord *remote,
		gpointer data)
{
	LOG ("entering archive_remote");

        g_return_val_if_fail(remote!=NULL,-1);
	g_return_val_if_fail(local!=NULL,-1);

	return -1;
}

/*
  Store and set status to Nothing
 */
static gint
store_remote (GnomePilotConduitStandardAbs *conduit,
	      PilotRecord *remote,
	      gpointer data)
{
	LOG ("entering store_remote");

	g_return_val_if_fail(remote!=NULL,-1);

	return update_record(conduit,remote);
}

static gint
clear_status_archive_local (GnomePilotConduitStandardAbs *conduit,
			    CalLocalRecord *local,
			    gpointer data)
{
	LOG ("entering clear_status_archive_local");

	g_return_val_if_fail(local!=NULL,-1);

        return -1;
}

static gint
iterate (GnomePilotConduitStandardAbs *conduit,
	 CalLocalRecord **local,
	 gpointer data)
{
	static GList *events,*iterator;
	static int hest;

	g_return_val_if_fail(local!=NULL,-1);

	if(*local==NULL) {
		LOG ("beginning iteration");

		events = get_calendar_objects(conduit);
		hest = 0;
		
		if(events!=NULL) {
			LOG ("iterating over %d records",g_list_length(events));
			*local = g_new0(CalLocalRecord,1);

			local_record_from_ical_uid(*local,(gchar*)events->data);
			iterator = events;
		} else {
			LOG ("no events");
			(*local) = NULL;
		}
	} else {
		LOG ("continuing iteration");
		hest++;
		if(g_list_next(iterator)==NULL) {
			GList *l;

			LOG ("ending");
			/** free stuff allocated for iteration */
			g_free((*local));

			LOG ("iterated over %d records",hest);
			for(l=events;l;l=l->next)
				g_free(l->data);

			g_list_free(events);

			/* ends iteration */
			(*local) = NULL;
			return 0;
		} else {
			iterator = g_list_next(iterator);
			local_record_from_ical_uid(*local,(gchar*)(iterator->data));
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
#ifdef DEBUG_CALCONDUIT
	{
		gchar *tmp;
		switch (flag) {
		case GnomePilotRecordNothing: tmp = g_strdup("RecordNothing"); break;
		case GnomePilotRecordModified: tmp = g_strdup("RecordModified"); break;
		case GnomePilotRecordNew: tmp = g_strdup("RecordNew"); break;
		}
		LOG ("entering iterate_specific(flag = %s)",tmp);
		g_free(tmp);
	}
#endif
	g_return_val_if_fail(local!=NULL,-1);

	/* iterate until a record meets the criteria */
	while(gnome_pilot_conduit_standard_abs_iterate(conduit,(LocalRecord**)local)) {
		if((*local)==NULL) break;
		/* g_print("calconduit: local->attr = %d, flag = %d, & %d\n",(*local)->local.attr,flag,((*local)->local.attr &  flag)); */
		if(archived && ((*local)->local.archived==archived)) break;
		if(((*local)->local.attr == flag)) break;
	}

	return (*local)==NULL?0:1;
}

static gint
purge (GnomePilotConduitStandardAbs *conduit,
       gpointer data)
{
	LOG ("entering purge");


	/* HEST, gem posterne her */

	return -1;
}

static gint
set_status (GnomePilotConduitStandardAbs *conduit,
	    CalLocalRecord *local,
	    gint status,
	    gpointer data)
{
	LOG ("entering set_status");

	g_return_val_if_fail(local!=NULL,-1);

	g_assert(local->ical!=NULL);
	
	local->local.attr = status;
	switch(status) {
	case GnomePilotRecordPending:
	case GnomePilotRecordNothing:
		local->ical->pilot_status = ICAL_PILOT_SYNC_NONE;
		break;
	case GnomePilotRecordDeleted:
		break;
	case GnomePilotRecordNew:
	case GnomePilotRecordModified:
		local->ical->pilot_status = ICAL_PILOT_SYNC_MOD;
		break;	  
	}
	
	if ( status != GnomePilotRecordDeleted) 
		GNOME_Calendar_Repository_update_pilot_id(calendar,
							  local->ical->uid,
							  local->local.ID,
							  local->ical->pilot_status,
							  &ev);
	else
		GNOME_Calendar_Repository_delete_object(calendar,local->ical->uid,&ev);
	
	if (ev._major == CORBA_USER_EXCEPTION){
		LOG ("Object did not exist");
		show_exception(&ev);
		CORBA_exception_free(&ev); 
		return -1;
	} else if(ev._major != CORBA_NO_EXCEPTION) {
		WARN (_("Error while communicating with calendar server"));
		show_exception(&ev);
		CORBA_exception_free(&ev); 
		return -1;
	} 
        return 0;
}

static gint
set_archived (GnomePilotConduitStandardAbs *conduit,
	      CalLocalRecord *local,
	      gint archived,
	      gpointer data)
{
	LOG ("entering set_archived");

	g_return_val_if_fail(local!=NULL,-1);
	g_assert(local->ical!=NULL);

	local->local.archived = archived;
	update_calendar_entry_in_repository(conduit,local->ical);
	/* FIXME: This should move the entry into a speciel
	   calendar file, eg. Archive, or (by config option), simply
	   delete it */
        return 0;
}

static gint
set_pilot_id (GnomePilotConduitStandardAbs *conduit,
	      CalLocalRecord *local,
	      guint32 ID,
	      gpointer data)
{
	LOG ("entering set_pilot_id");

	g_return_val_if_fail(local!=NULL,-1);
	g_assert(local->ical!=NULL);

	local->local.ID = ID;
	local->ical->pilot_id = ID;
	GNOME_Calendar_Repository_update_pilot_id(calendar,
						  local->ical->uid,
						  local->local.ID,
						  local->ical->pilot_status,
						  &ev);

	if (ev._major == CORBA_USER_EXCEPTION){
		LOG ("Object did not exist");
		show_exception(&ev);
		CORBA_exception_free(&ev); 
		return -1;
	} else if(ev._major != CORBA_NO_EXCEPTION) {
		WARN (_("Error while communicating with calendar server"));
		show_exception(&ev);
		CORBA_exception_free(&ev); 
		return -1;
	} 
        return 0;
}

static gint
compare (GnomePilotConduitStandardAbs *conduit,
	 CalLocalRecord *local,
	 PilotRecord *remote,
	 gpointer data)
{
	LOG ("entering compare");

	g_return_val_if_fail(local!=NULL,-1);
	g_return_val_if_fail(remote!=NULL,-1);

        return -1;
}

static gint
compare_backup (GnomePilotConduitStandardAbs *conduit,
		CalLocalRecord *local,
		PilotRecord *remote,
		gpointer data)
{
	LOG ("entering compare_backup");

	g_return_val_if_fail(local!=NULL,-1);
	g_return_val_if_fail(remote!=NULL,-1);

        return -1;
}

static gint
free_transmit (GnomePilotConduitStandardAbs *conduit,
	       CalLocalRecord *local,
	       PilotRecord **remote,
	       gpointer data)
{
	LOG ("entering free_transmit");

	g_return_val_if_fail(local!=NULL,-1);
	g_return_val_if_fail(remote!=NULL,-1);

	free_Appointment(local->a);
	g_free((*remote)->record);
	*remote = NULL;
        return 0;
}

static gint
delete_all (GnomePilotConduitStandardAbs *conduit,
	    gpointer data)
{
	LOG ("entering delete_all");

        return -1;
}

static gint
transmit (GnomePilotConduitStandardAbs *conduit,
	  CalLocalRecord *local,
	  PilotRecord **remote,
	  gpointer data)
{
	PilotRecord *p;
	int daycount;
	int x,y;
	
	LOG ("entering transmit");

	g_return_val_if_fail(local!=NULL,-1);
	g_return_val_if_fail(remote!=NULL,-1);
	g_assert(local->ical!=NULL);

	p = g_new0(PilotRecord,1);

	p->ID = local->local.ID;
	p->attr = local->local.attr;
	p->archived = local->local.archived;
	p->secret = local->local.secret;

	local->a = g_new0(struct Appointment,1);

	local->a->event = 0; /* if no start time, leave at 1 */
	local->a->begin = *localtime(&local->ical->dtstart);
	local->a->end = *localtime(&local->ical->dtend);

	/* set the Audio Alarm  parameters */
	if(local->ical->aalarm.enabled) {
		local->a->alarm = 1;
		local->a->advance = local->ical->aalarm.count;
		switch(local->ical->aalarm.units) {
		case ALARM_MINUTES:
			local->a->advanceUnits = advMinutes;
			break;
		case ALARM_HOURS:
			local->a->advanceUnits = advHours;
			break;
		case ALARM_DAYS:
			local->a->advanceUnits = advDays;
			break;
		}
	} else {
		local->a->alarm = 0;
		local->a->advance = 0;
		local->a->advanceUnits = advMinutes;
	}

	/* set the recurrence parameters */
	if (local->ical->recur != NULL) {
		switch (local->ical->recur->type) {
		case RECUR_DAILY:
			local->a->repeatType = repeatDaily;
			break;
		case RECUR_WEEKLY:
			local->a->repeatType = repeatWeekly;
			break;
		case RECUR_MONTHLY_BY_POS:
			local->a->repeatType = repeatMonthlyByDate;
			break;
		case RECUR_MONTHLY_BY_DAY:
			local->a->repeatType = repeatMonthlyByDay;
			break;
		case RECUR_YEARLY_BY_MONTH:
			local->a->repeatType = repeatYearly;
			break;
		case RECUR_YEARLY_BY_DAY:
			local->a->repeatType = repeatYearly;
			break;
		}
		if (local->ical->recur->duration == 0) {
			local->a->repeatForever = 1;
		} else {
			local->a->repeatForever = 0;
			local->a->repeatEnd = *localtime(&local->ical->recur->_enddate);
		}
		local->a->repeatFrequency = local->ical->recur->interval; 


		for ( daycount=0; daycount<7; daycount++ ) {
			if (local->ical->recur->weekday & (1 << daycount))
				local->a->repeatDays[daycount] = 1;
 		}
	} else {
		local->a->repeatType = repeatNone;
		local->a->repeatForever = 0;
		local->a->repeatEnd = local->a->end;
		local->a->repeatFrequency = 0;
		local->a->repeatDay = dom1stSun;
		local->a->repeatDays[0] = 0;
		local->a->repeatDays[1] = 0;
		local->a->repeatDays[2] = 0;
		local->a->repeatDays[3] = 0;
		local->a->repeatDays[4] = 0;
		local->a->repeatDays[5] = 0;
		local->a->repeatDays[6] = 0;
		local->a->repeatWeekstart = 0;
		local->a->exceptions = 0;
		local->a->exception = NULL;
	}

	/* STOP: don't replace these with g_strdup, since free_Appointment
	   uses free to deallocte */
	local->a->note = 
		local->ical->comment==NULL?NULL:strdup(local->ical->comment);
	local->a->description = 
		local->ical->summary==NULL?NULL:strdup(local->ical->summary);

	/* Generate pilot record structure */
	p->record = g_new0(char,0xffff);
	p->length = pack_Appointment(local->a,p->record,0xffff);

#if 0
	g_message("calconduit: new item from %s to %s",asctime(&(local->a->begin)),asctime(&(local->a->end))); 
	
	g_message("local->a->note = %s",local->a->note);
	g_message("local->a->description = %s",local->a->description);
	g_message("sizeof(p->record) = %d, length is %d",sizeof(p->record),p->length);
	for(x=0;x<p->length;x+=32) {
		for(y=x;y<x+32;y++)
			if(p->record[y]<33 || p->record[y]>128)
				printf("%02X",p->record[y]);
			else 
				printf(" %c",p->record[y]);
		printf("\n");
	}
#endif

	*remote = p;

	return 0;
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

	if(calendar!=CORBA_OBJECT_NIL)
		GNOME_Calendar_Repository_done (calendar, &ev);

        cc = GET_CONFIG(conduit);
        destroy_configuration(&cc);

	cd = GET_DATA(conduit);
	g_free(cd);

	gtk_object_destroy (GTK_OBJECT (conduit));

}


