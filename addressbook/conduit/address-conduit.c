/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#include <config.h>
#include <sys/stat.h>
#include <utime.h>
#include <unistd.h>
#include <pwd.h>
#include <signal.h>
#include <errno.h>

#include <pi-source.h>
#include <pi-socket.h>
#include <pi-file.h>
#include <pi-dlp.h>
#include <libgnorba/gnorba.h>
#include <libgnorba/gnome-factory.h>
#include <pi-version.h>
#include <gpilotd/gnome-pilot-conduit.h>
#include <gpilotd/gnome-pilot-conduit-standard-abs.h>
#include <address-conduit.h>
#include <libversit/vcc.h>
#include "ebook/e-book-types.h"

GnomePilotConduit * conduit_get_gpilot_conduit (guint32);
void conduit_destroy_gpilot_conduit (GnomePilotConduit*);
void local_record_from_ecard (AddressbookLocalRecord *local, ECard *ecard);

#define CONDUIT_VERSION "0.1"
#ifdef G_LOG_DOMAIN
#undef G_LOG_DOMAIN
#endif
#define G_LOG_DOMAIN "addressconduit" 

#define DEBUG_CALCONDUIT 1
/* #undef DEBUG_CALCONDUIT */

#ifdef DEBUG_ADDRESSBOOKCONDUIT
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


static void
status_cb (EBook *ebook, EBookStatus status, gpointer closure)
{
	(*(EBookStatus*)closure) = status;
	gtk_main_quit();
}


/* Destroys any data allocated by gcalconduit_load_configuration
   and deallocates the given configuration. */
static void 
conduit_destroy_configuration(AddressbookConduitCfg **c) 
{
	g_return_if_fail(c!=NULL);
	g_return_if_fail(*c!=NULL);
	//g_free(*c); FIX ME
	*c = NULL;
}


/* Given a AddressbookConduitContext**, allocates the structure */
static void
conduit_new_context(AddressbookConduitContext **ctxt,
		    AddressbookConduitCfg *c) 
{
	*ctxt = g_new0(AddressbookConduitContext,1);
	g_assert(ctxt!=NULL);
	(*ctxt)->cfg = c;
	CORBA_exception_init (&((*ctxt)->ev));
}


/* Destroys any data allocated by conduit_new_context
   and deallocates its data. */
static void
conduit_destroy_context(AddressbookConduitContext **ctxt)
{
	g_return_if_fail(ctxt!=NULL);
	g_return_if_fail(*ctxt!=NULL);

	if ((*ctxt)->cfg!=NULL)
		conduit_destroy_configuration(&((*ctxt)->cfg));

	g_free(*ctxt);
	*ctxt = NULL;
}


