/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
  Test search api
  */


#include <camel/camel.h>
#include <camel/camel-exception.h>
#include <camel/camel-folder.h>
#include <camel/md5-utils.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <glib.h>

static char *
auth_callback(char *prompt, gboolean secret,
	      CamelService *service, char *item,
	      CamelException *ex)
{
	printf ("auth_callback called: %s\n", prompt);
	return NULL;
}

int
main (int argc, char**argv)
{
	CamelSession *session;
	CamelException *ex;
	CamelStore *store;
	gchar *store_url = "mbox:///tmp/evmail";
	CamelFolder *folder, *outbox;
	GList *n, *matches;

	gtk_init (&argc, &argv);
	camel_init ();		
	ex = camel_exception_new ();

	session = camel_session_new (auth_callback);

	camel_provider_load (session, "../camel/providers/mbox/.libs/libcamelmbox.so.0", ex);
	if (camel_exception_get_id (ex)) {
		printf ("Exceptions suck\n");
		return 1;
	}

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

	printf("create output folder ...\n");
	outbox = camel_store_get_folder (store, "Gnome", ex);
	if (!camel_folder_exists(outbox, ex)) {
		camel_folder_create(outbox, ex);
	}
	
	camel_folder_open (outbox, FOLDER_OPEN_WRITE, ex);

	printf("Search for messages\n");

	matches = camel_folder_search_by_expression  (folder,
						      "(match-all (header-contains \"subject\" \"gnome\"))",
						      ex);

	printf("search found matches:\n");
	n = matches;
	while (n) {
		CamelMimeMessage *m;
		
		printf("uid: %s\n", (char *) n->data);
		m = camel_folder_get_message_by_uid(folder, n->data, ex);
		
		if (camel_exception_get_id (ex)) {
			printf ("Cannot get message\n"
				"Full description : %s\n", camel_exception_get_description (ex));
		} else {
		
			camel_folder_append_message(outbox, m, ex);
			
			if (camel_exception_get_id (ex)) {
				printf ("Cannot save message\n"
					"Full description : %s\n", camel_exception_get_description (ex));
			}

			printf("Removing matching message from source folder?\n");
			camel_mime_message_set_flags(m, CAMEL_MESSAGE_DELETED, CAMEL_MESSAGE_DELETED);
/*			camel_mime_message_set_flags(m, CAMEL_MESSAGE_ANSWERED, CAMEL_MESSAGE_ANSWERED);*/
		}
		n = g_list_next(n);
	}

	camel_folder_close (folder, TRUE, ex);

	return 0;
}




