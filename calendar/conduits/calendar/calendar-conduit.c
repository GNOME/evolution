/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#include <config.h>
#include <sys/stat.h>
#include <utime.h>
#include <unistd.h>
#include <pwd.h>
#include <signal.h>
#include <errno.h>

#include <cal-client/cal-client.h>
#include <cal-util/calobj.h>
#include <cal-util/timeutil.h>
#include <pi-source.h>
#include <pi-socket.h>
#include <pi-file.h>
#include <pi-dlp.h>
#include <libgnorba/gnorba.h>
#include <libgnorba/gnome-factory.h>
#include <pi-version.h>
#include <gpilotd/gnome-pilot-conduit.h>
#include <gpilotd/gnome-pilot-conduit-standard-abs.h>
#include <calendar-conduit.h>

//#include "GnomeCal.h"

GnomePilotConduit * conduit_get_gpilot_conduit (guint32);
void conduit_destroy_gpilot_conduit (GnomePilotConduit*);
void local_record_from_icalobject (GCalLocalRecord *local, iCalObject *obj);

#define CONDUIT_VERSION "0.8.11"
#ifdef G_LOG_DOMAIN
#undef G_LOG_DOMAIN
#endif
#define G_LOG_DOMAIN "gcalconduit" 

#define DEBUG_CALCONDUIT 
#undef DEBUG_CALCONDUIT

#ifdef DEBUG_CALCONDUIT
#define show_exception(e) g_warning ("Exception: %s\n", CORBA_exception_id (e))
#define LOG(e...) g_log(G_LOG_DOMAIN,G_LOG_LEVEL_MESSAGE, e)
#else
#define show_exception(e)
#define LOG(e...)
#endif 

#define WARN(e...) g_log(G_LOG_DOMAIN,G_LOG_LEVEL_WARNING, e)
#define INFO(e...) g_log(G_LOG_DOMAIN,G_LOG_LEVEL_MESSAGE, e)

#define catch_ret_val(_env,ret)                                                                 \
  if (_env._major != CORBA_NO_EXCEPTION) {                                                      \
        g_log(G_LOG_DOMAIN,G_LOG_LEVEL_MESSAGE,"%s:%d: Caught exception",__FILE__,__LINE__);    \
        g_warning ("Exception: %s\n", CORBA_exception_id (&(_env)));                               \
	CORBA_exception_free(&(_env));                                                             \
	return ret;                                                                             \
  }




/* Destroys any data allocated by gcalconduit_load_configuration
   and deallocates the given configuration. */
static void 
gcalconduit_destroy_configuration(GCalConduitCfg **c) 
{
	g_return_if_fail(c!=NULL);
	g_return_if_fail(*c!=NULL);
	g_free(*c);
	*c = NULL;
}


/* Given a GCalConduitContxt*, allocates the structure */
static void
gcalconduit_new_context(GCalConduitContext **ctxt,
			GCalConduitCfg *c) 
{
	*ctxt = g_new0(GCalConduitContext,1);
	g_assert(ctxt!=NULL);
	(*ctxt)->cfg = c;
	CORBA_exception_init (&((*ctxt)->ev));
}


/* Destroys any data allocated by gcalconduit_new_context
   and deallocates its data. */
static void
gcalconduit_destroy_context(GCalConduitContext **ctxt)
{
	g_return_if_fail(ctxt!=NULL);
	g_return_if_fail(*ctxt!=NULL);
/*
	if ((*ctxt)->cfg!=NULL)
		gcalconduit_destroy_configuration(&((*ctxt)->cfg));
*/
	g_free(*ctxt);
	*ctxt = NULL;
}


static void
gnome_calendar_load_cb (GtkWidget *cal_client,
			CalClientLoadStatus status,
			GCalConduitContext *ctxt)
{
	CalClient *client = CAL_CLIENT (cal_client);

	printf ("entering gnome_calendar_load_cb, tried=%d\n",
		ctxt->calendar_load_tried);

	if (status == CAL_CLIENT_LOAD_SUCCESS) {
		ctxt->calendar_load_success = TRUE;
		printf ("  success\n");
		gtk_main_quit (); /* end the sub event loop */
	} else {
		if (ctxt->calendar_load_tried) {
			printf ("load and create of calendar failed\n");
			gtk_main_quit (); /* end the sub event loop */
			return;
		}

		cal_client_create_calendar (client, ctxt->calendar_file);
		ctxt->calendar_load_tried = 1;
	}
}





