/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* testing mh providers 
   do not use CamelMhFolder and CamelMhStore directly.
   We do it here for test purpose only */



#include "camel-folder.h"
#include "camel-mh-folder.h"
#include "camel-mh-store.h"
#include "camel.h"
#include "camel-log.h"

int
main (int argc, char**argv)
{
	CamelStore *store;
	CamelFolder *inbox_folder;
	CamelFolder *root_mh_folder;
	GList *mh_subfolders_name;
	CamelMimeMessage *message_1;
	gboolean inbox_exists;

	camel_debug_level = CAMEL_LOG_LEVEL_FULL_DEBUG;

	gtk_init (&argc, &argv);
	camel_init ();

	
	store = gtk_type_new (CAMEL_MH_STORE_TYPE);
	camel_store_init (store, (CamelSession *)NULL, g_strdup ("mh:///root/Mail"));
	
	root_mh_folder = camel_store_get_folder (store, "");
	mh_subfolders_name = camel_folder_list_subfolders (root_mh_folder);
	
	printf ("\n------------- Listing root Mh folder subfolders --------\n");
	while (mh_subfolders_name) {
		printf ("\t\"%s\"\n", mh_subfolders_name->data);
		mh_subfolders_name = mh_subfolders_name->next;
	}
	printf ("--------------------------------------------------------\n\n");

	inbox_folder = camel_store_get_folder (store, "inbox");
	if (!inbox_folder) {
		printf ("** Error: could not get inbox folder from store\n");
		return;
	}
	
	/* test existence */
	inbox_exists = camel_folder_exists (inbox_folder);
	if (inbox_exists)
		printf ("MH folder inbox exists, continuing tests\n");
	else {
		printf ("MH folder inbox does not exist. Stopping \n");
		return;
	}
	
	printf ("\n  Inbox folder contains %d messages\n", camel_folder_get_message_count (inbox_folder));
	printf ("\n------------- Gettting message numer 1 in inbox --------\n");
	message_1 = camel_folder_get_message (inbox_folder, 1);
	printf ("--------------------------------------------------------\n\n");




	
}
