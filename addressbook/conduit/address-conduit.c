/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Evolution addressbook - Address Conduit
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
#include <pi-source.h>
#include <pi-socket.h>
#include <pi-file.h>
#include <pi-dlp.h>
#include <pi-version.h>
#include <ebook/e-book.h>
#include <ebook/e-card-cursor.h>
#include <ebook/e-card.h>
#include <ebook/e-card-simple.h>

#define ADDR_CONFIG_LOAD 1
#define ADDR_CONFIG_DESTROY 1
#include <address-conduit-config.h>
#undef ADDR_CONFIG_LOAD
#undef ADDR_CONFIG_DESTROY

#include <address-conduit.h>

GnomePilotConduit * conduit_get_gpilot_conduit (guint32);
void conduit_destroy_gpilot_conduit (GnomePilotConduit*);

#define CONDUIT_VERSION "0.1.0"
#ifdef G_LOG_DOMAIN
#undef G_LOG_DOMAIN
#endif
#define G_LOG_DOMAIN "eaddrconduit"

#define DEBUG_CONDUIT 1
/* #undef DEBUG_CONDUIT */

#ifdef DEBUG_CONDUIT
#define LOG(e...) g_log (G_LOG_DOMAIN, G_LOG_LEVEL_MESSAGE, e)
#else
#define LOG(e...)
#endif 

#define WARN(e...) g_log (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, e)
#define INFO(e...) g_log (G_LOG_DOMAIN, G_LOG_LEVEL_MESSAGE, e)

typedef struct {
	EBookStatus status;
	char *id;
} add_card_cons;

/* debug spew DELETE ME */
static char *
print_local (EAddrLocalRecord *local)
{
	static char buff[ 4096 ];

	if (local == NULL) {
		sprintf (buff, "[NULL]");
		return buff;
	}

/*  	if (local->addr && local->addr->description) { */
/*  		sprintf (buff, "[%d %ld %d %d '%s' '%s']", */
/*  			 local->todo->indefinite, */
/*  			 mktime (& local->todo->due), */
/*  			 local->todo->priority, */
/*  			 local->todo->complete, */
/*  			 local->todo->description, */
/*  			 local->todo->note); */
/*  		return buff; */
/*  	} */

	return "";
}


/* debug spew DELETE ME */
static char *print_remote (GnomePilotRecord *remote)
{
	static char buff[ 4096 ];
	struct Address addr;

	if (remote == NULL) {
		sprintf (buff, "[NULL]");
		return buff;
	}

	memset (&addr, 0, sizeof (struct Address));
	unpack_Address (&addr, remote->record, remote->length);

	sprintf (buff, "Hi");
/*  	sprintf (buff, "[%d %ld %d %d '%s' '%s']", */
/*  		 todo.indefinite, */
/*  		 mktime (& todo.due), */
/*  		 todo.priority, */
/*  		 todo.complete, */
/*  		 todo.description, */
/*  		 todo.note); */

	return buff;
}

/* Context Routines */
static void
e_addr_context_new (EAddrConduitContext **ctxt, guint32 pilot_id) 
{
	*ctxt = g_new0 (EAddrConduitContext,1);
	g_assert (ctxt!=NULL);

	addrconduit_load_configuration (&(*ctxt)->cfg, pilot_id);
}

static void
e_addr_context_destroy (EAddrConduitContext **ctxt)
{
	g_return_if_fail (ctxt!=NULL);
	g_return_if_fail (*ctxt!=NULL);

	if ((*ctxt)->cfg != NULL)
		addrconduit_destroy_configuration (&(*ctxt)->cfg);

	g_free (*ctxt);
	*ctxt = NULL;
}