static int
start_calendar_server (GnomePilotConduitStandardAbs *conduit,
		       GCalConduitContext *ctxt)
{
	
	g_return_val_if_fail(conduit!=NULL,-2);
	g_return_val_if_fail(ctxt!=NULL,-2);

	ctxt->client = cal_client_new ();

	/* FIX ME */
	ctxt->calendar_file = g_concat_dir_and_file (g_get_home_dir (),
			       "evolution/local/Calendar/calendar.vcf");

	gtk_signal_connect (GTK_OBJECT (ctxt->client), "cal_loaded",
			    gnome_calendar_load_cb, ctxt);

	printf ("calling cal_client_load_calendar\n");
	cal_client_load_calendar (ctxt->client, ctxt->calendar_file);

	/* run a sub event loop to turn cal-client's async load
	   notification into a synchronous call */
	gtk_main ();

	if (ctxt->calendar_load_success)
		return 0;

	return -1;
}


#if 0
/* Just a stub to link with */
void calendar_notify (time_t time, CalendarAlarm *which, void *data);
void calendar_notify (time_t time, CalendarAlarm *which, void *data) { }
#endif /* 0 */


static GSList * 
get_calendar_objects(GnomePilotConduitStandardAbs *conduit,
		     gboolean *status,
		     GCalConduitContext *ctxt) 
{
	GList *uids;
	GSList *result = NULL;

	g_return_val_if_fail (conduit != NULL, NULL);
	g_return_val_if_fail (ctxt != NULL, NULL);

	/* uids = cal_client_get_uids (ctxt->client, CALOBJ_TYPE_ANY); */
	uids = cal_client_get_uids (ctxt->client,
				    CALOBJ_TYPE_EVENT |
				    /*CALOBJ_TYPE_TODO |*/
				    CALOBJ_TYPE_JOURNAL);

	if (status != NULL)
		(*status) = TRUE;

	if (! uids)
		INFO ("No entries found");
	else {
		GList *c;
		for (c=uids; c; c=c->next)
			result = g_slist_prepend (result, (gchar *) c->data);
		/* FIX ME free uids */
	}

	return result;
}


static void 
local_record_from_ical_uid (GCalLocalRecord *local,
			    char *uid,
			    GCalConduitContext *ctxt)
{
	iCalObject *obj;
	CalClientGetStatus status;

	g_assert(local!=NULL);

	status = cal_client_get_object (ctxt->client, uid, &obj);

	if (status == CAL_CLIENT_GET_SUCCESS)
		local_record_from_icalobject(local,obj);
	else
		INFO ("Object did not exist");
}


/*
 * converts a iCalObject to a GCalLocalRecord
 */

