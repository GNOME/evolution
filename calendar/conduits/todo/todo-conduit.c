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

#include <cal-client/cal-client.h>
#include <cal-util/timeutil.h>
#include <pi-source.h>
#include <pi-socket.h>
#include <pi-file.h>
#include <pi-dlp.h>
#include <pi-version.h>
#include <gpilotd/gnome-pilot-conduit.h>
#include <gpilotd/gnome-pilot-conduit-standard-abs.h>
#include <todo-conduit-config.h>
#include <todo-conduit.h>
#include <libical/src/libical/icaltypes.h>

#include <liboaf/liboaf.h>
#include <bonobo.h>

GnomePilotConduit * conduit_get_gpilot_conduit (guint32);
void conduit_destroy_gpilot_conduit (GnomePilotConduit*);

#define CONDUIT_VERSION "0.8.11"
#ifdef G_LOG_DOMAIN
#undef G_LOG_DOMAIN
#endif
#define G_LOG_DOMAIN "todoconduit"

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
static char *print_local (EToDoLocalRecord *local)
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

/* Map routines */
static char *
map_name (EToDoConduitContext *ctxt) 
{
	char *filename;

	
	filename = g_strdup_printf ("%s/evolution/local/Calendar/pilot-map-%d.xml", g_get_home_dir (), ctxt->cfg->pilot_id);

	return filename;
}

static void
map_set_node_timet (xmlNodePtr node, const char *name, time_t t)
{
	char *tstring;
	
	tstring = g_strdup_printf ("%ld", t);
	xmlSetProp (node, name, tstring);
}

static void
map_sax_start_element (void *data, const xmlChar *name, 
		       const xmlChar **attrs)
{
	EToDoConduitContext *ctxt = (EToDoConduitContext *)data;

	if (!strcmp (name, "PilotMap")) {
		while (attrs && *attrs != NULL) {
			const xmlChar **val = attrs;
			
			val++;
			if (!strcmp (*attrs, "timestamp")) 
				ctxt->since = (time_t)strtoul (*val, NULL, 0);

			attrs = ++val;
		}
	}
	 
	if (!strcmp (name, "map")) {
		char *uid = NULL;
		guint32 *pid = g_new (guint32, 1);

		*pid = 0;

		while (attrs && *attrs != NULL) {
			const xmlChar **val = attrs;
			
			val++;
			if (!strcmp (*attrs, "uid")) 
				uid = g_strdup (*val);
			
			if (!strcmp (*attrs, "pilotid"))
				*pid = strtoul (*val, NULL, 0);
				
			attrs = ++val;
		}
			
		if (uid && *pid != 0)
			g_hash_table_insert (ctxt->map, pid, uid);
		else
			g_free (pid);
	}
}

static void
map_write_foreach (gpointer key, gpointer value, gpointer data)
{
	xmlNodePtr root = data;
	xmlNodePtr mnode;
	unsigned long *pid = key;
	const char *uid = value;
	char *pidstr;
	
	mnode = xmlNewChild (root, NULL, "map", NULL);
	xmlSetProp (mnode, "uid", uid);
	pidstr = g_strdup_printf ("%lu", *pid);
	xmlSetProp (mnode, "pilot_id", pidstr);
	g_free (pidstr);
}
		