/* Map routines */
static char *
map_name (EAddrConduitContext *ctxt) 
{
	char *filename = NULL;
	
	filename = g_strdup_printf ("%s/evolution/local/Contacts/pilot-map-%d.xml", g_get_home_dir (), ctxt->cfg->pilot_id);

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
	EAddrConduitContext *ctxt = (EAddrConduitContext *)data;

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
			
			if (!strcmp (*attrs, "pilot_id"))
				*pid = strtoul (*val, NULL, 0);
				
			attrs = ++val;
		}
			
		if (uid && *pid != 0) {
			g_hash_table_insert (ctxt->pid_map, pid, uid);
			g_hash_table_insert (ctxt->uid_map, uid, pid);
		} else {
			g_free (pid);
		}
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
map_write (EAddrConduitContext *ctxt, char *filename)
{
	xmlDocPtr doc;
	int ret;
	
	if (ctxt->pid_map == NULL)
		return 0;
	
	doc = xmlNewDoc ("1.0");
	if (doc == NULL) {
		WARN ("Pilot map file could not be created\n");
		return -1;
	}
	doc->root = xmlNewDocNode(doc, NULL, "PilotMap", NULL);
	map_set_node_timet (doc->root, "timestamp", time (NULL));

	g_hash_table_foreach (ctxt->pid_map, map_write_foreach, doc->root);
	
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

/* Addressbok Server routines */
static void
add_card_cb (EBook *ebook, EBookStatus status, const char *id, gpointer closure)
{
	add_card_cons *cons = (add_card_cons*)closure;

	cons->status = status;
	cons->id = g_strdup (id);

	gtk_main_quit();
}

static void
status_cb (EBook *ebook, EBookStatus status, gpointer closure)
{
	(*(EBookStatus*)closure) = status;
	gtk_main_quit();
}

static void
cursor_cb (EBook *book, EBookStatus status, ECardCursor *cursor, gpointer closure)
{
	EAddrConduitContext *ctxt = (EAddrConduitContext*)closure;

	if (status == E_BOOK_STATUS_SUCCESS) {
		long length;
		int i;

		// ctxt->cursor = cursor;
		ctxt->address_load_success = TRUE;

		length = e_card_cursor_get_length (cursor);
		ctxt->cards = NULL;
		for (i = 0; i < length; i ++)
			ctxt->cards = g_list_append (ctxt->cards, e_card_cursor_get_nth (cursor, i));

		gtk_main_quit(); /* end the sub event loop */
	}
	else {
		WARN (_("BLARG\n"));
		gtk_main_quit(); /* end the sub event loop */
	}
}

static void
book_open_cb (EBook *book, EBookStatus status, gpointer closure)
{
	EAddrConduitContext *ctxt = (EAddrConduitContext*)closure;

	if (status == E_BOOK_STATUS_SUCCESS) {
		e_book_get_cursor (book, "(contains \"full_name\" \"\")", cursor_cb, ctxt);
	}
	else {
		WARN (_("BLARG\n"));
		gtk_main_quit(); /* end the sub event loop */
	}
}

static int
start_addressbook_server (EAddrConduitContext *ctxt)
{
	gchar *uri, *path;

	g_return_val_if_fail(ctxt!=NULL,-2);

	ctxt->ebook = e_book_new ();

	path = g_concat_dir_and_file (g_get_home_dir (),
				      "evolution/local/Contacts/addressbook.db");
	uri = g_strdup_printf ("file://%s", path);
	g_free (path);

	e_book_load_uri (ctxt->ebook, uri, book_open_cb, ctxt);

	/* run a sub event loop to turn ebook's async loading into a
           synchronous call */
	gtk_main ();

	g_free (uri);

	if (ctxt->address_load_success)
		return 0;

	return -1;
}

/* Utility routines */
static void
compute_pid (EAddrConduitContext *ctxt, EAddrLocalRecord *local, const char *uid)
{
/*  	guint32 *pid; */
	
/*  	pid = g_hash_table_lookup (ctxt->uid_map, uid); */
	
/*  	if (pid) */
/*  		local->local.ID = *pid; */
/*  	else */
/*  		local->local.ID = 0; */
}

static void
compute_status (EAddrConduitContext *ctxt, EAddrLocalRecord *local, const char *uid)
{
	local->local.archived = FALSE;
	local->local.secret = FALSE;
	
/*  	if (g_hash_table_lookup (ctxt->added, uid)) */
/*  		local->local.attr = GnomePilotRecordNew; */
/*  	else if (g_hash_table_lookup (ctxt->modified, uid)) */
/*  		local->local.attr = GnomePilotRecordModified; */
/*  	else if (g_hash_table_lookup (ctxt->deleted, uid)) */
/*  		local->local.attr = GnomePilotRecordDeleted; */
/*  	else */
/*  		local->local.attr = GnomePilotRecordNothing; */
}

static GnomePilotRecord *
local_record_to_pilot_record (EAddrLocalRecord *local,
			      EAddrConduitContext *ctxt)
{
	GnomePilotRecord *p = NULL;
	
	g_return_val_if_fail (local != NULL, NULL);
	g_assert (local->addr != NULL );
	
	LOG ("local_record_to_remote_record\n");

	p = g_new0 (GnomePilotRecord, 1);

	p->ID = local->local.ID;
	p->category = 0;
	p->attr = local->local.attr;
	p->archived = local->local.archived;
	p->secret = local->local.secret;

	/* Generate pilot record structure */
	p->record = g_new0 (char,0xffff);
	p->length = pack_Address (local->addr, p->record, 0xffff);

	return p;	
}

#if 0 
/*
 * converts a CalComponent object to a EAddrLocalRecord
 */
static void
local_record_from_comp (EAddrLocalRecord *local, CalComponent *comp, EAddrConduitContext *ctxt) 
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
	compute_pid (ctxt, local, uid);
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
#endif

static void 
local_record_from_uid (EAddrLocalRecord *local,
		       char *uid,
		       EAddrConduitContext *ctxt)
{
/*  	CalComponent *comp; */
/*  	CalClientGetStatus status; */

/*  	g_assert(local!=NULL); */

/*  	status = cal_client_get_object (ctxt->client, uid, &comp); */

/*  	if (status == CAL_CLIENT_GET_SUCCESS) { */
/*  		local_record_from_comp (local, comp, ctxt); */
/*  	} else if (status == CAL_CLIENT_GET_NOT_FOUND) { */
/*  		comp = cal_component_new (); */
/*  		cal_component_set_new_vtype (comp, CAL_COMPONENT_TODO); */
/*  		cal_component_set_uid (comp, uid); */
/*  		local_record_from_comp (local, comp, ctxt); */
/*  	} else { */
/*  		INFO ("Object did not exist"); */
/*  	}	 */
}

static ECard *
ecard_from_remote_record(EAddrConduitContext *ctxt,
			 GnomePilotRecord *remote,
			 ECard *in_card)
{
	struct Address address;
	ECard *ecard;
	ECardSimple *simple;
	int i;
	char *string;
	char *stringparts[4];
	char *commaparts[3];
	char *spaceparts[3];
	char *commastring, *spacestring;

	g_return_val_if_fail(remote!=NULL,NULL);
	memset (&address, 0, sizeof (struct Address));
	unpack_Address (&address, remote->record, remote->length);

	if (in_card == NULL) {
		ecard = e_card_new("");
		simple = e_card_simple_new(ecard);
	} else {
		ecard = in_card;
		simple = E_CARD_SIMPLE (ecard);
	}

#define get(pilotprop) \
        (address.entry [(pilotprop)])
#define check(pilotprop) \
        (address.entry [(pilotprop)] && *address.entry [(pilotprop)])

	i = 0;
	if (check(entryFirstname))
		stringparts[i++] = get(entryFirstname);
	if (check(entryLastname))
		stringparts[i++] = get(entryLastname);
	stringparts[i] = NULL;
	string = g_strjoinv(" ", stringparts);
	e_card_simple_set(simple, E_CARD_SIMPLE_FIELD_FULL_NAME, string);
	g_free(string);

	i = 0;
	if (check (entryAddress))
		spaceparts[i++] = get (entryAddress);
	if (check (entryZip))
		spaceparts[i++] = get (entryZip);
	spaceparts[i] = 0;
	spacestring = g_strjoinv(" ", spaceparts);

	i = 0;
	if (check (entryCity))
		commaparts[i++] = get (entryCity);
	if (spacestring && *spacestring)
		commaparts[i++] = spacestring;
	commaparts[i] = 0;
	commastring = g_strjoinv(", ", commaparts);

	i = 0;
	if (check (entryAddress))
		stringparts[i++] = get (entryAddress);
	if (commastring && *commastring)
		stringparts[i++] = commastring;
	if (check (entryCountry))
		stringparts[i++] = get (entryCountry);
	stringparts[i] = NULL;
	string = g_strjoinv("\n", stringparts);
	e_card_simple_set(simple, E_CARD_SIMPLE_FIELD_ADDRESS_HOME, string);

	g_free(spacestring);
	g_free(commastring);
	g_free(string);

	if (check (entryTitle))
		e_card_simple_set(simple, E_CARD_SIMPLE_FIELD_TITLE, get (entryTitle));

	if (check (entryCompany))
		e_card_simple_set(simple, E_CARD_SIMPLE_FIELD_ORG, get (entryCompany));

	for (i = entryPhone1; i <= entryPhone5; i ++) {
		if (address.entry [i] && *(address.entry [i])) {
			char *phonelabel = ctxt->ai.phoneLabels[address.phoneLabel[i - entryPhone1]];
			if (!strcmp (phonelabel, "E-mail"))
				e_card_simple_set(simple, E_CARD_SIMPLE_FIELD_EMAIL, address.entry[i]);
			else if (!strcmp (phonelabel, "Home"))
				e_card_simple_set(simple, E_CARD_SIMPLE_FIELD_PHONE_HOME, address.entry[i]);
			else if (!strcmp (phonelabel, "Work"))
				e_card_simple_set(simple, E_CARD_SIMPLE_FIELD_PHONE_BUSINESS, address.entry[i]);
			else if (!strcmp (phonelabel, "Fax"))
				e_card_simple_set(simple, E_CARD_SIMPLE_FIELD_PHONE_BUSINESS_FAX, address.entry[i]);
			else if (!strcmp (phonelabel, "Other"))
				e_card_simple_set(simple, E_CARD_SIMPLE_FIELD_PHONE_OTHER, address.entry[i]);
			else if (!strcmp (phonelabel, "Main"))
				e_card_simple_set(simple, E_CARD_SIMPLE_FIELD_PHONE_PRIMARY, address.entry[i]);
			else if (!strcmp (phonelabel, "Pager"))
				e_card_simple_set(simple, E_CARD_SIMPLE_FIELD_PHONE_PAGER, address.entry[i]);
			else if (!strcmp (phonelabel, "Mobile"))
				e_card_simple_set(simple, E_CARD_SIMPLE_FIELD_PHONE_MOBILE, address.entry[i]);
		}
	}
#undef get
#undef set

	free_Address(&address);

	gtk_object_unref(GTK_OBJECT(simple));

	return ecard;
}

#if 0
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
#endif

static void
check_for_slow_setting (GnomePilotConduit *c, EAddrConduitContext *ctxt)
{
	int count, map_count;

/*  	count = g_list_length (ctxt->uids); */
	count = 0;

	map_count = g_hash_table_size (ctxt->pid_map);
	
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
	  EAddrConduitContext *ctxt)
{
	GnomePilotConduitSyncAbs *abs_conduit;
/*    	GList *l; */
	int len;
	unsigned char *buf;
	char *filename;
	xmlSAXHandler handler;
/*  	gint num_records; */

	abs_conduit = GNOME_PILOT_CONDUIT_SYNC_ABS (conduit);

	LOG ("---------------------------------------------------------\n");
	LOG ("pre_sync: Addressbook Conduit v.%s", CONDUIT_VERSION);
	g_message ("Addressbook Conduit v.%s", CONDUIT_VERSION);

	ctxt->ebook = NULL;
	
	if (start_addressbook_server (ctxt) != 0) {
		WARN(_("Could not start wombat server"));
		gnome_pilot_conduit_error (conduit, _("Could not start wombat"));
		return -1;
	}

	/* Load the uid <--> pilot id mappings */
	ctxt->pid_map = g_hash_table_new (g_int_hash, g_int_equal);
	ctxt->uid_map = g_hash_table_new (g_str_hash, g_str_equal);

	filename = map_name (ctxt);
	if (g_file_exists (filename)) {
		memset (&handler, 0, sizeof (xmlSAXHandler));
		handler.startElement = map_sax_start_element;
		
		if (xmlSAXUserParseFile (&handler, ctxt, filename) < 0)
			return -1;
	}
	
	g_free (filename);

	/* Set the count information */
/*  	num_records = cal_client_get_n_objects (ctxt->client, CALOBJ_TYPE_TODO); */
/*  	gnome_pilot_conduit_sync_abs_set_num_local_records(abs_conduit, num_records); */
/*  	num_records = g_hash_table_size (ctxt->added); */
/*  	gnome_pilot_conduit_sync_abs_set_num_new_local_records (abs_conduit, num_records); */
/*  	num_records = g_hash_table_size (ctxt->modified); */
/*  	gnome_pilot_conduit_sync_abs_set_num_updated_local_records (abs_conduit, num_records); */
/*  	num_records = g_hash_table_size (ctxt->deleted); */
/*  	gnome_pilot_conduit_sync_abs_set_num_deleted_local_records(abs_conduit, num_records); */

	gtk_object_set_data (GTK_OBJECT (conduit), "dbinfo", dbi);

	buf = (unsigned char*)g_malloc (0xffff);
	len = dlp_ReadAppBlock (dbi->pilot_socket, dbi->db_handle, 0,
			      (unsigned char *)buf, 0xffff);
	
	if (len < 0) {
		WARN (_("Could not read pilot's Address application block"));
		WARN ("dlp_ReadAppBlock(...) = %d", len);
		gnome_pilot_conduit_error (conduit,
					   _("Could not read pilot's Address application block"));
		return -1;
	}
	unpack_AddressAppInfo (&(ctxt->ai), buf, len);
	g_free (buf);

/*  	check_for_slow_setting (conduit, ctxt); */

	return 0;
}

static gint
post_sync (GnomePilotConduit *conduit,
	   GnomePilotDBInfo *dbi,
	   EAddrConduitContext *ctxt)
{
	gchar *filename;
	
	LOG ("post_sync: Address Conduit v.%s", CONDUIT_VERSION);
	LOG ("---------------------------------------------------------\n");

	filename = map_name (ctxt);
	map_write (ctxt, filename);
	g_free (filename);
	
	return 0;
}

static gint
set_pilot_id (GnomePilotConduitSyncAbs *conduit,
	      EAddrLocalRecord *local,
	      guint32 ID,
	      EAddrConduitContext *ctxt)
{
  	char *new_uid;
  	guint32 *pid = g_new (guint32, 1);

	LOG ("set_pilot_id: setting to %d\n", ID);
	
  	*pid = ID;
  	new_uid = g_strdup (local->ecard->id);
  	g_hash_table_insert (ctxt->pid_map, pid, new_uid);
  	g_hash_table_insert (ctxt->uid_map, new_uid, pid);

        return 0;
}

static gint
for_each (GnomePilotConduitSyncAbs *conduit,
	  EAddrLocalRecord **local,
	  EAddrConduitContext *ctxt)
{
  	static GList *cards, *iterator;
  	static int count;

  	g_return_val_if_fail (local != NULL, -1);

	if (*local == NULL) {
		LOG ("beginning for_each");

		cards = ctxt->cards;
		count = 0;
		
		if (cards != NULL) {
			LOG ("iterating over %d records", g_list_length (cards));

			*local = g_new0 (EAddrLocalRecord, 1);
/*  			local_record_from_uid (*local, uids->data, ctxt) */;

			iterator = cards;
		} else {
			LOG ("no events");
			(*local) = NULL;
			return 0;
		}
	} else {
		count++;
		if (g_list_next (iterator)) {
			iterator = g_list_next (iterator);

			*local = g_new0 (EAddrLocalRecord, 1);
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
		   EAddrLocalRecord **local,
		   EAddrConduitContext *ctxt)
{
/*  	static GList *changes, *iterator; */
/*  	static int count; */

/*  	g_return_val_if_fail (local != NULL, 0); */

/*  	if (*local == NULL) { */
/*  		LOG ("beginning for_each_modified: beginning\n"); */
		
/*  		changes = ctxt->changed; */
		
/*  		count = 0; */
		
/*  		if (changes != NULL) { */
/*  			CalObjChange *coc = changes->data; */
			
/*  			LOG ("iterating over %d records", g_list_length (changes)); */
			 
/*  			*local = g_new0 (EAddrLocalRecord, 1); */
/*  			local_record_from_uid (*local, coc->uid, ctxt); */

/*  			iterator = changes; */
/*  		} else { */
/*  			LOG ("no events"); */
/*  			(*local) = NULL; */
/*  			return 0; */
/*  		} */
/*  	} else { */
/*  		count++; */
/*  		if (g_list_next (iterator)) { */
/*  			CalObjChange *coc; */

/*  			iterator = g_list_next (iterator); */
/*  			coc = iterator->data; */

/*  			*local = g_new0 (EAddrLocalRecord, 1); */
/*  			local_record_from_uid (*local, coc->uid, ctxt); */
/*  		} else { */
/*  			LOG ("for_each_modified ending"); */

    			/* Tell the pilot the iteration is over */
/*  			(*local) = NULL; */

/*  			return 0; */
/*  		} */
/*  	} */

	return 0;
}

static gint
compare (GnomePilotConduitSyncAbs *conduit,
	 EAddrLocalRecord *local,
	 GnomePilotRecord *remote,
	 EAddrConduitContext *ctxt)
{
	/* used by the quick compare */
	GnomePilotRecord *local_pilot = NULL;
	int retval = 0;

	LOG ("compare: local=%s remote=%s...\n",
	     print_local (local), print_remote (remote));

	g_return_val_if_fail (local!=NULL,-1);
	g_return_val_if_fail (remote!=NULL,-1);

/*  	local_pilot = local_record_to_pilot_record (local, ctxt); */
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
	    EAddrConduitContext *ctxt)
{
	ECard *ecard;
  	char *new_uid;
  	guint32 *pid = g_new (guint32, 1);
	add_card_cons cons;
	int retval = 0;
	
	g_return_val_if_fail (remote != NULL, -1);

	LOG ("add_record: adding %s to desktop\n", print_remote (remote));

	ecard = ecard_from_remote_record (ctxt, remote, NULL);

	/* add the ecard to the server */
	e_book_add_card (ctxt->ebook, ecard, add_card_cb, &cons);

	gtk_main(); /* enter sub mainloop */
	
	if (cons.status != E_BOOK_STATUS_SUCCESS) {
		WARN ("add_record: failed to add card to ebook\n");
		return -1;
	}

	ctxt->cards = g_list_append (ctxt->cards,
				     e_book_get_card (ctxt->ebook, cons.id));
	g_free (cons.id);

	*pid = remote->ID;
	new_uid = g_strdup (ecard->id);
	g_hash_table_insert (ctxt->pid_map, pid, new_uid);
	g_hash_table_insert (ctxt->uid_map, new_uid, pid);

	return retval;
}

static gint
add_archive_record (GnomePilotConduitSyncAbs *conduit,
		    EAddrLocalRecord *local,
		    GnomePilotRecord *remote,
		    EAddrConduitContext *ctxt)
{
	int retval = 0;
	
	g_return_val_if_fail (remote != NULL, -1); 
	g_return_val_if_fail (local != NULL, -1);

	LOG ("add_archive_record: doing nothing with %s\n",
	     print_local (local));

	return retval;
}

static gint
replace_record (GnomePilotConduitSyncAbs *conduit,
		EAddrLocalRecord *local,
		GnomePilotRecord *remote,
		EAddrConduitContext *ctxt)
{
	ECard *new_ecard;
	EBookStatus commit_status;
	int retval = 0;
	
	g_return_val_if_fail (remote != NULL, -1);

	LOG ("replace_record: replace %s with %s\n",
	     print_local (local), print_remote (remote));

	new_ecard = ecard_from_remote_record (ctxt, remote, local->ecard);
	gtk_object_unref (GTK_OBJECT (local->ecard));
	local->ecard = new_ecard;

	e_book_commit_card (ctxt->ebook, local->ecard, status_cb, &commit_status);
	
	gtk_main (); /* enter sub mainloop */
	
	if (commit_status != E_BOOK_STATUS_SUCCESS)
		WARN ("replace_record: failed to update card in ebook\n");

	gtk_object_unref (GTK_OBJECT (new_ecard));

	return retval;
}

static gint
delete_record (GnomePilotConduitSyncAbs *conduit,
	       EAddrLocalRecord *local,
	       EAddrConduitContext *ctxt)
{
/*  	const char *uid; */

/*  	g_return_val_if_fail (local != NULL, -1); */
/*  	g_assert (local->comp != NULL); */

/*  	cal_component_get_uid (local->comp, &uid); */

/*  	LOG ("delete_record: deleting %s\n", uid); */

/*  	cal_client_remove_object (ctxt->client, uid); */
	
        return 0;
}

static gint
delete_archive_record (GnomePilotConduitSyncAbs *conduit,
		       EAddrLocalRecord *local,
		       EAddrConduitContext *ctxt)
{
	int retval = 0;
	
	g_return_val_if_fail(local!=NULL,-1);

	LOG ("delete_archive_record: doing nothing\n");

        return retval;
}

static gint
match (GnomePilotConduitSyncAbs *conduit,
       GnomePilotRecord *remote,
       EAddrLocalRecord **local,
       EAddrConduitContext *ctxt)
{
/*  	char *uid; */
	
	LOG ("match: looking for local copy of %s\n",
	     print_remote (remote));	
	
	g_return_val_if_fail (local != NULL, -1);
	g_return_val_if_fail (remote != NULL, -1);

/*  	*local = NULL; */
/*  	uid = g_hash_table_lookup (ctxt->pid_map, &remote->ID); */
	
/*  	if (!uid) */
/*  		return 0; */

/*  	LOG ("  matched\n"); */
	
/*  	*local = g_new0 (EAddrLocalRecord, 1); */
/*  	local_record_from_uid (*local, uid, ctxt); */
	
	return 0;
}

static gint
free_match (GnomePilotConduitSyncAbs *conduit,
	    EAddrLocalRecord *local,
	    EAddrConduitContext *ctxt)
{
	LOG ("free_match: freeing\n");

	g_return_val_if_fail (local != NULL, -1);

/*  	gtk_object_unref (GTK_OBJECT (local->comp)); */
	g_free (local);

	return 0;
}

static gint
prepare (GnomePilotConduitSyncAbs *conduit,
	 EAddrLocalRecord *local,
	 GnomePilotRecord **remote,
	 EAddrConduitContext *ctxt)
{
	LOG ("prepare: encoding local %s\n", print_local (local));

/*  	*remote = local_record_to_pilot_record (local, ctxt); */

/*  	if (!*remote) */
/*  		return -1; */
	
	return 0;
}

static gint
free_prepare (GnomePilotConduitSyncAbs *conduit,
	      EAddrLocalRecord *local,
	      GnomePilotRecord **remote,
	      EAddrConduitContext *ctxt)
{
	LOG ("free_prepare: freeing\n");

	g_return_val_if_fail (local != NULL, -1);
	g_return_val_if_fail (remote != NULL, -1);

	g_free (*remote);
	*remote = NULL;

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
	EAddrConduitContext *ctxt;

	LOG ("in address's conduit_get_gpilot_conduit\n");

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

	retval = gnome_pilot_conduit_sync_abs_new ("AddressDB", 0x61646472);
	g_assert (retval != NULL);

	gnome_pilot_conduit_construct (GNOME_PILOT_CONDUIT (retval),
				       "e_addr_conduit");

	e_addr_context_new (&ctxt, pilot_id);
	gtk_object_set_data (GTK_OBJECT (retval), "addrconduit_context", ctxt);

	gtk_signal_connect (retval, "pre_sync", (GtkSignalFunc) pre_sync, ctxt);
	gtk_signal_connect (retval, "post_sync", (GtkSignalFunc) post_sync, ctxt);

  	gtk_signal_connect (retval, "set_pilot_id", (GtkSignalFunc) set_pilot_id, ctxt);

  	gtk_signal_connect (retval, "for_each", (GtkSignalFunc) for_each, ctxt);
  	gtk_signal_connect (retval, "for_each_modified", (GtkSignalFunc) for_each_modified, ctxt);
  	gtk_signal_connect (retval, "compare", (GtkSignalFunc) compare, ctxt);

  	gtk_signal_connect (retval, "add_record", (GtkSignalFunc) add_record, ctxt);
  	gtk_signal_connect (retval, "add_archive_record", (GtkSignalFunc) add_archive_record, ctxt); 

  	gtk_signal_connect (retval, "replace_record", (GtkSignalFunc) replace_record, ctxt);

  	gtk_signal_connect (retval, "delete_record", (GtkSignalFunc) delete_record, ctxt);
  	gtk_signal_connect (retval, "delete_archive_record", (GtkSignalFunc) delete_archive_record, ctxt);

  	gtk_signal_connect (retval, "match", (GtkSignalFunc) match, ctxt);
  	gtk_signal_connect (retval, "free_match", (GtkSignalFunc) free_match, ctxt);

  	gtk_signal_connect (retval, "prepare", (GtkSignalFunc) prepare, ctxt);
  	gtk_signal_connect (retval, "free_prepare", (GtkSignalFunc) free_prepare, ctxt);

	return GNOME_PILOT_CONDUIT (retval);
}

void
conduit_destroy_gpilot_conduit (GnomePilotConduit *conduit)
{ 
	EAddrConduitContext *ctxt;

	ctxt = gtk_object_get_data (GTK_OBJECT (conduit), 
				    "addrconduit_context");

	e_addr_context_destroy (&ctxt);

	gtk_object_destroy (GTK_OBJECT (conduit));
}