void
local_record_from_icalobject(GCalLocalRecord *local,
			     iCalObject *obj) 
{
	g_return_if_fail(local!=NULL);
	g_return_if_fail(obj!=NULL);

	local->ical = obj;
	local->local.ID = local->ical->pilot_id;
/*
	LOG ("local->Id = %ld [%s], status = %d",
		  local->local.ID,obj->summary,local->ical->pilot_status);
*/
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
static GCalLocalRecord *
find_record_in_repository(GnomePilotConduitStandardAbs *conduit,
			  PilotRecord *remote,
			  GCalConduitContext *ctxt) 
{
	char *uid = NULL;
	GCalLocalRecord *loc;
	CalClientGetStatus status;
	iCalObject *obj;
  
	g_return_val_if_fail(conduit!=NULL,NULL);
	g_return_val_if_fail(remote!=NULL,NULL);
  
	LOG ("requesting %ld", remote->ID);


	status = cal_client_get_uid_by_pilot_id (ctxt->client, remote->ID, &uid);

	if (status == CAL_CLIENT_GET_SUCCESS) {
		status = cal_client_get_object (ctxt->client, uid, &obj);
		if (status == CAL_CLIENT_GET_SUCCESS) {
			LOG ("Found");
			loc = g_new0(GCalLocalRecord,1);
			/* memory allocated in new_from_string is freed in free_match */
			local_record_from_icalobject (loc, obj);
			return loc;
		}
	}

	INFO ("Object did not exist");
	return NULL;
}


/* 
 * updates an given iCalObject in the repository
 */
static void
update_calendar_entry_in_repository(GnomePilotConduitStandardAbs *conduit,
				    iCalObject *obj,
				    GCalConduitContext *ctxt) 
{
	gboolean success;

	g_return_if_fail(conduit!=NULL);
	g_return_if_fail(obj!=NULL);

	success = cal_client_update_object (ctxt->client, obj);
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
	       PilotRecord *remote,
	       GCalConduitContext *ctxt)
{
	iCalObject *obj;
	struct Appointment a;
	CalClientGetStatus status;
	char *uid;

	g_return_val_if_fail(remote!=NULL,-1);

	memset(&a,0,sizeof(struct Appointment));
	unpack_Appointment(&a,remote->record,remote->length);

	LOG ("requesting %ld [%s]", remote->ID, a.description);
	printf ("requesting %ld [%s]\n", remote->ID, a.description);

	status = cal_client_get_uid_by_pilot_id(ctxt->client, remote->ID, &uid);
	if (status == CAL_CLIENT_GET_SUCCESS)
		status = cal_client_get_object (ctxt->client, uid, &obj);

	if (status != CAL_CLIENT_GET_SUCCESS) {
		time_t now = time (NULL);

		LOG ("Object did not exist, creating a new one");
		printf ("Object did not exist, creating a new one\n");

		obj = ical_new (a.note ? a.note : "",
				g_get_user_name (),
				a.description ? a.description : "");
		
		obj->created = now;
		obj->last_mod = now;
		obj->priority = 0;
		obj->transp = 0;
		obj->related = NULL;
		obj->pilot_id = remote->ID;
		obj->pilot_status = ICAL_PILOT_SYNC_NONE;
	} else {
		iCalObject *new_obj;
		LOG ("Found");
		printf ("Found\n");
		new_obj = ical_from_remote_record (conduit, remote, obj);
		obj = new_obj;
	}

	/* update record on server */
	
	update_calendar_entry_in_repository (conduit, obj, ctxt);
	cal_client_update_pilot_id (ctxt->client, obj->uid, obj->pilot_id,
				    ICAL_PILOT_SYNC_NONE);

	/*
	 * Shutdown
	 */
	ical_object_unref (obj);
	free_Appointment(&a);

	return 0;
}

static void
check_for_slow_setting (GnomePilotConduit *c, GCalConduitContext *ctxt)
{
	GList *uids;
	unsigned long int entry_number;

	/* get all but TODOs, those are handled by the todo conduit */
	/* uids = cal_client_get_uids (ctxt->client, CALOBJ_TYPE_ANY); */
	uids = cal_client_get_uids (ctxt->client,
				    CALOBJ_TYPE_EVENT |
				    /*CALOBJ_TYPE_TODO |*/
				    CALOBJ_TYPE_JOURNAL);

	entry_number = g_list_length (uids);

	LOG (_("Calendar holds %d entries"), entry_number);
	/* If the local base is empty, do a slow sync */
	if (entry_number == 0) {
		GnomePilotConduitStandard *conduit;
		conduit = GNOME_PILOT_CONDUIT_STANDARD (c);
		gnome_pilot_conduit_standard_set_slow (conduit);
	}
}

static gint
pre_sync (GnomePilotConduit *c,
	  GnomePilotDBInfo *dbi,
	  GCalConduitContext *ctxt)
{
	int l;
	unsigned char *buf;
	GnomePilotConduitStandardAbs *conduit;
	/* gint num_records; */
	//GList *uids;


	/*
	g_log_set_always_fatal (G_LOG_LEVEL_ERROR |
				G_LOG_LEVEL_CRITICAL |
				G_LOG_LEVEL_WARNING);
	*/


	conduit = GNOME_PILOT_CONDUIT_STANDARD_ABS(c);
  
	g_message ("GnomeCal Conduit v.%s",CONDUIT_VERSION);

	ctxt->client = NULL;
	
	if (start_calendar_server (GNOME_PILOT_CONDUIT_STANDARD_ABS(c), ctxt) != 0) {
		WARN(_("Could not start gnomecal server"));
		gnome_pilot_conduit_error(GNOME_PILOT_CONDUIT(c),
					  _("Could not start gnomecal server"));
		return -1;
	}


#if 0  
	/* Set the counters for the progress bar crap */
	num_records = GNOME_Calendar_Repository_get_number_of_objects (ctxt->calendar, GNOME_Calendar_Repository_ANY, &(ctxt->ev));

	catch_ret_val (ctxt->ev, -1);
	gnome_pilot_conduit_standard_abs_set_num_local_records(GNOME_PILOT_CONDUIT_STANDARD_ABS(c), num_records);
	num_records = GNOME_Calendar_Repository_get_number_of_objects (ctxt->calendar, GNOME_Calendar_Repository_MODIFIED, &(ctxt->ev));
	catch_ret_val (ctxt->ev, -1);
	gnome_pilot_conduit_standard_abs_set_num_updated_local_records(GNOME_PILOT_CONDUIT_STANDARD_ABS(c), num_records);
	num_records = GNOME_Calendar_Repository_get_number_of_objects (ctxt->calendar, GNOME_Calendar_Repository_NEW, &(ctxt->ev));
	catch_ret_val (ctxt->ev, -1);
	gnome_pilot_conduit_standard_abs_set_num_new_local_records(GNOME_PILOT_CONDUIT_STANDARD_ABS(c), num_records);
	num_records = GNOME_Calendar_Repository_get_number_of_objects (ctxt->calendar, GNOME_Calendar_Repository_DELETED, &(ctxt->ev));
	catch_ret_val (ctxt->ev, -1);
	gnome_pilot_conduit_standard_abs_set_num_deleted_local_records(GNOME_PILOT_CONDUIT_STANDARD_ABS(c), num_records);
#endif /* 0 */

	gtk_object_set_data(GTK_OBJECT(c),"dbinfo",dbi);
  
	/* load_records(c); */

	buf = (unsigned char*)g_malloc(0xffff);
	if((l=dlp_ReadAppBlock(dbi->pilot_socket,dbi->db_handle,0,(unsigned char *)buf,0xffff)) < 0) {
		WARN(_("Could not read pilot's DateBook application block"));
		WARN("dlp_ReadAppBlock(...) = %d",l);
		gnome_pilot_conduit_error(GNOME_PILOT_CONDUIT(c),
			     _("Could not read pilot's DateBook application block"));
		return -1;
	}
	unpack_AppointmentAppInfo(&(ctxt->ai),buf,l);
	g_free(buf);

	check_for_slow_setting(c,ctxt);

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
		 GCalLocalRecord **local,
		 PilotRecord *remote,
		 GCalConduitContext *ctxt)
{
	LOG ("in match_record");

	g_return_val_if_fail(local!=NULL,-1);
	g_return_val_if_fail(remote!=NULL,-1);

	*local = find_record_in_repository(conduit,remote,ctxt);
  
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
		 GCalLocalRecord **local,
		 GCalConduitContext *ctxt)
{
	LOG ("entering free_match");

	g_return_val_if_fail(local!=NULL,-1);
	g_return_val_if_fail(*local!=NULL,-1);

	ical_object_unref (GCAL_LOCALRECORD(*local)->ical); 
	g_free(*local);
	
        *local = NULL;
	return 0;
}

/*
  Move to archive and set status to Nothing
 */
static gint
archive_local (GnomePilotConduitStandardAbs *conduit,
	       GCalLocalRecord *local,
	       GCalConduitContext *ctxt)
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
		GCalLocalRecord *local,
		PilotRecord *remote,
		GCalConduitContext *ctxt)
{
	LOG ("entering archive_remote");

        //g_return_val_if_fail(remote!=NULL,-1);
	//g_return_val_if_fail(local!=NULL,-1);

	return -1;
}

