/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Evolution calendar - ToDo Conduit
 *
 * Copyright (C) 1998 Free Software Foundation
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Authors: Eskil Heyn Olsen <deity@eskil.dk> 
 *          JP Rosevear <jpr@helixcode.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <sys/stat.h>
#include <utime.h>
#include <unistd.h>
#include <pwd.h>
#include <signal.h>
#include <errno.h>

#include <liboaf/liboaf.h>
#include <bonobo.h>
#include <gnome-xml/parser.h>
#include <cal-client/cal-client.h>
#include <cal-util/timeutil.h>
#include <pi-source.h>
#include <pi-socket.h>
#include <pi-file.h>
#include <pi-dlp.h>
#include <pi-version.h>
#include <libical/src/libical/icaltypes.h>

#define TODO_CONFIG_LOAD 1
#define TODO_CONFIG_DESTROY 1
#include <todo-conduit-config.h>
#undef TODO_CONFIG_LOAD
#undef TODO_CONFIG_DESTROY

#include <todo-conduit.h>

GnomePilotConduit * conduit_get_gpilot_conduit (guint32);
void conduit_destroy_gpilot_conduit (GnomePilotConduit*);

#define CONDUIT_VERSION "0.1.0"
#ifdef G_LOG_DOMAIN
#undef G_LOG_DOMAIN
#endif
#define G_LOG_DOMAIN "etodoconduit"

#define DEBUG_CALCONDUIT 1
/* #undef DEBUG_CALCONDUIT */

#ifdef DEBUG_CALCONDUIT
#define LOG(e...) g_log (G_LOG_DOMAIN, G_LOG_LEVEL_MESSAGE, e)
#else
#define LOG(e...)
#endif 

#define WARN(e...) g_log (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, e)
#define INFO(e...) g_log (G_LOG_DOMAIN, G_LOG_LEVEL_MESSAGE, e)

/* debug spew DELETE ME */
static char *
print_local (EToDoLocalRecord *local)
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

	return "";
}


/* debug spew DELETE ME */
static char *print_remote (GnomePilotRecord *remote)
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

/* Context Routines */
static void
e_todo_context_new (EToDoConduitContext **ctxt, guint32 pilot_id) 
{
	*ctxt = g_new0 (EToDoConduitContext,1);
	g_assert (ctxt!=NULL);

	todoconduit_load_configuration (&(*ctxt)->cfg, pilot_id);
}

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

/* Calendar Server routines */
static void
start_calendar_server_cb (GtkWidget *cal_client,
			  CalClientLoadStatus status,
			  EToDoConduitContext *ctxt)
{
	CalClient *client = CAL_CLIENT (cal_client);

	LOG ("  entering start_calendar_server_load_cb, tried=%d\n",
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
		ctxt->calendar_load_tried = TRUE;
	}
}

static int
start_calendar_server (EToDoConduitContext *ctxt)
{
	
	g_return_val_if_fail (ctxt != NULL, -2);

	ctxt->client = cal_client_new ();

	/* FIX ME */
	ctxt->calendar_file = g_concat_dir_and_file (g_get_home_dir (),
			       "evolution/local/Calendar/calendar.ics");

	gtk_signal_connect (GTK_OBJECT (ctxt->client), "cal_loaded",
			    start_calendar_server_cb, ctxt);

	LOG ("    calling cal_client_load_calendar\n");
	cal_client_load_calendar (ctxt->client, ctxt->calendar_file);

	/* run a sub event loop to turn cal-client's async load
	   notification into a synchronous call */
	gtk_main ();

	if (ctxt->calendar_load_success)
		return 0;

	return -1;
}

/* Utility routines */
static char *
map_name (EToDoConduitContext *ctxt) 
{
	char *filename;
	
	filename = g_strdup_printf ("%s/evolution/local/Calendar/pilot-map-%d.xml", g_get_home_dir (), ctxt->cfg->pilot_id);

	return filename;
}

static GList *
next_changed_item (EToDoConduitContext *ctxt, GList *changes) 
{
	CalObjChange *coc;
	GList *l;
	
	for (l = changes; l != NULL; l = l->next) {
		coc = l->data;
		
		if (g_hash_table_lookup (ctxt->changed_hash, coc->uid))
			return l;
	}
	
	return NULL;
}