static void
cursor_cb (EBook *book, EBookStatus status, ECardCursor *cursor, gpointer closure)
{
	AddressbookConduitContext *ctxt = (AddressbookConduitContext*)closure;

	if (status == E_BOOK_STATUS_SUCCESS) {
		long length;
		int i;

		ctxt->cursor = cursor;
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
	AddressbookConduitContext *ctxt = (AddressbookConduitContext*)closure;

	if (status == E_BOOK_STATUS_SUCCESS) {
		e_book_get_cursor (book, "(contains \"full_name\" \"\")", cursor_cb, ctxt);
	}
	else {
		WARN (_("BLARG\n"));
		gtk_main_quit(); /* end the sub event loop */
	}
}

static int
start_address_server (GnomePilotConduitStandardAbs *conduit,
		      AddressbookConduitContext *ctxt)
{
	gchar *uri, *path;

	g_return_val_if_fail(conduit!=NULL,-2);
	g_return_val_if_fail(ctxt!=NULL,-2);

	ctxt->ebook = e_book_new ();

	path = g_concat_dir_and_file (g_get_home_dir (),
				      "evolution/local/Contacts");
	uri = g_strdup_printf ("file:%s", path);
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

/*
 * converts a ECard to a AddressbookLocalRecord
 */
void
local_record_from_ecard(AddressbookLocalRecord *local,
			ECard *ecard)
{
	g_return_if_fail(local!=NULL);
	g_return_if_fail(ecard!=NULL);

	local->ecard = ecard;
	local->local.ID = local->ecard->pilot_id;
#if 0
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
#endif

	local->local.attr = GnomePilotRecordNothing;

	/* Records without a pilot_id are new */
	if(local->local.ID == 0) 
		local->local.attr = GnomePilotRecordNew; 
  
	local->local.secret = 0;
#if 0
	if(obj->class!=NULL) 
		if(strcmp(obj->class,"PRIVATE")==0)
			local->local.secret = 1;
#endif
 
	local->local.archived = 0;  
}

static ECard *
get_ecard_by_pilot_id (GList *card_list, recordid_t id)
{
	GList *l;

	for (l = card_list; l; l = l->next) {
		guint32 pilot_id;
		ECard *card = l->data;

		if (!card)
			continue;

		gtk_object_get (GTK_OBJECT(card), "pilot_id", &pilot_id);

		if (pilot_id == id)
			return card;
	}

	return NULL;
}

/*
 * Given a PilotRecord, find the matching record in
 * the addressbook. If no match, return NULL
 */
static AddressbookLocalRecord *
find_record_in_ebook(GnomePilotConduitStandardAbs *conduit,
		     PilotRecord *remote,
		     AddressbookConduitContext *ctxt) 
{
	AddressbookLocalRecord *loc;
	ECard *ecard;
  
	g_return_val_if_fail(conduit!=NULL,NULL);
	g_return_val_if_fail(remote!=NULL,NULL);
  
	LOG ("requesting %ld", remote->ID);

	ecard = get_ecard_by_pilot_id (ctxt->cards, remote->ID);

	if (NULL != ecard) {
		LOG ("Found");
		loc = g_new0(AddressbookLocalRecord,1);
		/* memory allocated in new_from_string is freed in free_match */
		local_record_from_ecard (loc, ecard);
		return loc;
	}

	INFO ("Object did not exist");
	return NULL;
}

static ECard *
ecard_from_remote_record(GnomePilotConduitStandardAbs *conduit,
			 PilotRecord *remote)
{
	ECard *ecard;
	struct Address address;
	VObject *vobj;
	VObject *nameprop, *addressprop;
	char *temp;

	g_return_val_if_fail(remote!=NULL,NULL);
	memset (&address, 0, sizeof (struct Address));
	unpack_Address (&address, remote->record, remote->length);
	
	vobj = newVObject (VCCardProp);
	nameprop = addProp (vobj, VCNameProp);

#define ADD_PROP(v,pilotprop,vprop) \
	if (address.entry [(pilotprop)]) \
		addPropValue ((v), (vprop), address.entry [(pilotprop)])

	ADD_PROP (nameprop, entryFirstname, VCGivenNameProp);
	ADD_PROP (nameprop, entryLastname, VCFamilyNameProp);

	addressprop = addProp (vobj, VCAdrProp);

	ADD_PROP (addressprop, entryAddress, VCStreetAddressProp);
	ADD_PROP (addressprop, entryCity, VCCityProp);
	ADD_PROP (addressprop, entryState, VCRegionProp);
	ADD_PROP (addressprop, entryZip, VCPostalCodeProp);
	ADD_PROP (addressprop, entryCountry, VCCountryNameProp);

	ADD_PROP (vobj, entryTitle, VCTitleProp);

	if (address.entry [entryCompany]) {
		VObject *orgprop;
		orgprop = addProp (vobj, VCOrgProp);
		ADD_PROP (orgprop, entryCompany, VCOrgNameProp);
	}

	temp = writeMemVObject (NULL, NULL, vobj);
	ecard = e_card_new (temp);
	free (temp);
	cleanVObject (vobj);

	free_Address(&address);

	gtk_object_set (GTK_OBJECT(ecard), "pilot_id", remote->ID);

	return ecard;
}

static void
add_card_cb (EBook *ebook, EBookStatus status, const char *id, gpointer closure)
{
	(*(EBookStatus*)closure) = status;
	gtk_main_quit();
}

static gint
update_record (GnomePilotConduitStandardAbs *conduit,
	       PilotRecord *remote,
	       AddressbookConduitContext *ctxt)
{
	struct Address address;
	ECard *ecard;

	g_return_val_if_fail(remote!=NULL,-1);

	memset (&address, 0, sizeof (struct Address));
	unpack_Address (&address, remote->record, remote->length);

	LOG ("requesting %ld [%s %s]", remote->ID, address.entry[entryFirstname], address.entry[entryLastname]);
	printf ("requesting %ld [%s %s]\n", remote->ID, address.entry[entryFirstname], address.entry[entryLastname]);

	ecard = get_ecard_by_pilot_id (ctxt->cards, remote->ID);

	if (ecard == NULL) {
		EBookStatus ebook_status;

		LOG ("Object did not exist, creating a new one");
		printf ("Object did not exist, creating a new one\n");

		ecard = ecard_from_remote_record (conduit, remote);

		/* add the ecard to the server */
		e_book_add_card (ctxt->ebook, ecard, add_card_cb, &ebook_status);
		gtk_main(); /* enter sub mainloop */

		if (ebook_status == E_BOOK_STATUS_SUCCESS)
			ctxt->cards = g_list_append (ctxt->cards, ecard);
		else
			WARN ("update_record: failed to add card to ebook\n");
	} else {
#if 0
		/* XXX toshok: need to figure out exactly what should
                   be done here - default to the existing ecard and
                   overwrite with information from the pilot? */
		iCalObject *new_obj;
		LOG ("Found");
		printf ("Found\n");
		new_obj = ecard_from_remote_record (conduit, remote, obj);
		obj = new_obj;
#endif
	}

#if 0
	/* update record on server */
	update_ecard (conduit, ecard, ctxt);
#endif
	free_Address(&address);

	return 0;
}

static void
check_for_slow_setting (GnomePilotConduit *c, AddressbookConduitContext *ctxt)
{
#if 0
	GList *uids;
	unsigned long int entry_number;

	uids = cal_client_get_uids (ctxt->client, CALOBJ_TYPE_ADDRESS);

	entry_number = g_list_length (uids);

	LOG (_("Address holds %ld address entries"), entry_number);
	/* If the local base is empty, do a slow sync */
	if (entry_number == 0) {
		GnomePilotConduitStandard *conduit;
		conduit = GNOME_PILOT_CONDUIT_STANDARD (c);
		gnome_pilot_conduit_standard_set_slow (conduit);
	}
#endif /* 0 */
}

static gint
pre_sync (GnomePilotConduit *c,
	  GnomePilotDBInfo *dbi,
	  AddressbookConduitContext *ctxt)
{
	int l;
	unsigned char *buf;
	GnomePilotConduitStandardAbs *conduit;

	conduit = GNOME_PILOT_CONDUIT_STANDARD_ABS(c);
  
	g_message ("Evolution Addressbook Conduit v.%s",CONDUIT_VERSION);

	ctxt->ebook = NULL;
	
	if (start_address_server (GNOME_PILOT_CONDUIT_STANDARD_ABS(c), ctxt) != 0) {
		WARN(_("Could not start addressbook server"));
		gnome_pilot_conduit_error(GNOME_PILOT_CONDUIT(c),
					  _("Could not start addressbook server"));
		return -1;
	}


	/* Set the counters for the progress bar crap */

	gtk_object_set_data(GTK_OBJECT(c),"dbinfo",dbi);
  
	/* load_records(c); */

	buf = (unsigned char*)g_malloc(0xffff);
	if((l=dlp_ReadAppBlock(dbi->pilot_socket,dbi->db_handle,0,(unsigned char *)buf,0xffff)) < 0) {
		WARN(_("Could not read pilot's Address application block"));
		WARN("dlp_ReadAppBlock(...) = %d",l);
		gnome_pilot_conduit_error(GNOME_PILOT_CONDUIT(c),
			     _("Could not read pilot's Address application block"));
		return -1;
	}
	unpack_AddressAppInfo(&(ctxt->ai),buf,l);
	g_free(buf);

#if 0
	check_for_slow_setting(c,ctxt);
#else
	/* for now just always use the slow sync method */
	gnome_pilot_conduit_standard_set_slow (GNOME_PILOT_CONDUIT_STANDARD (c));
#endif

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
		 AddressbookLocalRecord **local,
		 PilotRecord *remote,
		 AddressbookConduitContext *ctxt)
{
	LOG ("in match_record");

	g_return_val_if_fail(local!=NULL,-1);
	g_return_val_if_fail(remote!=NULL,-1);

	*local = find_record_in_ebook(conduit,remote,ctxt);
  
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
		 AddressbookLocalRecord **local,
		 AddressbookConduitContext *ctxt)
{
	LOG ("entering free_match");

	g_return_val_if_fail(local!=NULL,-1);
	g_return_val_if_fail(*local!=NULL,-1);

	gtk_object_unref (GTK_OBJECT((*local)->ecard));
	g_free(*local);
	
        *local = NULL;
	return 0;
}

/*
  Move to archive and set status to Nothing
 */
static gint
archive_local (GnomePilotConduitStandardAbs *conduit,
	       AddressbookLocalRecord *local,
	       AddressbookConduitContext *ctxt)
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
		AddressbookLocalRecord *local,
		PilotRecord *remote,
		AddressbookConduitContext *ctxt)
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
	      AddressbookConduitContext *ctxt)
{
	LOG ("entering store_remote");

	g_return_val_if_fail(remote!=NULL,-1);
	remote->attr = GnomePilotRecordNothing;

	return update_record(conduit,remote,ctxt);
}