/*
  Store and set status to Nothing
 */
static gint
store_remote (GnomePilotConduitStandardAbs *conduit,
	      PilotRecord *remote,
	      GCalConduitContext *ctxt)
{
	LOG ("entering store_remote");

	g_return_val_if_fail(remote!=NULL,-1);
	remote->attr = GnomePilotRecordNothing;

	return update_record(conduit,remote,ctxt);
}

static gint
clear_status_archive_local (GnomePilotConduitStandardAbs *conduit,
			    GCalLocalRecord *local,
			    GCalConduitContext *ctxt)
{
	LOG ("entering clear_status_archive_local");

	g_return_val_if_fail(local!=NULL,-1);

        return -1;
}

static gint
iterate (GnomePilotConduitStandardAbs *conduit,
	 GCalLocalRecord **local,
	 GCalConduitContext *ctxt)
{
	static GSList *events,*iterator;
	static int hest;

	g_return_val_if_fail(local!=NULL,-1);

	if(*local==NULL) {
		LOG ("beginning iteration");

		events = get_calendar_objects(conduit,NULL,ctxt);
		hest = 0;
		
		if(events!=NULL) {
			LOG ("iterating over %d records", g_slist_length (events));
			*local = g_new0(GCalLocalRecord,1);

			local_record_from_ical_uid(*local,(gchar*)events->data,ctxt);
			iterator = events;
		} else {
			LOG ("no events");
			(*local) = NULL;
		}
	} else {
		/* printf ("continuing iteration\n"); */
		hest++;
		if(g_slist_next(iterator)==NULL) {
			GSList *l;

			LOG ("ending");
			/** free stuff allocated for iteration */
			g_free((*local));

			LOG ("iterated over %d records", hest);
			for(l=events;l;l=l->next)
				g_free(l->data);

			g_slist_free(events);

			/* ends iteration */
			(*local) = NULL;
			return 0;
		} else {
			iterator = g_slist_next(iterator);
			local_record_from_ical_uid(*local,(gchar*)(iterator->data),ctxt);
		}
	}
	return 1;
}


