/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#include <config.h>
#include <sys/stat.h>
#include <utime.h>
#include <unistd.h>
#include <pwd.h>
#include <signal.h>
#include <errno.h>

#include <cal-client/cal-client.h>
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
#include <todo-conduit-config.h>
#include <todo-conduit.h>
#include <libical/src/libical/icaltypes.h>

#include <bonobo.h>

GnomePilotConduit * conduit_get_gpilot_conduit (guint32);
void conduit_destroy_gpilot_conduit (GnomePilotConduit*);
void local_record_from_compobject (GCalLocalRecord *local, CalComponent *comp);

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

	return cal_component_get_as_string (local->comp);
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

/* Given a GCalConduitContxt*, allocates the structure */
static void
e_todo_context_new (EToDoConduitContext **ctxt, guint32 pilotId) 
{
	*ctxt = g_new0 (EToDoConduitContext,1);
	g_assert (ctxt!=NULL);

	todoconduit_load_configuration (&(*ctxt)->cfg, pilotId);
}


/* Destroys any data allocated by gcalconduit_new_context
   and deallocates its data. */
static void
e_todo_context_destroy (EToDoConduitContext **ctxt)
{
	g_return_if_fail (ctxt!=NULL);
	g_return_if_fail (*ctxt!=NULL);

	if ((*ctxt)->client != NULL)
		gtk_object_unref (GTK_OBJECT ((*ctxt)->client));

	if ((*ctxt)->cfg != NULL)
		todoconduit_destroy_configuration (&(*ctxt)->cfg);

	g_free (*ctxt);
	*ctxt = NULL;
}


