/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */


#include "camel.h"
#include "camel-mbox-folder.h"
#include "camel-mbox-parser.h"
#include "camel-mbox-utils.h"
#include "camel-mbox-summary.h"
#include "camel-log.h"
#include "camel-exception.h"
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


	camel_debug_level = 10;

	gtk_init (&argc, &argv);
	camel_init ();		
	ex = camel_exception_new ();
	camel_provider_register_as_module ("../camel/providers/mbox/.libs/libcamelmbox.so");
	
	session = camel_session_new ();
	store = camel_session_get_store (session, store_url);
	

	folder = camel_store_get_folder (store, "Inbox", ex);
	if (camel_exception_get_id (ex)) {
		printf ("Exception caughy in camel_store_get_folder"
			"Full description : %s\n", camel_exception_get_description (ex));
	}
	camel_folder_open (folder, FOLDER_OPEN_RW, ex);
	
	camel_folder_close (folder, FALSE, ex);

	return 1;
}  