static gint
clear_status_archive_local (GnomePilotConduitStandardAbs *conduit,
			    AddressbookLocalRecord *local,
			    AddressbookConduitContext *ctxt)
{
	LOG ("entering clear_status_archive_local");

	g_return_val_if_fail(local!=NULL,-1);

        return -1;
}

static gint
iterate (GnomePilotConduitStandardAbs *conduit,
	 AddressbookLocalRecord **local,
	 AddressbookConduitContext *ctxt)
{
	static GList *iterator;
	static int num;

	g_return_val_if_fail(local!=NULL,-1);

	if (*local==NULL) {
		LOG ("beginning iteration");

		iterator = ctxt->cards;
		num = 0;
		
		LOG ("iterating over %d records", g_list_length (ctxt->cards));
		*local = g_new0(AddressbookLocalRecord, 1);
		local_record_from_ecard (*local, (ECard*)iterator->data);
	} else {
		/* printf ("continuing iteration\n"); */
		num++;
		if(g_list_next(iterator)==NULL) {
			LOG ("ending");
			/** free stuff allocated for iteration */
			g_free((*local));

			LOG ("iterated over %d records", num);

			/* ends iteration */
			(*local) = NULL;
			return 0;
		} else {
			iterator = g_list_next (iterator);
			local_record_from_ecard (*local,(ECard*)(iterator->data));
		}
	}
	return 1;
}