static void
gnome_calendar_load_cb (GtkWidget *cal_client,
			CalClientLoadStatus status,
			EToDoConduitContext *ctxt)
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
		       EToDoConduitContext *ctxt)
{
	
	g_return_val_if_fail(conduit!=NULL,-2);
	g_return_val_if_fail(ctxt!=NULL,-2);

	ctxt->client = cal_client_new ();

	/* FIX ME */
	ctxt->calendar_file = g_concat_dir_and_file (g_get_home_dir (),
			       "evolution/local/Calendar/calendar.ics");

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


static GSList * 
get_calendar_objects(GnomePilotConduitStandardAbs *conduit,
		     gboolean *status,
		     EToDoConduitContext *ctxt) 
{
	GList *uids;
	GSList *result = NULL;

	g_return_val_if_fail (conduit != NULL, NULL);
	g_return_val_if_fail (ctxt != NULL, NULL);

	uids = cal_client_get_uids (ctxt->client, CALOBJ_TYPE_TODO);

	LOG ("got %d todo entries from cal server\n", g_list_length (uids));

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
local_record_from_comp_uid (GCalLocalRecord *local,
			    char *uid,
			    EToDoConduitContext *ctxt)
{
	CalComponent *comp;
	CalClientGetStatus status;

	g_assert(local!=NULL);

	status = cal_client_get_object (ctxt->client, uid, &comp);

	if (status == CAL_CLIENT_GET_SUCCESS)
		local_record_from_compobject (local, comp);
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
 * converts a CalComponent object to a GCalLocalRecord
 */
void
local_record_from_compobject(GCalLocalRecord *local,
			     CalComponent *comp) 
{
	unsigned long *pilot_id;
	unsigned long *pilot_status;
	CalComponentClassification classif;
	
	g_return_if_fail (local!=NULL);
	g_return_if_fail (comp!=NULL);

	local->comp = comp;
	local->todo = NULL; /* ??? */
	cal_component_get_pilot_id (comp, &pilot_id);
	cal_component_get_pilot_status (comp, &pilot_status);


	/* Records without a pilot_id are new */
	if (!pilot_id) {
		local->local.attr = GnomePilotRecordNew;
	} else {
		local->local.ID = *pilot_id;
		local->local.attr = *pilot_status;
	}

	cal_component_get_classification (comp, &classif);

	if (classif == CAL_COMPONENT_CLASS_PRIVATE)
		local->local.secret = 1;
	else
		local->local.secret = 0;

	local->local.archived = 0;  
}


/*
 * Given a PilotRecord, find the matching record in
 * the calendar repository. If no match, return NULL
 */
static GCalLocalRecord *
find_record_in_repository(GnomePilotConduitStandardAbs *conduit,
			  PilotRecord *remote,
			  EToDoConduitContext *ctxt) 
{
	char *uid = NULL;
	GCalLocalRecord *loc;
	CalClientGetStatus status;
	CalComponent *obj;
  
	g_return_val_if_fail(conduit!=NULL,NULL);
	g_return_val_if_fail(remote!=NULL,NULL);

	LOG ("find_record_in_repository: remote=%s... ",
		print_remote (remote));

	LOG ("requesting %ld", remote->ID);

	status = cal_client_get_uid_by_pilot_id (ctxt->client, remote->ID, &uid);

	if (status == CAL_CLIENT_GET_SUCCESS) {
		status = cal_client_get_object (ctxt->client, uid, &obj);
		if (status == CAL_CLIENT_GET_SUCCESS) {
			LOG ("found %s\n", cal_component_get_as_string (obj));
			loc = g_new0(GCalLocalRecord,1);
			/* memory allocated in new_from_string is freed in free_match */
			local_record_from_compobject (loc, obj);
			return loc;
		}
	}

	INFO ("Object did not exist");
	LOG ("not found\n");
	return NULL;
}


/* 
 * updates an given CalComponent in the repository
 */
static void
update_calendar_entry_in_repository(GnomePilotConduitStandardAbs *conduit,
				    CalComponent *comp,
				    EToDoConduitContext *ctxt) 
{
	gboolean success;

	g_return_if_fail (conduit != NULL);
	g_return_if_fail (comp != NULL);

	LOG ("        update_calendar_entry_in_repository "
		"saving to desktop\n%s", cal_component_get_as_string (comp));

	success = cal_client_update_object (ctxt->client, comp);

	if (!success)
		WARN (_("Error while communicating with calendar server"));
}


static CalComponent *
comp_from_remote_record (GnomePilotConduitStandardAbs *conduit,
			 PilotRecord *remote,
			 CalComponent *in_comp)
{
	CalComponent *comp;
	struct ToDo todo;
	struct icaltimetype now = icaltime_from_timet (time (NULL), FALSE, FALSE);
	unsigned long pilot_status = GnomePilotRecordNothing;
	CalComponentText summary = {NULL, NULL};
	CalComponentText comment = {NULL, NULL};
	CalComponentDateTime dt = {NULL, NULL};
	struct icaltimetype due;
 	GSList *comment_list;

	g_return_val_if_fail (remote != NULL, NULL);

	memset (&todo, 0, sizeof (struct ToDo));
	unpack_ToDo (&todo, remote->record, remote->length);

	if (in_comp == NULL) {
		comp = cal_component_new ();
		cal_component_set_new_vtype (comp, CAL_COMPONENT_TODO);
		cal_component_set_created (comp, &now);
	} else {
		comp = cal_component_clone (in_comp);
	}

  	LOG ("        comp_from_remote_record: " 
  	     "merging remote %s into local %s\n", 
  	     print_remote (remote), cal_component_get_as_string (comp));

	cal_component_set_last_modified (comp, &now);

	summary.value = todo.description;
	cal_component_set_summary (comp, &summary);

	comment.value = todo.note;
	comment_list = g_slist_append (NULL, &comment);
	cal_component_set_comment_list (comp, comment_list);
	g_slist_free (comment_list);

	if (todo.complete) {
		int percent = 100;
		cal_component_set_completed (comp, &now);
		cal_component_set_percent (comp, &percent);
	}

	due = icaltime_from_timet (mktime (& todo.due), FALSE, FALSE);
	dt.value = &due;
	cal_component_set_due (comp, &dt);

	cal_component_set_priority (comp, &todo.priority);
	cal_component_set_transparency (comp, CAL_COMPONENT_TRANSP_NONE);

	if (remote->attr & dlpRecAttrSecret)
		cal_component_set_classification (comp, CAL_COMPONENT_CLASS_PRIVATE);
	else
		cal_component_set_classification (comp, CAL_COMPONENT_CLASS_PUBLIC);

	cal_component_set_pilot_id (comp, &remote->ID);
	cal_component_set_pilot_status (comp, &pilot_status);

	cal_component_commit_sequence (comp);
	
	free_ToDo(&todo);

	return comp;
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
	       EToDoConduitContext *ctxt)
{
	CalComponent *comp;
	CalClientGetStatus status;
	struct ToDo todo;
	unsigned long pilot_status = GnomePilotRecordNothing;
	char *uid;

	g_return_val_if_fail (remote!=NULL,-1);

	memset (&todo, 0, sizeof (struct ToDo));
	unpack_ToDo (&todo, remote->record, remote->length);

	LOG ("    cal_client_get_uid_by_pilot_id... ");

	status = cal_client_get_uid_by_pilot_id (ctxt->client,
						 remote->ID, &uid);
	if (status == CAL_CLIENT_GET_SUCCESS) {
		LOG (" succeeded with '%s'\n", uid);
		LOG ("    cal_client_get_object... ");
		status = cal_client_get_object (ctxt->client, uid, &comp);
	}

	if (status != CAL_CLIENT_GET_SUCCESS) {
		comp = comp_from_remote_record (conduit, remote, NULL);
	} else {
		CalComponent *new_comp;

		LOG ("succeeded %s\n", cal_component_get_as_string (comp));

		new_comp = comp_from_remote_record (conduit, remote, comp);
		gtk_object_unref (GTK_OBJECT (comp));
		comp = new_comp;
	}

	/* update record on server */
	{
		const char *uid;
		unsigned long *pilot_id;

		cal_component_get_uid (comp, &uid);
		cal_component_get_pilot_id (comp, &pilot_id);
		
		update_calendar_entry_in_repository (conduit, comp, ctxt);
		cal_client_update_pilot_id (ctxt->client, (char *) uid, 
					    *pilot_id, pilot_status);
	}

	/*
	 * Shutdown
	 */
	gtk_object_unref (GTK_OBJECT (comp));
	free_ToDo(&todo);

	return 0;
}

static void
check_for_slow_setting (GnomePilotConduit *c, EToDoConduitContext *ctxt)
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
	  EToDoConduitContext *ctxt)
{
	int l;
	unsigned char *buf;
	GnomePilotConduitStandardAbs *conduit;
	gint num_records;

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


	num_records = cal_client_get_n_objects (ctxt->client, CALOBJ_TYPE_TODO);
	gnome_pilot_conduit_standard_abs_set_num_local_records(GNOME_PILOT_CONDUIT_STANDARD_ABS(c), num_records);

	/* FIX ME How are we going to fill in these fields? */
	num_records = 0;
	gnome_pilot_conduit_standard_abs_set_num_updated_local_records(GNOME_PILOT_CONDUIT_STANDARD_ABS(c), num_records);
	gnome_pilot_conduit_standard_abs_set_num_new_local_records(GNOME_PILOT_CONDUIT_STANDARD_ABS(c), num_records);
	gnome_pilot_conduit_standard_abs_set_num_deleted_local_records(GNOME_PILOT_CONDUIT_STANDARD_ABS(c), num_records);

	gtk_object_set_data (GTK_OBJECT(c), "dbinfo", dbi);
  
	/* load_records(c); */

	buf = (unsigned char*)g_malloc (0xffff);
	l = dlp_ReadAppBlock (dbi->pilot_socket, dbi->db_handle, 0,
			      (unsigned char *)buf, 0xffff);
	
	if (l < 0) {
		WARN(_("Could not read pilot's ToDo application block"));
		WARN("dlp_ReadAppBlock(...) = %d",l);
		gnome_pilot_conduit_error(GNOME_PILOT_CONDUIT(c),
			     _("Could not read pilot's ToDo application block"));
		return -1;
	}
	unpack_ToDoAppInfo (&(ctxt->ai), buf, l);
	g_free (buf);

	check_for_slow_setting (c, ctxt);

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
		 EToDoConduitContext *ctxt)
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
		 EToDoConduitContext *ctxt)
{
	LOG ("free_match: %s\n", print_local (*local));

	g_return_val_if_fail (local != NULL, -1);
	g_return_val_if_fail (*local != NULL, -1);

	gtk_object_unref (GTK_OBJECT ((*local)->comp));
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
	       EToDoConduitContext *ctxt)
{
	LOG ("archive_local: doing nothing with %s\n", print_local (local));

	g_return_val_if_fail (local != NULL, -1);

	return -1;
}

/*
  Store in archive and set status to Nothing
 */
static gint
archive_remote (GnomePilotConduitStandardAbs *conduit,
		GCalLocalRecord *local,
		PilotRecord *remote,
		EToDoConduitContext *ctxt)
{
	LOG ("archive_remote: doing nothing with %s\n",
		print_local (local));

/*          g_return_val_if_fail(remote!=NULL,-1); */
/*  	g_return_val_if_fail(local!=NULL,-1); */

	return -1;
}

/*
  Store and set status to Nothing
 */
static gint
store_remote (GnomePilotConduitStandardAbs *conduit,
	      PilotRecord *remote,
	      EToDoConduitContext *ctxt)
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
			    EToDoConduitContext *ctxt)
{
	LOG ("clear_status_archive_local: doing nothing\n");

	g_return_val_if_fail(local!=NULL,-1);

        return -1;
}