static int
map_write (EToDoConduitContext *ctxt, char *filename)
{
	xmlDocPtr doc;
	int ret;
	
	if (ctxt->map == NULL)
		return 0;
	
	doc = xmlNewDoc ("1.0");
	if (doc == NULL) {
		WARN ("Pilot map file could not be created\n");
		return -1;
	}
	doc->root = xmlNewDocNode(doc, NULL, "PilotMap", NULL);
	map_set_node_timet (doc->root, "timestamp", time (NULL));

	g_hash_table_foreach (ctxt->map, map_write_foreach, doc->root);
	
	/* Write the file */
	xmlSetDocCompressMode (doc, 0);
	ret = xmlSaveFile (filename, doc);
	if (ret < 0) {
		g_warning ("Pilot map file '%s' could not be saved\n", filename);
		return -1;
	}
	
	xmlFreeDoc (doc);

	return 0;
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
static const char *
status_to_string (gint status)
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

static PilotRecord *
local_record_to_pilot_record (EToDoLocalRecord *local,
			      EToDoConduitContext *ctxt)
{
	PilotRecord *p = NULL;
	
	g_return_val_if_fail (local != NULL, NULL);
	g_assert (local->comp != NULL);
	g_assert (local->todo != NULL );
	
	LOG ("local_record_to_remote_record\n");

	p = g_new0 (PilotRecord, 1);

	p->ID = local->local.ID;
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
local_record_from_comp (EToDoLocalRecord *local, CalComponent *comp) 
{
	int *priority;
	struct icaltimetype *completed;
	CalComponentText summary;
	GSList *d_list = NULL;
	CalComponentText *description;
	CalComponentDateTime due;
	time_t due_time;
	CalComponentClassification classif;
	unsigned long *pilot_id;
	unsigned long *pilot_status;

	LOG ("local_record_from_comp\n");

	g_return_if_fail (local != NULL);
	g_return_if_fail (comp != NULL);

	local->comp = comp;

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

static void 
local_record_from_uid (EToDoLocalRecord *local,
		       char *uid,
		       EToDoConduitContext *ctxt)
{
	CalComponent *comp;
	CalClientGetStatus status;

	g_assert(local!=NULL);

	status = cal_client_get_object (ctxt->client, uid, &comp);

	if (status == CAL_CLIENT_GET_SUCCESS) {
		local_record_from_comp (local, comp);
	} else {
		INFO ("Object did not exist");
	}	
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
 	     "merging remote %s into local %s\n", 
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

	cal_component_set_pilot_id (comp, &remote->ID);
	cal_component_set_pilot_status (comp, &pilot_status);

	cal_component_commit_sequence (comp);
	
	free_ToDo(&todo);

	return comp;
}

static void
update_comp (GnomePilotConduitStandardAbs *conduit, CalComponent *comp,
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
	map_count = g_hash_table_size (ctxt->map);
	
	/* If there are no objects or objects but no log */
	if ((count == 0) || (count > 0 && map_count == 0)) {
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
	GnomePilotConduitStandardAbs *abs_conduit;
	int len, ret;
	unsigned char *buf;
	char *filename;
	xmlSAXHandler handler;
	GList *changes, *l;
	gint num_records;

	abs_conduit = GNOME_PILOT_CONDUIT_STANDARD_ABS (conduit);

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
	
	/* Load the uid <--> pilot id mapping */
	ctxt->map = g_hash_table_new (g_int_hash, g_int_equal);

	filename = map_name (ctxt);
	if (g_file_exists (filename)) {
		memset (&handler, 0, sizeof (xmlSAXHandler));
		handler.startElement = map_sax_start_element;
		
		if (xmlSAXUserParseFile (&handler, ctxt, filename) < 0)
			return -1;
	}
	
	g_free (filename);

	/* Find the added, modified and deleted items */
	changes = cal_client_get_changed_uids (ctxt->client, CALOBJ_TYPE_TODO,
					       ctxt->since);
	for (l = changes; l != NULL; l = l->next) {
		CalObjChange *coc = l->data;

		switch (coc->type) {
		case CALOBJ_UPDATED:
			if (g_hash_table_lookup (ctxt->map, coc->uid))
				ctxt->modified = g_list_prepend (ctxt->modified, coc);
			else
				ctxt->added = g_list_prepend (ctxt->added, coc);
			break;
		case CALOBJ_REMOVED:
			ctxt->deleted = g_list_prepend (ctxt->deleted, coc);
			break;
		}
	}
	g_list_free (changes);
	
	/* Set the count information */
	num_records = cal_client_get_n_objects (ctxt->client, CALOBJ_TYPE_TODO);
	gnome_pilot_conduit_standard_abs_set_num_local_records(abs_conduit, num_records);
	num_records = g_list_length (ctxt->added);
	gnome_pilot_conduit_standard_abs_set_num_new_local_records (abs_conduit, num_records);
	num_records = g_list_length (ctxt->modified);
	gnome_pilot_conduit_standard_abs_set_num_updated_local_records (abs_conduit, num_records);
	num_records = g_list_length (ctxt->deleted);
	gnome_pilot_conduit_standard_abs_set_num_deleted_local_records(abs_conduit, num_records);

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
update_record (GnomePilotConduitStandardAbs *conduit,
	       PilotRecord *remote,
	       EToDoConduitContext *ctxt)
{
	CalComponent *comp;
	CalClientGetStatus status;
	struct ToDo todo;
	const char *uid;

	LOG ("update_record\n");

	g_return_val_if_fail (remote != NULL, -1);

	memset (&todo, 0, sizeof (struct ToDo));
	unpack_ToDo (&todo, remote->record, remote->length);

	uid = g_hash_table_lookup (ctxt->map, &remote->ID);
	
	if (uid)
		status = cal_client_get_object (ctxt->client, uid, &comp);
	else
		status = CAL_CLIENT_LOAD_ERROR;
	
	if (status != CAL_CLIENT_GET_SUCCESS) {
		LOG ("  new record being created\n");
		comp = comp_from_remote_record (conduit, remote, NULL);
	} else {
		CalComponent *new_comp;

		LOG ("  record found\n");

		new_comp = comp_from_remote_record (conduit, remote, comp);
		gtk_object_unref (GTK_OBJECT (comp));
		comp = new_comp;
	}

	update_comp (conduit, comp, ctxt);

	if (!uid) {
		guint32 *pid = g_new (guint32, 1);

		*pid = remote->ID;
		cal_component_get_uid (comp, &uid);
		g_hash_table_insert (ctxt->map, pid, g_strdup (uid));
	}
	
	gtk_object_unref (GTK_OBJECT (comp));
	free_ToDo(&todo);

	return 0;
}

static gint
match_record (GnomePilotConduitStandardAbs *conduit,
	      EToDoLocalRecord **local,
	      PilotRecord *remote,
	      EToDoConduitContext *ctxt)
{
	char *uid;
	
	LOG ("match_record: looking for local copy of %s\n",
	     print_remote (remote));	
	
	g_return_val_if_fail (local != NULL, -1);
	g_return_val_if_fail (remote != NULL, -1);

	uid = g_hash_table_lookup (ctxt->map, &remote->ID);
	
	if (!uid)
		return -1;

	LOG ("  matched\n");
	
	*local = g_new0 (EToDoLocalRecord, 1);
	local_record_from_uid (*local, uid, ctxt);
	
	if (*local == NULL)
		return -1;
	
	return 0;
}

static gint
free_match (GnomePilotConduitStandardAbs *conduit,
	    EToDoLocalRecord **local,
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

static gint
archive_local (GnomePilotConduitStandardAbs *conduit,
	       EToDoLocalRecord *local,
	       EToDoConduitContext *ctxt)
{
	LOG ("archive_local: doing nothing with %s\n", print_local (local));

	g_return_val_if_fail (local != NULL, -1);

	return -1;
}

static gint
archive_remote (GnomePilotConduitStandardAbs *conduit,
		EToDoLocalRecord *local,
		PilotRecord *remote,
		EToDoConduitContext *ctxt)
{
	LOG ("archive_remote: doing nothing with %s\n",
	     print_local (local));
	
	g_return_val_if_fail (remote != NULL, -1); 
	g_return_val_if_fail (local != NULL, -1);
	
	return -1;
}

static gint
store_remote (GnomePilotConduitStandardAbs *conduit,
	      PilotRecord *remote,
	      EToDoConduitContext *ctxt)
{
	int ret;

	g_return_val_if_fail (remote != NULL, -1);

	LOG ("store_remote: copying pilot record %s to desktop\n",
		print_remote (remote));

	ret = update_record (conduit, remote, ctxt);

	return ret;
}

static gint
clear_status_archive_local (GnomePilotConduitStandardAbs *conduit,
			    EToDoLocalRecord *local,
			    EToDoConduitContext *ctxt)
{
	LOG ("clear_status_archive_local: doing nothing\n");

	g_return_val_if_fail(local!=NULL,-1);

        return -1;
}

static gint
iterate (GnomePilotConduitStandardAbs *conduit,
	 EToDoLocalRecord **local,
	 EToDoConduitContext *ctxt)
{
	static GList *uids, *iterator;
	static int count;

	g_return_val_if_fail (local != NULL, -1);

	if (*local == NULL) {
		LOG ("beginning iteration");

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
			return -1;
		}
	} else {
		count++;
		if (g_list_next (iterator)) {
			iterator = g_list_next (iterator);

			*local = g_new0 (EToDoLocalRecord, 1);
			local_record_from_uid (*local, iterator->data, ctxt);
		} else {
			LOG ("iteration ending");

			/* Tell the pilot the iteration is over */
			*local = NULL;

			return 0;
		}
	}
	return 1;
}

static gint
iterate_specific (GnomePilotConduitStandardAbs *conduit,
		  EToDoLocalRecord **local,
		  gint flag,
		  gint archived,
		  EToDoConduitContext *ctxt)
{
	static GList *changes, *iterator;
	static int count;

	g_return_val_if_fail (local != NULL, 0);

	/* FIX ME Hack - gnome-pilot does not recognize iterate_specific err values */
	if (*local == NULL) {
		LOG ("beginning iteration for %s\n", status_to_string (flag));

		switch (flag) {
		case GnomePilotRecordNew:
			changes = ctxt->added;
			break;
		case GnomePilotRecordModified:
			changes = ctxt->modified;
			break;
		case GnomePilotRecordDeleted:
			changes = ctxt->deleted;
			break;
		}
		
		count = 0;
		
		if (changes != NULL) {
			CalObjChange *coc = changes->data;
			
			LOG ("iterating over %d records", g_list_length (changes));
			 
			*local = g_new0 (EToDoLocalRecord, 1);
			local_record_from_uid (*local, coc->uid, ctxt);

			iterator = changes;
		} else {
			LOG ("no events");
			(*local) = NULL;
			return 0;
		}
	} else {
		count++;
		if (g_list_next (iterator)) {
			CalObjChange *coc;

			iterator = g_list_next (iterator);
			coc = iterator->data;

			*local = g_new0 (EToDoLocalRecord, 1);
			local_record_from_uid (*local, coc->uid, ctxt);
		} else {
			LOG ("iteration ending");

			/* Tell the pilot the iteration is over */
			(*local) = NULL;

			return 0;
		}
	}

	return 1;
}

static gint
purge (GnomePilotConduitStandardAbs *conduit,
       EToDoConduitContext *ctxt)
{
	GList *l;
	char *filename;
	int ret = 0;
	
	LOG ("purge\n");

	/* FIX ME report an error */
	for (l = ctxt->deleted; l != NULL; l = l->next) {
		CalObjChange *coc = l->data;
	
		cal_client_remove_object (ctxt->client, coc->uid);
	}

	filename = map_name (ctxt);
	ret = map_write (ctxt, filename);
	g_free (filename);

	return ret;
}


static gint
set_status (GnomePilotConduitStandardAbs *conduit,
	    EToDoLocalRecord *local,
	    gint status,
	    EToDoConduitContext *ctxt)
{
	CalObjChange *coc;

	g_return_val_if_fail (local != NULL, -1);
	g_assert (local->comp != NULL);

	LOG ("set_status: %s status is now '%s' for %s\n",
	     print_local (local),
	     status_to_string (status),
	     cal_component_get_as_string (local->comp));

	/* FIX ME New and modified? */
	switch (status) {
	case GnomePilotRecordNew:
		break;
	case GnomePilotRecordModified:
		break;
	case GnomePilotRecordDeleted:
		coc = g_new0 (CalObjChange, 1);
		ctxt->deleted = g_list_prepend (ctxt->deleted, coc);
		break;
	}

        return 0;
}

static gint
set_archived (GnomePilotConduitStandardAbs *conduit,
	      EToDoLocalRecord *local,
	      gint archived,
	      EToDoConduitContext *ctxt)
{
	LOG ("set_archived: %s archived flag is now '%d'\n",
		print_local (local), archived);

	g_return_val_if_fail(local!=NULL,-1);
	g_assert(local->comp!=NULL);

	local->local.archived = archived;
	update_comp (conduit, local->comp, ctxt);

	/* FIXME: This should move the entry into a speciel
	   calendar file, eg. Archive, or (by config option), simply
	   delete it */

        return 0;
}

static gint
set_pilot_id (GnomePilotConduitStandardAbs *conduit,
	      EToDoLocalRecord *local,
	      guint32 ID,
	      EToDoConduitContext *ctxt)
{
	const char *uid;
	guint32 *pid = g_new (guint32, 1);
	
	cal_component_get_uid (local->comp, &uid);
	*pid = ID;
	g_hash_table_insert (ctxt->map, pid, g_strdup (uid));

        return 0;
}

static gint
transmit (GnomePilotConduitStandardAbs *conduit,
	  EToDoLocalRecord *local,
	  PilotRecord **remote,
	  EToDoConduitContext *ctxt)
{
	LOG ("transmit: encoding local %s\n", print_local (local));

	*remote = local_record_to_pilot_record (local, ctxt);

	if (!*remote)
		return -1;
	
	return 0;
}

static gint
free_transmit (GnomePilotConduitStandardAbs *conduit,
	       EToDoLocalRecord *local,
	       PilotRecord **remote,
	       EToDoConduitContext *ctxt)
{
	LOG ("free_transmit: freeing %s\n", print_local (local));

	g_return_val_if_fail (local != NULL, -1);
	g_return_val_if_fail (remote != NULL, -1);

	g_free (*remote);
	*remote = NULL;

        return 0;
}


static gint
compare (GnomePilotConduitStandardAbs *conduit,
	 EToDoLocalRecord *local,
	 PilotRecord *remote,
	 EToDoConduitContext *ctxt)
{
	/* used by the quick compare */
	PilotRecord *local_pilot;
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
		LOG ("    match.\n");
	else
		LOG ("    did not match");
	
	g_free (local_pilot);
	
	return retval;
}


static gint
compare_backup (GnomePilotConduitStandardAbs *conduit,
		EToDoLocalRecord *local,
		PilotRecord *remote,
		EToDoConduitContext *ctxt)
{
	LOG ("compare_backup: doing nothing\n");

	g_return_val_if_fail(local!=NULL,-1);
	g_return_val_if_fail(remote!=NULL,-1);

	/* FIX ME - What the hell? */

        return -1;
}


static gint
delete_all (GnomePilotConduitStandardAbs *conduit,
	    EToDoConduitContext *ctxt)
{
	GList *uids, *it;
	gboolean success;

	LOG ("delete_all: deleting all objects from desktop\n");

	uids = cal_client_get_uids (ctxt->client, CALOBJ_TYPE_TODO);

	for (it = uids; it != NULL; it = g_list_next (it)) {
		success = cal_client_remove_object (ctxt->client, it->data);

		if (!success)
			INFO ("Object did not exist");
	}
	cal_obj_uid_list_free (uids);

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

	retval = gnome_pilot_conduit_standard_abs_new ("ToDoDB", 0x746F646F);
	g_assert (retval != NULL);

	gnome_pilot_conduit_construct (GNOME_PILOT_CONDUIT (retval),
				       "e_todo_conduit");

	e_todo_context_new (&ctxt, pilot_id);
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