static gint
iterate_specific (GnomePilotConduitStandardAbs *conduit,
		  AddressbookLocalRecord **local,
		  gint flag,
		  gint archived,
		  AddressbookConduitContext *ctxt)
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
       AddressbookConduitContext *ctxt)
{
	LOG ("entering purge");

	/* HEST, gem posterne her */

	return -1;
}


static gint
set_status (GnomePilotConduitStandardAbs *conduit,
	    AddressbookLocalRecord *local,
	    gint status,
	    AddressbookConduitContext *ctxt)
{
#if 0
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
		WARN (_("Error while communicating with address server"));
	}
#endif
	
        return 0;
}

static gint
set_archived (GnomePilotConduitStandardAbs *conduit,
	      AddressbookLocalRecord *local,
	      gint archived,
	      AddressbookConduitContext *ctxt)
{
#if 0
	LOG ("entering set_archived");

	g_return_val_if_fail(local!=NULL,-1);
	g_assert(local->ecard!=NULL);

	local->local.archived = archived;
	update_address_entry_in_repository (conduit, local->ical, ctxt);
#endif
        return 0;
}

static gint
set_pilot_id (GnomePilotConduitStandardAbs *conduit,
	      AddressbookLocalRecord *local,
	      guint32 ID,
	      AddressbookConduitContext *ctxt)
{
	EBookStatus commit_status;

	LOG ("entering set_pilot_id(id=%d)",ID);

	g_return_val_if_fail(local!=NULL,-1);
	g_assert(local->ecard!=NULL);

	local->local.ID = ID;

	gtk_object_set (GTK_OBJECT(local->ecard), "pilot_id", local->local.ID);
	e_book_commit_card (ctxt->ebook, local->ecard, status_cb, &commit_status);

	if (commit_status == E_BOOK_STATUS_SUCCESS) {
		return 0;
	}
	else {
		WARN ("set_pilot_id failed.\n");
		return -1;
	}
}