static gint
iterate (GnomePilotConduitStandardAbs *conduit,
	 GCalLocalRecord **local,
	 EToDoConduitContext *ctxt)
{
	static GSList *events,*iterator;
	static int hest;

	g_return_val_if_fail(local!=NULL,-1);

	if(*local==NULL) {
		/*   LOG ("beginning iteration"); */

		events = get_calendar_objects(conduit,NULL,ctxt);
		hest = 0;
		
		if(events!=NULL) {
			 /*  LOG ("iterating over %d records", g_slist_length (events)); */
			*local = g_new0(GCalLocalRecord,1);

			local_record_from_comp_uid(*local,(gchar*)events->data,ctxt);
			iterator = events;
		} else {
			/*  LOG ("no events"); */
			(*local) = NULL;
		}
	} else {
		/* LOG ("continuing iteration\n"); */
		hest++;
		if(g_slist_next(iterator)==NULL) {
			GSList *l;

			/*  LOG ("ending"); */
			/** free stuff allocated for iteration */
			g_free((*local));

			/*  LOG ("iterated over %d records", hest) */;
			for(l=events;l;l=l->next)
				g_free(l->data);

			g_slist_free(events);

			/* ends iteration */
			(*local) = NULL;
			return 0;
		} else {
			iterator = g_slist_next(iterator);
			local_record_from_comp_uid(*local,(gchar*)(iterator->data),ctxt);
		}
	}
	return 1;
}


