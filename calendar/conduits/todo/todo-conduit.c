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
#include <todo-conduit.h>

//#include "GnomeCal.h"

GnomePilotConduit * conduit_get_gpilot_conduit (guint32);
void conduit_destroy_gpilot_conduit (GnomePilotConduit*);
void local_record_from_icalobject (GCalLocalRecord *local, iCalObject *obj);

#define CONDUIT_VERSION "0.8.11"
#ifdef G_LOG_DOMAIN
#undef G_LOG_DOMAIN
#endif
#define G_LOG_DOMAIN "todoconduit" 

#define DEBUG_CALCONDUIT 1
/* #undef DEBUG_CALCONDUIT */

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

	uids = cal_client_get_uids (ctxt->client, CALOBJ_TYPE_TODO);

	printf ("got %d todo entries from cal server\n", g_list_length (uids));

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
	struct ToDo todo;
	time_t now;

	now = time (NULL);

	g_return_val_if_fail(remote!=NULL,NULL);
	memset (&todo, 0, sizeof (struct ToDo));
	unpack_ToDo (&todo, remote->record, remote->length);
	
	if (in_obj == NULL)
		obj = ical_new (todo.note ? todo.note : "",
				g_get_user_name (),
				todo.description ? todo.description : "");
	else 
		obj = in_obj;
	
	if (todo.note) {
		g_free (obj->comment);
		obj->comment = g_strdup (todo.note);
	}
	if (todo.description) {
		g_free (obj->summary);
		obj->summary = g_strdup (todo.description);
	}

	obj->type = ICAL_TODO;
	obj->new = TRUE;
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
	
	obj->dtend = mktime (& todo.due);

	if (todo.complete)
		obj->completed = now-5; /* FIX ME */

	printf ("[%s] from pilot, complete=%d/%ld\n",
		todo.description,
		todo.complete,
		obj->completed);

	obj->priority = todo.priority;

	g_free (obj->class);
	
	if (remote->attr & dlpRecAttrSecret)
		obj->class = g_strdup ("PRIVATE");
	else
		obj->class = g_strdup ("PUBLIC");


	free_ToDo(&todo);

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
	struct ToDo todo;
	CalClientGetStatus status;
	char *uid;

	g_return_val_if_fail(remote!=NULL,-1);

	memset (&todo, 0, sizeof (struct ToDo));
	unpack_ToDo (&todo, remote->record, remote->length);

	LOG ("requesting %ld [%s]", remote->ID, todo.description);
	printf ("requesting %ld [%s]\n", remote->ID, todo.description);

	status = cal_client_get_uid_by_pilot_id(ctxt->client, remote->ID, &uid);
	if (status == CAL_CLIENT_GET_SUCCESS)
		status = cal_client_get_object (ctxt->client, uid, &obj);

	if (status != CAL_CLIENT_GET_SUCCESS) {
		time_t now = time (NULL);

		LOG ("Object did not exist, creating a new one");
		printf ("Object did not exist, creating a new one\n");

		obj = ical_new (todo.note ? todo.note : "",
				g_get_user_name (),
				todo.description ? todo.description : "");

		obj->type = ICAL_TODO;
		obj->new = TRUE;
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
	free_ToDo(&todo);

	return 0;
}

static void
check_for_slow_setting (GnomePilotConduit *c, GCalConduitContext *ctxt)
{
	GList *uids;
	unsigned long int entry_number;

	uids = cal_client_get_uids (ctxt->client, CALOBJ_TYPE_TODO);

	entry_number = g_list_length (uids);

	LOG (_("Calendar holds %ld todo entries"), entry_number);
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
	unpack_ToDoAppInfo(&(ctxt->ai),buf,l);
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
	
	LOG ("entering transmit");

	g_return_val_if_fail(local!=NULL,-1);
	g_return_val_if_fail(remote!=NULL,-1);
	g_assert(local->ical!=NULL);

	p = g_new0(PilotRecord,1);

	p->ID = local->local.ID;
	p->attr = local->local.attr;
	p->archived = local->local.archived;
	p->secret = local->local.secret;

	local->todo = g_new0(struct ToDo,1);

	local->todo->indefinite = 0; /* FIX ME */
	local->todo->due = *localtime (&local->ical->dtend);
	local->todo->priority = local->ical->priority;

	if (local->ical->completed > 0)
		local->todo->complete = 1; /* FIX ME */

	/* STOP: don't replace these with g_strdup, since free_ToDo
	   uses free to deallocte */
	local->todo->description = 
		local->ical->summary==NULL?NULL:strdup(local->ical->summary);
	local->todo->note = 
		local->ical->comment==NULL?NULL:strdup(local->ical->comment);

	printf ("transmitting todo to pilot [%s] complete=%d/%ld\n",
		local->ical->summary==NULL?"NULL":local->ical->summary,
		local->todo->complete, local->ical->completed);

	/* Generate pilot record structure */
	p->record = g_new0(char,0xffff);
	p->length = pack_ToDo(local->todo,p->record,0xffff);

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

	free_ToDo(local->todo);
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

	g_message ("entering compare");
	printf ("entering compare\n");

	g_return_val_if_fail (local!=NULL,-1);
	g_return_val_if_fail (remote!=NULL,-1);

	err = transmit(conduit,local,&remoteOfLocal,ctxt);
	if (err != 0) return err;

	retval = 0;
	if (remote->length == remoteOfLocal->length) {
		if (memcmp(remoteOfLocal->record,remote->record,remote->length)!=0) {
			g_message("compare failed on contents");
			printf ("compare failed on contents\n");
			retval = 1;

			/* debug spew */
			{
				struct ToDo foolocal;
				struct ToDo fooremote;

				unpack_ToDo (&foolocal,
					     remoteOfLocal->record,
					     remoteOfLocal->length);
				unpack_ToDo (&fooremote,
					     remote->record,
					     remote->length);

				printf (" local:[%d %ld %d %d '%s' '%s']\n",
					foolocal.indefinite,
					mktime (& foolocal.due),
					foolocal.priority,
					foolocal.complete,
					foolocal.description,
					foolocal.note);

				printf ("remote:[%d %ld %d %d '%s' '%s']\n",
					fooremote.indefinite,
					mktime (& fooremote.due),
					fooremote.priority,
					fooremote.complete,
					fooremote.description,
					fooremote.note);
			}
		}
	} else {
		g_message("compare failed on length");
		printf("compare failed on length\n");
		retval = 1;
	}

	free_transmit(conduit,local,&remoteOfLocal,ctxt);	
	return retval;
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

	printf ("in todo's conduit_get_gpilot_conduit\n");

	retval = gnome_pilot_conduit_standard_abs_new ("ToDoDB", 0x746F646F);
	g_assert (retval != NULL);
	gnome_pilot_conduit_construct(GNOME_PILOT_CONDUIT(retval),"ToDoConduit");

	gcalconduit_load_configuration(&cfg,pilotId);
	gtk_object_set_data(retval,"todoconduit_cfg",cfg);

	gcalconduit_new_context(&ctxt,cfg);
	gtk_object_set_data(GTK_OBJECT(retval),"todoconduit_context",ctxt);

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
