/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#include <config.h>
#include <sys/stat.h>
#include <utime.h>
#include <unistd.h>
#include <pwd.h>
#include <signal.h>
#include <errno.h>

#include <cal-client/cal-client.h>
// #include <cal-util/calobj.h>
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
#include <libical/src/libical/icaltypes.h>

#include <bonobo.h>

//#include "GnomeCal.h"

GnomePilotConduit * conduit_get_gpilot_conduit (guint32);
void conduit_destroy_gpilot_conduit (GnomePilotConduit*);
void local_record_from_icalobject (GCalLocalRecord *local, CalComponent *obj);

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



/* debug spew DELETE ME */
static char *print_ical (CalComponent *obj
			 /*iCalObject *obj*/)
{
	static char buff[ 4096 ];

	int indefinite;
	CalComponentDateTime dtend;
	int priority;
	struct icaltimetype *complete;
	CalComponentText summary;
	GSList *comments;
	CalComponentText *first_comment = NULL;

	if (obj == NULL) {
		sprintf (buff, "[NULL]");
		return buff;
	}

	indefinite = 0; /* FIX ME how do i get this */
	cal_component_get_dtend (obj, &dtend);
	priority = 1; /* FIX ME how do i get this */
	cal_component_get_completed (obj, &complete);

	cal_component_get_summary (obj, &summary);
	cal_component_get_comment_list (obj, &comments);
	if (comments)
		first_comment = (CalComponentText *) comments->data;

	sprintf (buff, "[%d %d-%d-%d %d %d-%d-%d '%s' '%s']",
		 indefinite,
		 dtend.value->year, dtend.value->month, dtend.value->day,
		 priority,
		 complete->year, complete->month, complete->day,
		 summary.value,  /* description */
		 first_comment ? first_comment->value : "" /* note */
		 );
	return buff;
}


/* debug spew DELETE ME */
static char *print_local (GCalLocalRecord *local)
{
	static char buff[ 4096 ];

	if (local == NULL) {
		sprintf (buff, "[NULL]");
		return buff;
	}

	if (local->todo && local->todo->description) {
		sprintf (buff, "[%d %ld %d %d '%s' '%s']",
			 local->todo->indefinite,
			 mktime (& local->todo->due),
			 local->todo->priority,
			 local->todo->complete,
			 local->todo->description,
			 local->todo->note);
		return buff;
	}

	return print_ical (local->ical);
}