static gint
iterate_specific (GnomePilotConduitStandardAbs *conduit,
		  GCalLocalRecord **local,
		  gint flag,
		  gint archived,
		  EToDoConduitContext *ctxt)
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
       EToDoConduitContext *ctxt)
{
	LOG ("purge: doing nothing\n");

	/* HEST, gem posterne her */

	return -1;
}


static gint
set_status (GnomePilotConduitStandardAbs *conduit,
	    GCalLocalRecord *local,
	    gint status,
	    EToDoConduitContext *ctxt)
{
	gboolean success;

	LOG ("set_status: %s status is now '%s'\n",
		print_local (local),
		gnome_pilot_status_to_string (status));

	g_return_val_if_fail(local!=NULL,-1);
	g_assert(local->comp!=NULL);
	
	local->local.attr = status;
	if (status == GnomePilotRecordDeleted) {
		const char *uid;
		cal_component_get_uid (local->comp, &uid);
		success = cal_client_remove_object (ctxt->client, uid);
	} else {
		const char *uid;
		unsigned long *pilot_id;

		cal_component_get_uid (local->comp, &uid);
		cal_component_get_pilot_id (local->comp, &pilot_id);

		success = cal_client_update_object (ctxt->client, local->comp);
		cal_client_update_pilot_id (ctxt->client, (char *) uid,
					    *pilot_id, status);
	}

	if (!success) {
		WARN (_("Error while communicating with calendar server"));
	}
	
        return 0;
}