static void
compute_status (EToDoConduitContext *ctxt, EToDoLocalRecord *local, const char *uid)
{
	CalObjChange *coc;

	local->local.archived = FALSE;
	local->local.secret = FALSE;

	coc = g_hash_table_lookup (ctxt->changed_hash, uid);
	
	if (coc == NULL) {
		local->local.attr = GnomePilotRecordNothing;
		return;
	}
	
	switch (coc->type) {
	case CALOBJ_UPDATED:
		if (e_pilot_map_lookup_pid (ctxt->map, coc->uid) > 0)
			local->local.attr = GnomePilotRecordModified;
		else
			local->local.attr = GnomePilotRecordNew;
		break;
		
	case CALOBJ_REMOVED:
		local->local.attr = GnomePilotRecordDeleted;
		break;
	}
}

static GnomePilotRecord *
local_record_to_pilot_record (EToDoLocalRecord *local,
			      EToDoConduitContext *ctxt)
{
	GnomePilotRecord *p = NULL;
	
	g_return_val_if_fail (local != NULL, NULL);
	g_assert (local->comp != NULL);
	g_assert (local->todo != NULL );
	
	LOG ("local_record_to_pilot_record\n");

	p = g_new0 (GnomePilotRecord, 1);

	p->ID = local->local.ID;
	p->category = 0;
	p->attr = local->local.attr;
	p->archived = local->local.archived;
	p->secret = local->local.secret;

	/* Generate pilot record structure */
	p->record = g_new0 (char,0xffff);
	p->length = pack_ToDo (local->todo, p->record, 0xffff);

	return p;	
}

/*
 * converts a CalComponent object to a EToDoLocalRecord
 */
static void
local_record_from_comp (EToDoLocalRecord *local, CalComponent *comp, EToDoConduitContext *ctxt) 
{
	const char *uid;
	int *priority;
	struct icaltimetype *completed;
	CalComponentText summary;
	GSList *d_list = NULL;
	CalComponentText *description;
	CalComponentDateTime due;
	time_t due_time;
	CalComponentClassification classif;

	LOG ("local_record_from_comp\n");

	g_return_if_fail (local != NULL);
	g_return_if_fail (comp != NULL);

	local->comp = comp;

	cal_component_get_uid (local->comp, &uid);
	local->local.ID = e_pilot_map_lookup_pid (ctxt->map, uid);

	compute_status (ctxt, local, uid);

	local->todo = g_new0 (struct ToDo,1);

	/* STOP: don't replace these with g_strdup, since free_ToDo
	   uses free to deallocate */
	cal_component_get_summary (comp, &summary);
	if (summary.value) 
		local->todo->description = strdup ((char *) summary.value);

	cal_component_get_description_list (comp, &d_list);
	if (d_list) {
		description = (CalComponentText *) d_list->data;
		if (description && description->value)
			local->todo->note = strdup (description->value);
		else
			local->todo->note = NULL;
	} else {
		local->todo->note = NULL;
	}

	cal_component_get_due (comp, &due);	
	if (due.value) {
		due_time = icaltime_as_timet (*due.value);
		
		local->todo->due = *localtime (&due_time);
		local->todo->indefinite = 0;
	} else {
		local->todo->indefinite = 1;
	}

	cal_component_get_completed (comp, &completed);
	if (completed) {
		local->todo->complete = 1;
		cal_component_free_icaltimetype (completed);
	}	

	cal_component_get_priority (comp, &priority);
	if (priority) {
		local->todo->priority = *priority;
		cal_component_free_priority (priority);
	}
	
	cal_component_get_classification (comp, &classif);

	if (classif == CAL_COMPONENT_CLASS_PRIVATE)
		local->local.secret = 1;
	else
		local->local.secret = 0;

	local->local.archived = 0;
}

static void 
local_record_from_uid (EToDoLocalRecord *local,
		       const char *uid,
		       EToDoConduitContext *ctxt)
{
	CalComponent *comp;
	CalClientGetStatus status;

	g_assert(local!=NULL);

	status = cal_client_get_object (ctxt->client, uid, &comp);

	if (status == CAL_CLIENT_GET_SUCCESS) {
		local_record_from_comp (local, comp, ctxt);
	} else if (status == CAL_CLIENT_GET_NOT_FOUND) {
		comp = cal_component_new ();
		cal_component_set_new_vtype (comp, CAL_COMPONENT_TODO);
		cal_component_set_uid (comp, uid);
		local_record_from_comp (local, comp, ctxt);
	} else {
		INFO ("Object did not exist");
	}	
}


