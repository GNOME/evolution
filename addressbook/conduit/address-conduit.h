/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#ifndef __ADDRESS_CONDUIT_H__
#define __ADDRESS_CONDUIT_H__

#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <gnome.h>
#include <pi-address.h>
#include <gpilotd/gnome-pilot-conduit.h>
#include <gpilotd/gnome-pilot-conduit-standard-abs.h>
#include "ebook/e-card.h"
#include "ebook/e-book.h"
#include "ebook/e-book-view.h"

#ifdef USING_OAF
#include <liboaf/liboaf.h>
#else
#include <libgnorba/gnorba.h>
#endif


/* This is the local record structure for the GnomeCal conduit. */
typedef struct _AddressbookLocalRecord AddressbookLocalRecord;
struct _AddressbookLocalRecord {
	/* The stuff from gnome-pilot-conduit-standard-abs.h
	   Must be first in the structure, or instances of this
	   structure cannot be used by gnome-pilot-conduit-standard-abs.
	*/
	LocalRecord local;
	/* The corresponding Evolution addressbook object. */
	ECard *ecard;
        /* pilot-link address structure, used for implementing Transmit. */	
	struct Address *address;
};
#define ADDRESSBOOK_LOCALRECORD(s) ((AddressbookLocalRecord*)(s))

/* This is the configuration of the GnomeCal conduit. */
typedef struct _AddressbookConduitCfg AddressbookConduitCfg;
struct _AddressbookConduitCfg {
	gboolean open_secret;
	guint32 pilotId;
	GnomePilotConduitSyncType  sync_type;   /* only used by capplet */
};
#define GET_CONDUITCFG(c) ((AddressbookConduitCfg*)gtk_object_get_data(GTK_OBJECT(c),"addressconduit_cfg"))

/* This is the context for all the Addressbook conduit methods. */
typedef struct _AddressbookConduitContext AddressbookConduitContext;
struct _AddressbookConduitContext {
	struct AddressAppInfo ai;
	AddressbookConduitCfg *cfg;
	EBook *ebook;
	ECardCursor *cursor;
	GList *cards;
	/*	CalClient *client;*/
	CORBA_Environment ev;
	CORBA_ORB orb;
	gboolean address_load_tried;
	gboolean address_load_success;

	char *address_file;
};
#define GET_CONDUITCONTEXT(c) ((AddressbookConduitContext*)gtk_object_get_data(GTK_OBJECT(c),"addressconduit_context"))


/* Given a GCalConduitCfg*, allocates the structure and 
   loads the configuration data for the given pilot.
   this is defined in the header file because it is used by
   both address-conduit and address-conduit-control-applet,
   and we don't want to export any symbols we don't have to. */
static void 
conduit_load_configuration(AddressbookConduitCfg **c,
			   guint32 pilotId) 
{
	gchar prefix[256];
	g_snprintf(prefix,255,"/gnome-pilot.d/address-conduit/Pilot_%u/",pilotId);
	
	*c = g_new0(AddressbookConduitCfg,1);
	g_assert(*c != NULL);
	gnome_config_push_prefix(prefix);
	(*c)->open_secret = gnome_config_get_bool("open_secret=FALSE");
	(*c)->sync_type = GnomePilotConduitSyncTypeCustom; /* set in capplets main */
	gnome_config_pop_prefix();
	
	(*c)->pilotId = pilotId;
}


#endif __ADDRESS_CONDUIT_H__ 