static gint
iterate_specific (GnomePilotConduitStandardAbs *conduit,
		  GCalLocalRecord **local,
		  gint flag,
		  gint archived,
		  GCalConduitContext *ctxt)
{
#ifdef DEBUG_CALCONDUIT
	{
		gchar *tmp;
		switch (flag) {
		case GnomePilotRecordNothing: tmp = g_strdup("RecordNothing"); break;
		case GnomePilotRecordModified: tmp = g_strdup("RecordModified"); break;
		case GnomePilotRecordNew: tmp = g_strdup("RecordNew"); break;
		default: tmp = g_strdup_printf("0x%x",flag); break;
		}
		printf ("entering iterate_specific(flag = %s)\n", tmp);
		g_free(tmp);
	}
#endif
	g_return_val_if_fail(local!=NULL,-1);

	/* iterate until a record meets the criteria */
	while(gnome_pilot_conduit_standard_abs_iterate(conduit,(LocalRecord**)local)) {
		if((*local)==NULL) break;
		if(archived && ((*local)->local.archived==archived)) break;
		if(((*local)->local.attr == flag)) break;
	}

	return (*local)==NULL?0:1;
}

static gint
purge (GnomePilotConduitStandardAbs *conduit,
       GCalConduitContext *ctxt)
{
	LOG ("entering purge");

	/* HEST, gem posterne her */

	return -1;
}


static gint
set_status (GnomePilotConduitStandardAbs *conduit,
	    GCalLocalRecord *local,
	    gint status,
	    GCalConduitContext *ctxt)
{
	gboolean success;
	LOG ("entering set_status(status=%d)",status);

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
	
	if (status == GnomePilotRecordDeleted) {
		success = cal_client_remove_object (ctxt->client, local->ical->uid);
	} else {
		success = cal_client_update_object (ctxt->client, local->ical);
		cal_client_update_pilot_id (ctxt->client, local->ical->uid,
					    local->local.ID,
					    local->ical->pilot_status);
	}

	if (! success) {
		WARN (_("Error while communicating with calendar server"));
	}
	
        return 0;
}

static gint
set_archived (GnomePilotConduitStandardAbs *conduit,
	      GCalLocalRecord *local,
	      gint archived,
	      GCalConduitContext *ctxt)
{
	LOG ("entering set_archived");

	g_return_val_if_fail(local!=NULL,-1);
	g_assert(local->ical!=NULL);

	local->local.archived = archived;
	update_calendar_entry_in_repository(conduit,local->ical,ctxt);
	/* FIXME: This should move the entry into a speciel
	   calendar file, eg. Archive, or (by config option), simply
	   delete it */
        return 0;
}