static CalComponent *
comp_from_remote_record (GnomePilotConduitSyncAbs *conduit,
			 GnomePilotRecord *remote,
			 CalComponent *in_comp)
{
	CalComponent *comp;
	struct ToDo todo;
	struct icaltimetype now = icaltime_from_timet (time (NULL), FALSE, FALSE);
	CalComponentText summary = {NULL, NULL};
	CalComponentText description = {NULL, NULL};
	CalComponentDateTime dt = {NULL, NULL};
	struct icaltimetype due;
 	GSList *d_list;

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
 	     "creating from remote %s and comp %s\n", 
  	     print_remote (remote), cal_component_get_as_string (comp));

	cal_component_set_last_modified (comp, &now);

	summary.value = todo.description;
	cal_component_set_summary (comp, &summary);

	description.value = todo.note;
	d_list = g_slist_append (NULL, &description);
	cal_component_set_comment_list (comp, d_list);
	g_slist_free (d_list);

	if (todo.complete) {
		int percent = 100;
		cal_component_set_completed (comp, &now);
		cal_component_set_percent (comp, &percent);
	}

	/* FIX ME This is a bit hackish, how else can we tell if there is
	 * no due date set?
	 */
	if (todo.due.tm_sec || todo.due.tm_min || todo.due.tm_hour 
	    || todo.due.tm_mday || todo.due.tm_mon || todo.due.tm_year) {
		due = icaltime_from_timet (mktime (&todo.due), FALSE, FALSE);
		dt.value = &due;
		cal_component_set_due (comp, &dt);
	}
	
	cal_component_set_priority (comp, &todo.priority);
	cal_component_set_transparency (comp, CAL_COMPONENT_TRANSP_NONE);

	if (remote->attr & dlpRecAttrSecret)
		cal_component_set_classification (comp, CAL_COMPONENT_CLASS_PRIVATE);
	else
		cal_component_set_classification (comp, CAL_COMPONENT_CLASS_PUBLIC);

	cal_component_commit_sequence (comp);
	
	free_ToDo(&todo);

	return comp;
}

static void
update_comp (GnomePilotConduitSyncAbs *conduit, CalComponent *comp,
	     EToDoConduitContext *ctxt) 
{
	gboolean success;

	g_return_if_fail (conduit != NULL);
	g_return_if_fail (comp != NULL);

	LOG ("update_comp: saving to desktop\n%s\n", 
	     cal_component_get_as_string (comp));

	success = cal_client_update_object (ctxt->client, comp);

	if (!success)
		WARN (_("Error while communicating with calendar server"));
}

static void
check_for_slow_setting (GnomePilotConduit *c, EToDoConduitContext *ctxt)
{
	int count, map_count;

	count = g_list_length (ctxt->uids);
	map_count = g_hash_table_size (ctxt->map->pid_map);
	
	/* If there are no objects or objects but no log */
	if (map_count == 0) {
		GnomePilotConduitStandard *conduit;
		LOG ("    doing slow sync\n");
		conduit = GNOME_PILOT_CONDUIT_STANDARD (c);
		gnome_pilot_conduit_standard_set_slow (conduit);
	} else {
		LOG ("    doing fast sync\n");
	}
}