static gint
transmit (GnomePilotConduitStandardAbs *conduit,
	  AddressbookLocalRecord *local,
	  PilotRecord **remote,
	  AddressbookConduitContext *ctxt)
{
	PilotRecord *p;
	
	LOG ("entering transmit");

	g_return_val_if_fail(local!=NULL,-1);
	g_return_val_if_fail(remote!=NULL,-1);
	g_assert(local->ecard!=NULL);

	p = g_new0(PilotRecord,1);

	p->ID = local->local.ID;
	p->attr = local->local.attr;
	p->archived = local->local.archived;
	p->secret = local->local.secret;

	local->address = g_new0(struct Address,1);

#if 0
	printf ("transmitting address to pilot [%s] complete=%d/%ld\n",
		local->ical->summary==NULL?"NULL":local->ical->summary,
		local->address->complete, local->ical->completed);
#endif

	/* Generate pilot record structure */
	p->record = g_new0(char,0xffff);
	p->length = pack_Address(local->address,p->record,0xffff);

	*remote = p;

	return 0;
}

static gint
free_transmit (GnomePilotConduitStandardAbs *conduit,
	       AddressbookLocalRecord *local,
	       PilotRecord **remote,
	       AddressbookConduitContext *ctxt)
{
	LOG ("entering free_transmit");

	g_return_val_if_fail(local!=NULL,-1);
	g_return_val_if_fail(remote!=NULL,-1);

	free_Address(local->address);
	g_free((*remote)->record);
	*remote = NULL;
        return 0;
}

static gint
compare (GnomePilotConduitStandardAbs *conduit,
	 AddressbookLocalRecord *local,
	 PilotRecord *remote,
	 AddressbookConduitContext *ctxt)
{
#if 0
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
				struct Address foolocal;
				struct Address fooremote;

				unpack_Address (&foolocal,
					     remoteOfLocal->record,
					     remoteOfLocal->length);
				unpack_Address (&fooremote,
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
#endif /* 0 */
	return 0;
}

static gint
compare_backup (GnomePilotConduitStandardAbs *conduit,
		AddressbookLocalRecord *local,
		PilotRecord *remote,
		AddressbookConduitContext *ctxt)
{
	LOG ("entering compare_backup");

	g_return_val_if_fail(local!=NULL,-1);
	g_return_val_if_fail(remote!=NULL,-1);

        return -1;
}

static gint
delete_all (GnomePilotConduitStandardAbs *conduit,
	    AddressbookConduitContext *ctxt)
{
	GList *it;

	for (it=ctxt->cards; it; it = g_list_next (it)) {
		EBookStatus remove_status;
		e_book_remove_card (ctxt->ebook, it->data, status_cb, &remove_status);
		gtk_main (); /* enter sub main loop */

		if (E_BOOK_STATUS_SUCCESS != remove_status)
			WARN ("Failed to remove ecard");

		gtk_object_unref (it->data);
	}

	g_list_free (ctxt->cards);
	ctxt->cards = NULL;
        return -1;
}


GnomePilotConduit *
conduit_get_gpilot_conduit (guint32 pilotId)
{
	GtkObject *retval;
	AddressbookConduitCfg *cfg;
	AddressbookConduitContext *ctxt;

	printf ("in address's conduit_get_gpilot_conduit\n");

	retval = gnome_pilot_conduit_standard_abs_new ("AddressDB",
						       0x61646472);

	g_assert (retval != NULL);
	gnome_pilot_conduit_construct(GNOME_PILOT_CONDUIT(retval),"AddressConduit");

	conduit_load_configuration(&cfg,pilotId);
	gtk_object_set_data(retval,"addressconduit_cfg",cfg);

	conduit_new_context(&ctxt,cfg);
	gtk_object_set_data(GTK_OBJECT(retval),"addressconduit_context",ctxt);

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
        AddressbookConduitCfg *cc;
	AddressbookConduitContext *ctxt;

        cc = GET_CONDUITCFG(conduit);
	ctxt = GET_CONDUITCONTEXT(conduit);

	if (ctxt->ebook != NULL) {
		gtk_object_unref (GTK_OBJECT (ctxt->ebook));
	}

        conduit_destroy_configuration (&cc);

	conduit_destroy_context (&ctxt);

	gtk_object_destroy (GTK_OBJECT (conduit));
}