static gint
set_pilot_id (GnomePilotConduitStandardAbs *conduit,
	      GCalLocalRecord *local,
	      guint32 ID,
	      GCalConduitContext *ctxt)
{
	LOG ("entering set_pilot_id(id=%d)",ID);

	g_return_val_if_fail(local!=NULL,-1);
	g_assert(local->ical!=NULL);

	local->local.ID = ID;
	local->ical->pilot_id = ID;

	cal_client_update_pilot_id (ctxt->client,
				    local->ical->uid,
				    local->local.ID,
				    local->ical->pilot_status);

        return 0;
}

static gint
transmit (GnomePilotConduitStandardAbs *conduit,
	  GCalLocalRecord *local,
	  PilotRecord **remote,
	  GCalConduitContext *ctxt)
{
	PilotRecord *p;
	int daycount;
	
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
	/* This is some debug code that hexdumps the calendar entry...
	   You won't need this. */
	{
		int x,y;
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
	}
#endif

	*remote = p;

	return 0;
}

static gint
free_transmit (GnomePilotConduitStandardAbs *conduit,
	       GCalLocalRecord *local,
	       PilotRecord **remote,
	       GCalConduitContext *ctxt)
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
compare (GnomePilotConduitStandardAbs *conduit,
	    GCalLocalRecord *local,
	    PilotRecord *remote,
	    GCalConduitContext *ctxt)
{
	/* used by the quick compare */
	PilotRecord *remoteOfLocal;
	int err;
	int retval;

	/* used by the tedious compare */
	//struct Appointment a; 

	g_message ("entering compare");

	g_return_val_if_fail (local!=NULL,-1);
	g_return_val_if_fail (remote!=NULL,-1);
#if 1
	err = transmit(conduit,local,&remoteOfLocal,ctxt);
	if (err != 0) return err;

	retval = 0;
	if (remote->length == remoteOfLocal->length) {
		if (memcmp(remoteOfLocal->record,remote->record,remote->length)!=0) {
			g_message("compare failed on contents");
			retval = 1;
		}
	} else {
		g_message("compare failed on length");
		retval = 1;
	}

	free_transmit(conduit,local,&remoteOfLocal,ctxt);	
	return retval;

#else
	/** FIXME: All the { LOG("yadayada"); return 1; } bloat is for debug purposes.
	    Once this is known to work, compact to return 1;'s */

	/* Check record attributes */
	if (local->local.ID != remote->ID) {
		LOG("failed local->local.ID == remote->ID");
		return 1;
	}
	if (local->local.attr != remote->attr) {
		LOG("failed local->local.attr == remote->attr");
		return 1; 
	}
	if (local->local.archived != remote->archived) {
		LOG("failed local->local.archived == remote->archived");
		return 1;
	}
	if (local->local.secret != remote->secret) {
		LOG("failed local->local.secret == remote->secret");
		return 1;
	}

	unpack_Appointment(&a,remote->record,remote->length);
	
	/* Check records begin/end time */
	if (a.event==0) {
/* FIXME
		if (a.begin != *localtime(&local->ical->dtstart)) {
			LOG("a.begin == *localtime(&local->ical->dtstart)");
			return 1;
		}
		if (a.end != *localtime(&local->ical->dtend)) {
			LOG("a.end == *localtime(&local->ical->dtend)");
			return 1;
		}			
*/
	} else {
		LOG("failed local->a.event != 0, unsupported by gnomecal");
		return 1;
	}

	/* Check records alarm settings */
	if(a.alarm == 1) {
		if (local->ical->aalarm.enabled == 1) {
			if (a.advance != local->ical->aalarm.count) {
				LOG("failed a.advance == local->ical->aalarm.count");
				return 1;
			}
			switch(local->ical->aalarm.units) {
			case ALARM_MINUTES:
				if (a.advanceUnits != advMinutes) {
					LOG("failed local->ical->aalarm.units == a.advanceUnits");
					return 1;
				}
				break;
			case ALARM_HOURS:
				if (a.advanceUnits != advHours) {
					LOG("failed local->ical->aalarm.units == a.advanceUnits");
					return 1;
				}
				break;
			case ALARM_DAYS:
				if (a.advanceUnits != advDays) {
					LOG("failed local->ical->aalarm.units == a.advanceUnits");
					return 1;
				}
				break;
			}
		} else {
			LOG("failed a.alarm == 1 && local->ical->aalarm.enabled == 1");
			return 1;
		}
	} else if  (local->ical->aalarm.enabled == 1) {
		LOG("failed a.alarm != 1 && local->ical->aalarm.enabled != 1");
		return 1;
	}

	/* Check records recurrence settings */
        /* If this code is broken, a more or less safe although not efficient
	   approach is (other the fixing the bug), if either has recurrence, 
	   return 1, thus failing the comparision */
	if (local->ical->recur != NULL) {
		if (a.repeatType == repeatNone) {
			LOG("failed: local->ical->recur != NULL && a.repeatType != repeatNone");
			return 1;
		} 
		switch (local->ical->recur->type) {
		case RECUR_DAILY:
			if (a.repeatType != repeatDaily) {
				LOG("failed a.repeatType == repeatDaily");
				return 1; }
			break;
		case RECUR_WEEKLY:
			if (a.repeatType != repeatWeekly) {
				LOG("failed a.repeatType == repeatWeekly");
				return 1; }
			break;
		case RECUR_MONTHLY_BY_POS:
			if (a.repeatType != repeatMonthlyByDate) {
				LOG("failed a.repeatType == repeatMonthlyByDate");
				return 1; }
			break;
		case RECUR_MONTHLY_BY_DAY:
			if (a.repeatType != repeatMonthlyByDay) {
				LOG("failed a.repeatType == repeatMonthlyByDay");
				return 1; }
			break;
		case RECUR_YEARLY_BY_MONTH:
			if (a.repeatType != repeatYearly) {
				LOG("failed a.repeatType == repeatYearly");
				return 1; }
			break;
		case RECUR_YEARLY_BY_DAY:
			if (a.repeatType != repeatYearly) {
				LOG("failed a.repeatType == repeatYearly");
				return 1; }
			break;
		}
		if (local->ical->recur->duration == 0) {
			if(a.repeatForever != 1) {
				LOG("failed local->ical->recur->duration == 0 && a.repeatForever == 1");
				return 1;
			}
		} else {
			if(a.repeatForever != 0) {
				LOG("failed local->ical->recur->duration != 0 && ! a.repeatForever == 0");
				return 1;
			}
/* FIXME
			if(a.repeatEnd != *localtime(&local->ical->recur->_enddate)) {
				LOG("failed a.repeatEnd == *localtime(&local->ical->recur->_enddate)");
				return 1;
			}
*/
		}
		if (a.repeatFrequency != local->ical->recur->interval) {
			LOG("failed a.repeatFrequency == local->ical->recur->interval");
			return 1;
		}
		for (daycount = 0; daycount<7; daycount++) {
			if(local->ical->recur->weekday & (1<<daycount)) {
				if (a.repeatDays[daycount]!=1) {
					LOG("failed local->ical->recur->weekday & (1<<daycount) && a.repeatDays[daycount]==1");
					return 1;
				}
			} else {
				if (a.repeatDays[daycount]!=0) {
					LOG("failed local->ical->recur->weekday &! (1<<daycount) && a.repeatDays[daycount]==0");
					return 1;
				}
			}
		}
	} else if (a.repeatType != repeatNone ) {
		LOG("failed: local->ical->recur == NULL && a.repeatType == repeatNone");
		return 1;
	}

	/* check the note and description */
	if(a.note!=NULL) {
		if(local->ical->comment==NULL) {
			LOG("failed a.note != NULL && local->ical->coment != NULL");
			return 1;
		}
		if(strcmp(local->ical->comment,a.note)!=0) {
			LOG("failed strcmp(local->ical->comment,a.note)==0");
			return 1;
		}
	} if(local->ical->comment!=NULL) {
		LOG("failed a.note == NULL && local->ical->coment == NULL");
		return 1;
	}
	if(a.description!=NULL) {
		if(local->ical->summary==NULL) {
			LOG("failed a.description != NULL && local->ical->coment != NULL");
			return 1;
		}
		if(strcmp(local->ical->summary,a.description)!=0) {
			LOG("failed strcmp(local->ical->summary,a.description)==0");
			return 1;
		}
	} if(local->ical->summary!=NULL) {
		LOG("failed a.description == NULL && local->ical->coment == NULL");
		return 1;
	}
#endif
        return 0;
}