/* Pilot syncing callbacks */
static gint
pre_sync (GnomePilotConduit *conduit,
	  GnomePilotDBInfo *dbi,
	  EToDoConduitContext *ctxt)
{
	GnomePilotConduitSyncAbs *abs_conduit;
	GList *l;
	int len;
	unsigned char *buf;
	char *filename;
	gint num_records, add_records = 0, mod_records = 0, del_records = 0;

	abs_conduit = GNOME_PILOT_CONDUIT_SYNC_ABS (conduit);

	LOG ("---------------------------------------------------------\n");
	LOG ("pre_sync: ToDo Conduit v.%s", CONDUIT_VERSION);
	g_message ("ToDo Conduit v.%s", CONDUIT_VERSION);

	ctxt->client = NULL;
	
	if (start_calendar_server (ctxt) != 0) {
		WARN(_("Could not start wombat server"));
		gnome_pilot_conduit_error (conduit, _("Could not start wombat"));
		return -1;
	}

	/* Get the local database */
	ctxt->uids = cal_client_get_uids (ctxt->client, CALOBJ_TYPE_TODO);
	
	/* Load the uid <--> pilot id map */
	filename = map_name (ctxt);
	e_pilot_map_read (filename, &ctxt->map);
	g_free (filename);

	/* Count and hash the changes */
	ctxt->changed_hash = g_hash_table_new (g_str_hash, g_str_equal);
	ctxt->changed = cal_client_get_changed_uids (ctxt->client, 
						     CALOBJ_TYPE_TODO,
						     ctxt->map->since + 1);

	for (l = ctxt->changed; l != NULL; l = l->next) {
		CalObjChange *coc = l->data;

		if (!e_pilot_map_uid_is_archived (ctxt->map, coc->uid)) {
			
			g_hash_table_insert (ctxt->changed_hash, coc->uid, coc);
			
			switch (coc->type) {
			case CALOBJ_UPDATED:
				if (e_pilot_map_lookup_pid (ctxt->map, coc->uid) > 0)
					mod_records++;
				else
					add_records++;
				break;
				
			case CALOBJ_REMOVED:
				del_records++;
				break;
			}
		}
	}

	/* Set the count information */
	num_records = cal_client_get_n_objects (ctxt->client, CALOBJ_TYPE_TODO);
	gnome_pilot_conduit_sync_abs_set_num_local_records(abs_conduit, num_records);
	gnome_pilot_conduit_sync_abs_set_num_new_local_records (abs_conduit, add_records);
	gnome_pilot_conduit_sync_abs_set_num_updated_local_records (abs_conduit, mod_records);
	gnome_pilot_conduit_sync_abs_set_num_deleted_local_records(abs_conduit, del_records);

	gtk_object_set_data (GTK_OBJECT (conduit), "dbinfo", dbi);

	buf = (unsigned char*)g_malloc (0xffff);
	len = dlp_ReadAppBlock (dbi->pilot_socket, dbi->db_handle, 0,
			      (unsigned char *)buf, 0xffff);
	
	if (len < 0) {
		WARN (_("Could not read pilot's ToDo application block"));
		WARN ("dlp_ReadAppBlock(...) = %d", len);
		gnome_pilot_conduit_error (conduit,
					   _("Could not read pilot's ToDo application block"));
		return -1;
	}
	unpack_ToDoAppInfo (&(ctxt->ai), buf, len);
	g_free (buf);

	check_for_slow_setting (conduit, ctxt);

	return 0;
}

static gint
post_sync (GnomePilotConduit *conduit,
	   GnomePilotDBInfo *dbi,
	   EToDoConduitContext *ctxt)
{
	gchar *filename;
	
	LOG ("post_sync: ToDo Conduit v.%s", CONDUIT_VERSION);
	LOG ("---------------------------------------------------------\n");

	filename = map_name (ctxt);
	e_pilot_map_write (filename, ctxt->map);
	e_pilot_map_destroy (ctxt->map);
	g_free (filename);
	
	return 0;
}

static gint
set_pilot_id (GnomePilotConduitSyncAbs *conduit,
	      EToDoLocalRecord *local,
	      guint32 ID,
	      EToDoConduitContext *ctxt)
{
	const char *uid;

	LOG ("set_pilot_id: setting to %d\n", ID);
	
	cal_component_get_uid (local->comp, &uid);
	e_pilot_map_insert (ctxt->map, ID, uid, FALSE);

        return 0;
}

static gint
set_status_cleared (GnomePilotConduitSyncAbs *conduit,
		    EToDoLocalRecord *local,
		    EToDoConduitContext *ctxt)
{
	const char *uid;
	
	LOG ("set_status_cleared: clearing status\n");
	
	cal_component_get_uid (local->comp, &uid);
	g_hash_table_remove (ctxt->changed_hash, uid);
	
        return 0;
}

static gint
for_each (GnomePilotConduitSyncAbs *conduit,
	  EToDoLocalRecord **local,
	  EToDoConduitContext *ctxt)
{
	static GList *uids, *iterator;
	static int count;

	g_return_val_if_fail (local != NULL, -1);

	if (*local == NULL) {
		LOG ("beginning for_each");

		uids = ctxt->uids;
		count = 0;
		
		if (uids != NULL) {
			LOG ("iterating over %d records", g_list_length (uids));

			*local = g_new0 (EToDoLocalRecord, 1);
			local_record_from_uid (*local, uids->data, ctxt);

			iterator = uids;
		} else {
			LOG ("no events");
			(*local) = NULL;
			return 0;
		}
	} else {
		count++;
		if (g_list_next (iterator)) {
			iterator = g_list_next (iterator);

			*local = g_new0 (EToDoLocalRecord, 1);
			local_record_from_uid (*local, iterator->data, ctxt);
		} else {
			LOG ("for_each ending");

			/* Tell the pilot the iteration is over */
			*local = NULL;

			return 0;
		}
	}

	return 0;
}