/* debug spew DELETE ME */
static char *print_remote (PilotRecord *remote)
{
	static char buff[ 4096 ];
	struct ToDo todo;

	if (remote == NULL) {
		sprintf (buff, "[NULL]");
		return buff;
	}

	memset (&todo, 0, sizeof (struct ToDo));
	unpack_ToDo (&todo, remote->record, remote->length);

	sprintf (buff, "[%d %ld %d %d '%s' '%s']",
		 todo.indefinite,
		 mktime (& todo.due),
		 todo.priority,
		 todo.complete,
		 todo.description,
		 todo.note);

	return buff;
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

	LOG ("    todo-conduit entering gnome_calendar_load_cb, tried=%d\n",
		ctxt->calendar_load_tried);

	if (status == CAL_CLIENT_LOAD_SUCCESS) {
		ctxt->calendar_load_success = TRUE;
		LOG ("    success\n");
		gtk_main_quit (); /* end the sub event loop */
	} else {
		if (ctxt->calendar_load_tried) {
			LOG ("    load and create of calendar failed\n");
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

	LOG ("    calling cal_client_load_calendar\n");
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

	// LOG ("got %d todo entries from cal server\n", g_list_length (uids));

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
	//iCalObject *obj;
	CalComponent *obj;
	CalClientGetStatus status;

	g_assert(local!=NULL);

	status = cal_client_get_object (ctxt->client, uid, &obj);

	if (status == CAL_CLIENT_GET_SUCCESS)
		local_record_from_icalobject (local, obj);
	else
		INFO ("Object did not exist");
}



static char *gnome_pilot_status_to_string (gint status)
{
	switch(status) {
	case GnomePilotRecordPending: return "GnomePilotRecordPending";
	case GnomePilotRecordNothing: return "GnomePilotRecordNothing";
	case GnomePilotRecordDeleted: return "GnomePilotRecordDeleted";
	case GnomePilotRecordNew: return "GnomePilotRecordNew";
	case GnomePilotRecordModified: return "GnomePilotRecordModified";
	}

	return "Unknown";
}



/*
 * converts a iCalObject to a GCalLocalRecord
 */

void
local_record_from_icalobject(GCalLocalRecord *local,
			     CalComponent *obj) 
{
	//iCalPilotState pilot_status;
	unsigned long int pilot_status;

	g_return_if_fail(local!=NULL);
	g_return_if_fail(obj!=NULL);

	local->ical = obj;
	local->todo = NULL; /* ??? */
	cal_component_get_pilot_id (obj, &local->local.ID);
	cal_component_get_pilot_status (obj, &pilot_status);

	switch (pilot_status) {
	case ICAL_PILOT_SYNC_NONE: 
		local->local.attr = GnomePilotRecordNothing;
		break;
	case ICAL_PILOT_SYNC_MOD: 
		local->local.attr = GnomePilotRecordModified;
		break;
	case ICAL_PILOT_SYNC_DEL: 
		local->local.attr = GnomePilotRecordDeleted; 
		break;
	default:
		g_warning ("unhandled pilot status: %ld\n", pilot_status);
	}

	/* Records without a pilot_id are new */
	if(local->local.ID == 0)
		local->local.attr = GnomePilotRecordNew;

	/*
	local->local.secret = 0;
	if (obj->class!=NULL)
		if (strcmp(obj->class,"PRIVATE")==0)
			local->local.secret = 1;
	*/

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
	//iCalObject *obj;
	CalComponent *obj;
  
	g_return_val_if_fail(conduit!=NULL,NULL);
	g_return_val_if_fail(remote!=NULL,NULL);

	LOG ("find_record_in_repository: remote=%s... ",
		print_remote (remote));

	// LOG ("requesting %ld", remote->ID);

	status = cal_client_get_uid_by_pilot_id (ctxt->client, remote->ID, &uid);

	if (status == CAL_CLIENT_GET_SUCCESS) {
		status = cal_client_get_object (ctxt->client, uid, &obj);
		if (status == CAL_CLIENT_GET_SUCCESS) {
			LOG ("found %s\n", print_ical (obj));
			loc = g_new0(GCalLocalRecord,1);
			/* memory allocated in new_from_string is freed in free_match */
			local_record_from_icalobject (loc, obj);
			return loc;
		}
	}

	// INFO ("Object did not exist");
	LOG ("not found\n");
	return NULL;
}


/* 
 * updates an given iCalObject in the repository
 */
static void
update_calendar_entry_in_repository(GnomePilotConduitStandardAbs *conduit,
				    CalComponent *obj,
				    GCalConduitContext *ctxt) 
{
	gboolean success;

	g_return_if_fail (conduit!=NULL);
	g_return_if_fail (obj!=NULL);

	LOG ("        update_calendar_entry_in_repository "
		"saving %s to desktop\n",
		print_ical (obj));

	success = cal_client_update_object (ctxt->client, obj);

	if (! success) {
		WARN (_("Error while communicating with calendar server"));
	}
}


static CalComponent *
ical_from_remote_record (GnomePilotConduitStandardAbs *conduit,
			 PilotRecord *remote,
			 CalComponent *in_obj)
{
	CalComponent *obj;
	struct ToDo todo;
	struct icaltimetype now = icaltime_from_timet (time (NULL), FALSE, FALSE);

	CalComponentText summary = {NULL, NULL};
	CalComponentText comment = {NULL, NULL};
	GSList *comment_list;

	g_return_val_if_fail(remote!=NULL,NULL);
	memset (&todo, 0, sizeof (struct ToDo));
	unpack_ToDo (&todo, remote->record, remote->length);

	LOG ("        ical_from_remote_record: "
		"merging remote %s into local %s\n",
		print_remote (remote), print_ical (in_obj));
	
	if (in_obj == NULL) {
		obj = cal_component_new ();
	} else {
		obj = in_obj;
	}

	summary.value = todo.description;
	cal_component_set_summary (obj, &summary);

	comment.value = todo.note;
	comment_list = g_slist_append (NULL, &comment);
	cal_component_set_comment_list (obj, comment_list);
	g_slist_free (comment_list);

	cal_component_set_new_vtype (obj, CAL_COMPONENT_TODO);
	// obj->new = TRUE;
	// obj->created = now;
	cal_component_set_created (obj, &now);
	// obj->last_mod = now;
	cal_component_set_last_modified (obj, &now);
	/* obj->priority = 0; */
	// obj->transp = 0;
	cal_component_set_transparency (obj,
					/*CalComponentTransparency transp*/
					CAL_COMPONENT_TRANSP_NONE);
	/* obj->related = NULL; */

	// cal_component_set_pilot_status (obj, ICAL_PILOT_SYNC_NONE);

	/*
	 * Begin and end
	 */

	// obj->dtend = mktime (& todo.due);	
	{
		/* do i need to malloc these?  FIX ME */
		struct icaltimetype dtend_ictt;
		CalComponentDateTime dtend;

		dtend_ictt = icaltime_from_timet (mktime (& todo.due), FALSE, FALSE);
		dtend.value = &dtend_ictt;
		dtend.tzid = NULL;
		cal_component_set_dtend (obj, &dtend);
	}


	/*
	if (todo.complete) {
		obj->completed = now-5;
		obj->percent = 100;
	}
	*/
	{
		cal_component_set_completed (obj, &now);
	}

	/*
	LOG ("[%s] from pilot, complete=%d/%ld\n",
		todo.description,
		todo.complete,
		obj->completed);
	*/


	//obj->priority = todo.priority; FIX ME

	/* g_free (obj->class); */

	/*
	if (remote->attr & dlpRecAttrSecret)
		obj->class = g_strdup ("PRIVATE");
	else
		obj->class = g_strdup ("PUBLIC");
	*/

	if (remote->attr & dlpRecAttrSecret)
		cal_component_set_classification (obj, CAL_COMPONENT_CLASS_PRIVATE);
	else
		cal_component_set_classification (obj, CAL_COMPONENT_CLASS_PUBLIC);

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
 * store a copy of a pilot record in the desktop database
 *
 */
static gint
update_record (GnomePilotConduitStandardAbs *conduit,
	       PilotRecord *remote,
	       GCalConduitContext *ctxt)
{
	//iCalObject *obj;
	CalComponent *obj;
	struct ToDo todo;
	CalClientGetStatus status;
	char *uid;

	CalComponentText summary = {NULL, NULL};
	CalComponentText comment = {NULL, NULL};
	GSList *comment_list;

	g_return_val_if_fail(remote!=NULL,-1);

	memset (&todo, 0, sizeof (struct ToDo));
	unpack_ToDo (&todo, remote->record, remote->length);

	LOG ("    cal_client_get_uid_by_pilot_id... ");

	status = cal_client_get_uid_by_pilot_id (ctxt->client,
						 remote->ID, &uid);
	if (status == CAL_CLIENT_GET_SUCCESS) {
		LOG (" succeeded with '%s'\n", uid);
		LOG ("    cal_client_get_object... ");
		status = cal_client_get_object (ctxt->client, uid, &obj);
	}

	if (status != CAL_CLIENT_GET_SUCCESS) {
		struct icaltimetype now = icaltime_from_timet (time (NULL), FALSE, FALSE);

		LOG ("failed, making a new one.\n");

		obj = cal_component_new ();

		summary.value = todo.description;
		cal_component_set_summary (obj, &summary);

		comment.value = todo.note;
		comment_list = g_slist_append (NULL, &comment);
		cal_component_set_comment_list (obj, comment_list);
		g_slist_free (comment_list);

		// obj->type = ICAL_TODO;
		cal_component_set_new_vtype (obj, CAL_COMPONENT_TODO);
		// obj->new = TRUE;
		// obj->created = now;
		cal_component_set_created (obj, &now);
		// obj->last_mod = now;
		cal_component_set_last_modified (obj, &now);
		// obj->priority = 0;
		// obj->transp = 0;
		cal_component_set_transparency (obj,
						/*CalComponentTransparency transp*/
						CAL_COMPONENT_TRANSP_NONE);
		// obj->related = NULL;
		//obj->pilot_id = remote->ID;
		//obj->pilot_status = ICAL_PILOT_SYNC_NONE;
		cal_component_set_pilot_id (obj, remote->ID);
		cal_component_set_pilot_status (obj, ICAL_PILOT_SYNC_NONE);
	} else {
		CalComponent *new_obj;

		LOG ("succeeded %s\n", print_ical (obj));

		new_obj = ical_from_remote_record (conduit, remote, obj);
		obj = new_obj;
	}

	/* update record on server */
	{
		const char *uid;
		unsigned long pilot_id;

		cal_component_get_uid (obj, &uid);
		cal_component_get_pilot_id (obj, &pilot_id);

		update_calendar_entry_in_repository (conduit, obj, ctxt);
		cal_client_update_pilot_id (ctxt->client, (char *) uid, pilot_id,
					    ICAL_PILOT_SYNC_NONE);
	}

	/*
	 * Shutdown
	 */
	//ical_object_unref (obj);
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

	/* If the local base is empty, do a slow sync */
	if (entry_number == 0) {
		GnomePilotConduitStandard *conduit;
		LOG ("    doing slow sync\n");
		conduit = GNOME_PILOT_CONDUIT_STANDARD (c);
		gnome_pilot_conduit_standard_set_slow (conduit);
	} else {
		LOG ("    doing fast sync\n");
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

	LOG ("---------------------------------------------------------\n");
	LOG ("pre_sync: ToDo Conduit v.%s", CONDUIT_VERSION);
	g_message ("ToDo Conduit v.%s", CONDUIT_VERSION);

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
		WARN(_("Could not read pilot's ToDo application block"));
		WARN("dlp_ReadAppBlock(...) = %d",l);
		gnome_pilot_conduit_error(GNOME_PILOT_CONDUIT(c),
			     _("Could not read pilot's ToDo application block"));
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
	LOG ("match_record: looking for local copy of %s\n",
		print_remote (remote));

	g_return_val_if_fail (local != NULL, -1);
	g_return_val_if_fail (remote != NULL, -1);

	*local = find_record_in_repository(conduit,remote,ctxt);

	if (*local == NULL)
		LOG ("    match_record: not found.\n");
	else
		LOG ("    match_record: found, %s\n", print_local (*local));
  
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
	LOG ("free_match: %s\n", print_local (*local));

	g_return_val_if_fail (local!=NULL, -1);
	g_return_val_if_fail (*local!=NULL, -1);

	// ical_object_unref (GCAL_LOCALRECORD(*local)->ical); 
	g_free (*local);

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
	LOG ("archive_local: doing nothing with %s\n", print_local (local));

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
	LOG ("archive_remote: doing nothing with %s\n",
		print_local (local));

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
	LOG ("store_remote: copying pilot record %s to desktop\n",
		print_remote (remote));

	g_return_val_if_fail(remote!=NULL,-1);
	remote->attr = GnomePilotRecordNothing;

	return update_record(conduit,remote,ctxt);
}

static gint
clear_status_archive_local (GnomePilotConduitStandardAbs *conduit,
			    GCalLocalRecord *local,
			    GCalConduitContext *ctxt)
{
	LOG ("clear_status_archive_local: doing nothing\n");

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
		// LOG ("beginning iteration");

		events = get_calendar_objects(conduit,NULL,ctxt);
		hest = 0;
		
		if(events!=NULL) {
			// LOG ("iterating over %d records", g_slist_length (events));
			*local = g_new0(GCalLocalRecord,1);

			local_record_from_ical_uid(*local,(gchar*)events->data,ctxt);
			iterator = events;
		} else {
			// LOG ("no events");
			(*local) = NULL;
		}
	} else {
		/* LOG ("continuing iteration\n"); */
		hest++;
		if(g_slist_next(iterator)==NULL) {
			GSList *l;

			// LOG ("ending");
			/** free stuff allocated for iteration */
			g_free((*local));

			// LOG ("iterated over %d records", hest);
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
	(*local) = NULL; /* ??? */

	/* debugging */
	{
		gchar *tmp;
		switch (flag) {
		case GnomePilotRecordNothing:
			tmp = g_strdup("RecordNothing"); break;
		case GnomePilotRecordModified:
			tmp = g_strdup("RecordModified"); break;
		case GnomePilotRecordDeleted:
			tmp = g_strdup("RecordDeleted"); break;
		case GnomePilotRecordNew:
			tmp = g_strdup("RecordNew"); break;
		default: tmp = g_strdup_printf("0x%x",flag); break;
		}
		LOG ("\niterate_specific: (flag = %s)... ", tmp);
		g_free(tmp);
	}

	g_return_val_if_fail(local!=NULL,-1);

	/* iterate until a record meets the criteria */
	while (gnome_pilot_conduit_standard_abs_iterate (conduit,
						      (LocalRecord**)local)) {
		if((*local)==NULL) break;
		if(archived && ((*local)->local.archived==archived)) break;
		if(((*local)->local.attr == flag)) break;
	}

	if ((*local)) {
		LOG (" found %s\n", print_local (*local));
	} else {
		LOG (" no more found.\n");
	}

	return (*local)==NULL?0:1;
}

static gint
purge (GnomePilotConduitStandardAbs *conduit,
       GCalConduitContext *ctxt)
{
	LOG ("purge: doing nothing\n");

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
	iCalPilotState new_state;

	LOG ("set_status: %s status is now '%s'\n",
		print_local (local),
		gnome_pilot_status_to_string (status));

	g_return_val_if_fail(local!=NULL,-1);
	g_assert(local->ical!=NULL);
	
	local->local.attr = status;
	switch(status) {
	case GnomePilotRecordPending:
	case GnomePilotRecordNothing:
		new_state = ICAL_PILOT_SYNC_NONE;
		break;
	case GnomePilotRecordDeleted:
		break;
	case GnomePilotRecordNew:
	case GnomePilotRecordModified:
		new_state = ICAL_PILOT_SYNC_MOD;
		break;	  
	}
	
	if (status == GnomePilotRecordDeleted) {
		const char *uid;
		cal_component_get_uid (local->ical, &uid);
		success = cal_client_remove_object (ctxt->client, uid);
	} else {
		const char *uid;
		unsigned long pilot_id;

		cal_component_get_uid (local->ical, &uid);
		cal_component_get_pilot_id (local->ical, &pilot_id);

		success = cal_client_update_object (ctxt->client, local->ical);
		cal_client_update_pilot_id (ctxt->client, (char *) uid,
					    pilot_id, new_state);
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
	LOG ("set_archived: %s archived flag is now '%d'\n",
		print_local (local), archived);

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
	const char *uid;
	unsigned long int pilot_status;

	LOG ("set_pilot_id: %s pilot ID is now '%d'\n",
		print_local (local), ID);

	g_return_val_if_fail(local!=NULL,-1);
	g_assert(local->ical!=NULL);

	local->local.ID = ID;
	cal_component_set_pilot_id (local->ical, ID);

	cal_component_get_uid (local->ical, &uid);
	cal_component_get_pilot_status (local->ical, &pilot_status);

	cal_client_update_pilot_id (ctxt->client,
				    (char *) uid,
				    local->local.ID,
				    pilot_status);

        return 0;
}

static gint
transmit (GnomePilotConduitStandardAbs *conduit,
	  GCalLocalRecord *local,
	  PilotRecord **remote,
	  GCalConduitContext *ctxt)
{
	PilotRecord *p;
	/* priority; FIX ME */
	struct icaltimetype *completed;
	CalComponentText summary;
	GSList *comment_list = NULL;
	CalComponentText *comment;

	LOG ("transmit: encoding local %s\n", print_local (local));

	g_return_val_if_fail(local!=NULL,-1);
	g_return_val_if_fail(remote!=NULL,-1);
	g_assert(local->ical!=NULL);

	p = g_new0(PilotRecord,1);

	p->ID = local->local.ID;
	p->attr = local->local.attr;
	p->archived = local->local.archived;
	p->secret = local->local.secret;

	local->todo = g_new0(struct ToDo,1);

	{
		CalComponentDateTime dtend;
		time_t dtend_time_t;

		cal_component_get_dtend (local->ical, &dtend);
		dtend_time_t = icaltime_as_timet (*dtend.value);

		local->todo->due = *localtime (&dtend_time_t);
		local->todo->indefinite = (dtend.value->year == 0);
	}

	//local->todo->priority = local->ical->priority;
	local->todo->priority = 1; /* FIX ME */

	cal_component_get_completed (local->ical, &completed);
	if (completed->year > 0)
		local->todo->complete = 1; /* FIX ME */

	/* STOP: don't replace these with g_strdup, since free_ToDo
	   uses free to deallocte */

	cal_component_get_summary (local->ical, &summary);
	local->todo->description = 
		//local->ical->summary == NULL ? NULL : strdup (summary.value);
		strdup ((char *) summary.value);


	cal_component_get_comment_list (local->ical, &comment_list);
	if (comment_list) {
		comment = (CalComponentText *) comment_list->data;
		if (comment && comment->value)
			local->todo->note = strdup (comment->value);
		else
			local->todo->note = NULL;
	} else {
		local->todo->note = NULL;
	}

	/*
	LOG ("transmitting todo to pilot [%s] complete=%d/%ld\n",
		local->ical->summary==NULL?"NULL":local->ical->summary,
		local->todo->complete, local->ical->completed);
	*/

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
	LOG ("free_transmit: freeing %s\n",
		print_local (local));

	g_return_val_if_fail(local!=NULL,-1);
	g_return_val_if_fail(remote!=NULL,-1);

	/* free_ToDo(local->todo); */ /* FIX ME is this needed? */
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

	LOG ("compare: local=%s remote=%s...\n",
		print_local (local), print_remote (remote));

	g_return_val_if_fail (local!=NULL,-1);
	g_return_val_if_fail (remote!=NULL,-1);

	err = transmit(conduit,local,&remoteOfLocal,ctxt);
	if (err != 0) return err;

	retval = 0;
	if (remote->length == remoteOfLocal->length) {
		if  (memcmp (remoteOfLocal->record,
			     remote->record, remote->length)!=0) {
			LOG ("    compare failed on contents\n");
			retval = 1;
		}
	} else {
		LOG("    compare failed on length\n");
		retval = 1;
	}


	if (retval == 0) {
		LOG ("    match.\n");
	} else {
		/* debug spew */
		LOG ("        local:%s\n", print_remote (remoteOfLocal));
		LOG ("        remote:%s\n", print_remote (remote));
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
	LOG ("compare_backup: doing nothing\n");

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

	LOG ("delete_all: deleting all objects from desktop\n");

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


static ORBit_MessageValidationResult
accept_all_cookies (CORBA_unsigned_long request_id,
		    CORBA_Principal *principal,
		    CORBA_char *operation)
{
	/* allow ALL cookies */
	return ORBIT_MESSAGE_ALLOW_ALL;
}


GnomePilotConduit *
conduit_get_gpilot_conduit (guint32 pilotId)
{
	GtkObject *retval;
	GCalConduitCfg *cfg;
	GCalConduitContext *ctxt;

	LOG ("in todo's conduit_get_gpilot_conduit\n");

	/* we need to find wombat with oaf, so make sure oaf
	   is initialized here.  once the desktop is converted
	   to oaf and gpilotd is built with oaf, this can go away */
	if (! oaf_is_initialized ())
	{
		char *argv[ 1 ] = {"hi"};
		oaf_init (1, argv);

		if (bonobo_init (CORBA_OBJECT_NIL,
				 CORBA_OBJECT_NIL,
				 CORBA_OBJECT_NIL) == FALSE)
			g_error (_("Could not initialize Bonobo"));

		ORBit_set_request_validation_handler (accept_all_cookies);
	}

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