static gint
compare_backup (GnomePilotConduitStandardAbs *conduit,
		GCalLocalRecord *local,
		PilotRecord *remote,
		GCalConduitContext *ctxt)
{
	LOG ("entering compare_backup");

	g_return_val_if_fail(local!=NULL,-1);
	g_return_val_if_fail(remote!=NULL,-1);

        return -1;
}


static gint
delete_all (GnomePilotConduitStandardAbs *conduit,
	    GCalConduitContext *ctxt)
{
	GSList *events,*it;
	gboolean error;
	gboolean success;

	events = get_calendar_objects(conduit,&error,ctxt);

	if (error == FALSE) return -1;
	for (it=events; it; it = g_slist_next (it)) {
		success = cal_client_remove_object (ctxt->client, it->data);

		if (!success)
			INFO ("Object did not exist");

		g_free (it->data);
	}

	g_slist_free (events);
        return -1;
}


GnomePilotConduit *
conduit_get_gpilot_conduit (guint32 pilotId)
{
	GtkObject *retval;
	GCalConduitCfg *cfg;
	GCalConduitContext *ctxt;

	retval = gnome_pilot_conduit_standard_abs_new ("DatebookDB", 0x64617465);
	g_assert (retval != NULL);
	gnome_pilot_conduit_construct(GNOME_PILOT_CONDUIT(retval),"GnomeCalConduit");

	gcalconduit_load_configuration(&cfg,pilotId);
	gtk_object_set_data(retval,"gcalconduit_cfg",cfg);

	gcalconduit_new_context(&ctxt,cfg);
	gtk_object_set_data(GTK_OBJECT(retval),"gcalconduit_context",ctxt);

	gtk_signal_connect (retval, "match_record", (GtkSignalFunc) match_record, ctxt);
	gtk_signal_connect (retval, "free_match", (GtkSignalFunc) free_match, ctxt);
	gtk_signal_connect (retval, "archive_local", (GtkSignalFunc) archive_local, ctxt);
	gtk_signal_connect (retval, "archive_remote", (GtkSignalFunc) archive_remote, ctxt);
	gtk_signal_connect (retval, "store_remote", (GtkSignalFunc) store_remote, ctxt);
	gtk_signal_connect (retval, "clear_status_archive_local", (GtkSignalFunc) clear_status_archive_local, ctxt);
	gtk_signal_connect (retval, "iterate", (GtkSignalFunc) iterate, ctxt);
	gtk_signal_connect (retval, "iterate_specific", (GtkSignalFunc) iterate_specific, ctxt);
	gtk_signal_connect (retval, "purge", (GtkSignalFunc) purge, ctxt);
	gtk_signal_connect (retval, "set_status", (GtkSignalFunc) set_status, ctxt);
	gtk_signal_connect (retval, "set_archived", (GtkSignalFunc) set_archived, ctxt);
	gtk_signal_connect (retval, "set_pilot_id", (GtkSignalFunc) set_pilot_id, ctxt);
	gtk_signal_connect (retval, "compare", (GtkSignalFunc) compare, ctxt);
	gtk_signal_connect (retval, "compare_backup", (GtkSignalFunc) compare_backup, ctxt);
	gtk_signal_connect (retval, "free_transmit", (GtkSignalFunc) free_transmit, ctxt);
	gtk_signal_connect (retval, "delete_all", (GtkSignalFunc) delete_all, ctxt);
	gtk_signal_connect (retval, "transmit", (GtkSignalFunc) transmit, ctxt);
	gtk_signal_connect (retval, "pre_sync", (GtkSignalFunc) pre_sync, ctxt);

	return GNOME_PILOT_CONDUIT (retval);
}

void
conduit_destroy_gpilot_conduit (GnomePilotConduit *conduit)
{ 
        GCalConduitCfg *cc;
	GCalConduitContext *ctxt;

        cc = GET_GCALCONFIG(conduit);
	ctxt = GET_GCALCONTEXT(conduit);

	if (ctxt->client != NULL) {
		gtk_object_unref (GTK_OBJECT (ctxt->client));
		//pi_close (ctxt->link);
		//GNOME_Calendar_Repository_done (ctxt->calendar, &(ctxt->ev));
	}

        gcalconduit_destroy_configuration (&cc);

	gcalconduit_destroy_context (&ctxt);

	gtk_object_destroy (GTK_OBJECT (conduit));
}