static gint
for_each_modified (GnomePilotConduitSyncAbs *conduit,
		   EToDoLocalRecord **local,
		   EToDoConduitContext *ctxt)
{
	static GList *iterator;
	static int count;

	g_return_val_if_fail (local != NULL, 0);

	if (*local == NULL) {
		LOG ("beginning for_each_modified: beginning\n");
		
		iterator = ctxt->changed;
		
		count = 0;
	
		LOG ("iterating over %d records", g_hash_table_size (ctxt->changed_hash));
		
		iterator = next_changed_item (ctxt, iterator);
		if (iterator != NULL) {
			CalObjChange *coc = NULL;
			
			coc = iterator->data;
		
			LOG ("iterating over %d records", g_hash_table_size (ctxt->changed_hash));

			*local = g_new0 (EToDoLocalRecord, 1);
			local_record_from_uid (*local, coc->uid, ctxt);
		} else {
			LOG ("no events");

			*local = NULL;
		}
	} else {
		count++;
		iterator = g_list_next (iterator);
		if (iterator && (iterator = next_changed_item (ctxt, iterator))) {
			CalObjChange *coc = NULL;

			coc = iterator->data;
			
			*local = g_new0 (EToDoLocalRecord, 1);
			local_record_from_uid (*local, coc->uid, ctxt);
		} else {
			LOG ("for_each_modified ending");

			/* Signal the iteration is over */
			*local = NULL;
		}
	}

	return 0;
}

static gint
compare (GnomePilotConduitSyncAbs *conduit,
	 EToDoLocalRecord *local,
	 GnomePilotRecord *remote,
	 EToDoConduitContext *ctxt)
{
	/* used by the quick compare */
	GnomePilotRecord *local_pilot;
	int retval = 0;

	LOG ("compare: local=%s remote=%s...\n",
	     print_local (local), print_remote (remote));

	g_return_val_if_fail (local!=NULL,-1);
	g_return_val_if_fail (remote!=NULL,-1);

	local_pilot = local_record_to_pilot_record (local, ctxt);
	if (!local_pilot) 
		return -1;

	if (remote->length != local_pilot->length
	    || memcmp (local_pilot->record, remote->record, remote->length))
		retval = 1;

	if (retval == 0)
		LOG ("    equal");
	else
		LOG ("    not equal");
	
	g_free (local_pilot);
	
	return retval;
}

static gint
add_record (GnomePilotConduitSyncAbs *conduit,
	    GnomePilotRecord *remote,
	    EToDoConduitContext *ctxt)
{
	CalComponent *comp;
	const char *uid;
	int retval = 0;
	
	g_return_val_if_fail (remote != NULL, -1);

	LOG ("add_record: adding %s to desktop\n", print_remote (remote));

	comp = comp_from_remote_record (conduit, remote, NULL);
	update_comp (conduit, comp, ctxt);

	cal_component_get_uid (comp, &uid);

	e_pilot_map_insert (ctxt->map, remote->ID, uid, FALSE);

	return retval;
}

static gint
replace_record (GnomePilotConduitSyncAbs *conduit,
		EToDoLocalRecord *local,
		GnomePilotRecord *remote,
		EToDoConduitContext *ctxt)
{
	CalComponent *new_comp;
	int retval = 0;
	
	g_return_val_if_fail (remote != NULL, -1);

	LOG ("replace_record: replace %s with %s\n",
	     print_local (local), print_remote (remote));

	new_comp = comp_from_remote_record (conduit, remote, local->comp);
	gtk_object_unref (GTK_OBJECT (local->comp));
	local->comp = new_comp;
	update_comp (conduit, local->comp, ctxt);

	return retval;
}

static gint
delete_record (GnomePilotConduitSyncAbs *conduit,
	       EToDoLocalRecord *local,
	       EToDoConduitContext *ctxt)
{
	const char *uid;

	g_return_val_if_fail (local != NULL, -1);
	g_return_val_if_fail (local->comp != NULL, -1);

	cal_component_get_uid (local->comp, &uid);

	LOG ("delete_record: deleting %s\n", uid);

	cal_client_remove_object (ctxt->client, uid);
	
        return 0;
}

