/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
  Test search api
  */


#include "camel.h"
#include "camel-mbox-folder.h"
#include "camel-mbox-parser.h"
#include "camel-mbox-utils.h"
#include "camel-mbox-summary.h"
#include "camel-log.h"
#include "camel-exception.h"
#include "camel-folder-summary.h"
#include "md5-utils.h"
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <glib.h>

int
main (int argc, char**argv)
{
	CamelSession *session;
	CamelException *ex;
	CamelStore *store;
	gchar *store_url = "mbox:///tmp/evmail";
	CamelFolder *folder;
	CamelMimeMessage *message;
	GList *uid_list;
	int camel_debug_level = 10;
	GList *matches;

	gtk_init (&argc, &argv);
	camel_init ();		
	ex = camel_exception_new ();
	camel_provider_register_as_module ("../camel/providers/mbox/.libs/libcamelmbox.so.0");
	
	session = camel_session_new ();
	store = camel_session_get_store (session, store_url, ex);
	if (camel_exception_get_id (ex)) {
		printf ("Exception caught in camel_session_get_store\n"
			"Full description : %s\n", camel_exception_get_description (ex));
		return -1;
	}

	printf("get folder\n");	

	folder = camel_store_get_folder (store, "Inbox", ex);
	if (camel_exception_get_id (ex)) {
		printf ("Exception caught in camel_store_get_folder\n"
			"Full description : %s\n", camel_exception_get_description (ex));
		return -1;
	}

	printf("open folder\n");

	camel_folder_open (folder, FOLDER_OPEN_READ, ex);
	if (camel_exception_get_id (ex)) {
		printf ("Exception caught when trying to open the folder\n"
			"Full description : %s\n", camel_exception_get_description (ex));
		return -1;
	}

	
	printf("Search for messages\n");

	matches = camel_folder_search_by_expression  (folder, "(or (match-all (header-contains \"subject\" \"term\")) (match-all (header-contains \"subject\" \"gnome\")))", ex);

	if (matches) {
		GList *n;
		printf("search found matches:\n");
		n = matches;
		while (n) {
			printf("uid: %s\n", n->data);
			n = g_list_next(n);
		}
		
	} else {
		printf("no matches?\n");
	}

	camel_folder_close (folder, FALSE, ex);	

	return 0;
}