static gint
set_archived (GnomePilotConduitStandardAbs *conduit,
	      GCalLocalRecord *local,
	      gint archived,
	      EToDoConduitContext *ctxt)
{
	LOG ("set_archived: %s archived flag is now '%d'\n",
		print_local (local), archived);

	g_return_val_if_fail(local!=NULL,-1);
	g_assert(local->comp!=NULL);

	local->local.archived = archived;
	update_calendar_entry_in_repository(conduit,local->comp,ctxt);
	/* FIXME: This should move the entry into a speciel
	   calendar file, eg. Archive, or (by config option), simply
	   delete it */
        return 0;
}

static gint
set_pilot_id (GnomePilotConduitStandardAbs *conduit,
	      GCalLocalRecord *local,
	      guint32 ID,
	      EToDoConduitContext *ctxt)
{
	const char *uid;
	unsigned long *pilot_status;

	LOG ("set_pilot_id: %s pilot ID is now '%d'\n",
		print_local (local), ID);

	g_return_val_if_fail(local!=NULL,-1);
	g_assert(local->comp!=NULL);

	local->local.ID = ID;
	cal_component_set_pilot_id (local->comp, (unsigned long *)&ID);

	cal_component_get_uid (local->comp, &uid);
	cal_component_get_pilot_status (local->comp, &pilot_status);

	cal_client_update_pilot_id (ctxt->client,
				    (char *) uid,
				    local->local.ID,
				    *pilot_status);

        return 0;
}

static gint
transmit (GnomePilotConduitStandardAbs *conduit,
	  GCalLocalRecord *local,
	  PilotRecord **remote,
	  EToDoConduitContext *ctxt)
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
	g_assert(local->comp!=NULL);

	p = g_new0(PilotRecord,1);

	p->ID = local->local.ID;
	p->attr = local->local.attr;
	p->archived = local->local.archived;
	p->secret = local->local.secret;

	local->todo = g_new0(struct ToDo,1);

	{
		CalComponentDateTime dtend;
		time_t dtend_time_t;

		cal_component_get_dtend (local->comp, &dtend);
		dtend_time_t = icaltime_as_timet (*dtend.value);

		local->todo->due = *localtime (&dtend_time_t);
		local->todo->indefinite = (dtend.value->year == 0);
	}

/*  	local->todo->priority = local->comp->priority; */
/*  	local->todo->priority = 1; */ /* FIX ME */

	cal_component_get_completed (local->comp, &completed);
	if (completed->year > 0)
		local->todo->complete = 1; /* FIX ME */

	/* STOP: don't replace these with g_strdup, since free_ToDo
	   uses free to deallocte */

	cal_component_get_summary (local->comp, &summary);
	local->todo->description = 
		/*  local->comp->summary == NULL ? NULL : strdup (summary.value); */
		strdup ((char *) summary.value);


	cal_component_get_comment_list (local->comp, &comment_list);
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
		local->comp->summary==NULL?"NULL":local->comp->summary,
		local->todo->complete, local->comp->completed);
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
	       EToDoConduitContext *ctxt)
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
	    EToDoConduitContext *ctxt)
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
		EToDoConduitContext *ctxt)
{
	LOG ("compare_backup: doing nothing\n");

	g_return_val_if_fail(local!=NULL,-1);
	g_return_val_if_fail(remote!=NULL,-1);

        return -1;
}


static gint
delete_all (GnomePilotConduitStandardAbs *conduit,
	    EToDoConduitContext *ctxt)
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
        return 0;
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
	EToDoConduitContext *ctxt;

	LOG ("in todo's conduit_get_gpilot_conduit\n");

	/* we need to find wombat with oaf, so make sure oaf
	   is initialized here.  once the desktop is converted
	   to oaf and gpilotd is built with oaf, this can go away */
	if (!oaf_is_initialized ()) {
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

	gnome_pilot_conduit_construct (GNOME_PILOT_CONDUIT (retval),
				       "ToDoConduit");

	e_todo_context_new (&ctxt, pilotId);
	gtk_object_set_data (GTK_OBJECT (retval), "todoconduit_context", ctxt);

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
	EToDoConduitContext *ctxt;

	ctxt = gtk_object_get_data (GTK_OBJECT (conduit), 
				    "todoconduit_context");

	e_todo_context_destroy (&ctxt);

	gtk_object_destroy (GTK_OBJECT (conduit));
}