static gint
archive_record (GnomePilotConduitSyncAbs *conduit,
		EToDoLocalRecord *local,
		gboolean archive,
		EToDoConduitContext *ctxt)
{
	const char *uid;
	int retval = 0;
	
	g_return_val_if_fail (local != NULL, -1);

	LOG ("archive_record: %s\n", archive ? "yes" : "no");

	cal_component_get_uid (local->comp, &uid);
	e_pilot_map_insert (ctxt->map, local->local.ID, uid, archive);
	
        return retval;
}

static gint
match (GnomePilotConduitSyncAbs *conduit,
       GnomePilotRecord *remote,
       EToDoLocalRecord **local,
       EToDoConduitContext *ctxt)
{
	const char *uid;
	
	LOG ("match: looking for local copy of %s\n",
	     print_remote (remote));	
	
	g_return_val_if_fail (local != NULL, -1);
	g_return_val_if_fail (remote != NULL, -1);

	*local = NULL;
	uid = e_pilot_map_lookup_uid (ctxt->map, remote->ID);
	
	if (!uid)
		return 0;

	LOG ("  matched\n");
	
	*local = g_new0 (EToDoLocalRecord, 1);
	local_record_from_uid (*local, uid, ctxt);
	
	return 0;
}

static gint
free_match (GnomePilotConduitSyncAbs *conduit,
	    EToDoLocalRecord *local,
	    EToDoConduitContext *ctxt)
{
	LOG ("free_match: freeing\n");

	g_return_val_if_fail (local != NULL, -1);

	gtk_object_unref (GTK_OBJECT (local->comp));
	g_free (local);

	return 0;
}

static gint
prepare (GnomePilotConduitSyncAbs *conduit,
	 EToDoLocalRecord *local,
	 GnomePilotRecord **remote,
	 EToDoConduitContext *ctxt)
{
	LOG ("prepare: encoding local %s\n", print_local (local));

	*remote = local_record_to_pilot_record (local, ctxt);

	if (!*remote)
		return -1;
	
	return 0;
}

static gint
free_prepare (GnomePilotConduitSyncAbs *conduit,
	      GnomePilotRecord *remote,
	      EToDoConduitContext *ctxt)
{
	LOG ("free_prepare: freeing\n");

	g_return_val_if_fail (remote != NULL, -1);

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
conduit_get_gpilot_conduit (guint32 pilot_id)
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

	retval = gnome_pilot_conduit_sync_abs_new ("ToDoDB", 0x746F646F);
	g_assert (retval != NULL);

	gnome_pilot_conduit_construct (GNOME_PILOT_CONDUIT (retval),
				       "e_todo_conduit");

	e_todo_context_new (&ctxt, pilot_id);
	gtk_object_set_data (GTK_OBJECT (retval), "todoconduit_context", ctxt);

	gtk_signal_connect (retval, "pre_sync", (GtkSignalFunc) pre_sync, ctxt);
	gtk_signal_connect (retval, "post_sync", (GtkSignalFunc) post_sync, ctxt);

  	gtk_signal_connect (retval, "set_pilot_id", (GtkSignalFunc) set_pilot_id, ctxt);
  	gtk_signal_connect (retval, "set_status_cleared", (GtkSignalFunc) set_status_cleared, ctxt);

  	gtk_signal_connect (retval, "for_each", (GtkSignalFunc) for_each, ctxt);
  	gtk_signal_connect (retval, "for_each_modified", (GtkSignalFunc) for_each_modified, ctxt);
  	gtk_signal_connect (retval, "compare", (GtkSignalFunc) compare, ctxt);

  	gtk_signal_connect (retval, "add_record", (GtkSignalFunc) add_record, ctxt);
  	gtk_signal_connect (retval, "replace_record", (GtkSignalFunc) replace_record, ctxt);
  	gtk_signal_connect (retval, "delete_record", (GtkSignalFunc) delete_record, ctxt);
  	gtk_signal_connect (retval, "archive_record", (GtkSignalFunc) archive_record, ctxt);

  	gtk_signal_connect (retval, "match", (GtkSignalFunc) match, ctxt);
  	gtk_signal_connect (retval, "free_match", (GtkSignalFunc) free_match, ctxt);

  	gtk_signal_connect (retval, "prepare", (GtkSignalFunc) prepare, ctxt);
  	gtk_signal_connect (retval, "free_prepare", (GtkSignalFunc) free_prepare, ctxt);

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
