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

struct search_data {
	CamelFolder *folder;
	CamelFolder *outbox;
	CamelException *ex;
};

static void
search_cb(CamelFolder *folder, int id, gboolean complete, GList *matches, struct search_data *sd)
{
	GList *n;
	printf("search found matches:\n");
	n = matches;
	while (n) {
		CamelMimeMessage *m;
		
		printf("uid: %s\n", (char *) n->data);
		m = camel_folder_get_message_by_uid(sd->folder, n->data, sd->ex);
		
		if (camel_exception_get_id (sd->ex)) {
			printf ("Cannot get message\n"
				"Full description : %s\n", camel_exception_get_description (sd->ex));
		} else {
		
			camel_folder_append_message(sd->outbox, m, sd->ex);
			
			if (camel_exception_get_id (sd->ex)) {
				printf ("Cannot save message\n"
					"Full description : %s\n", camel_exception_get_description (sd->ex));
			}
		}
		n = g_list_next(n);
	}

	if (complete) {
		camel_folder_close (sd->folder, FALSE, sd->ex);
		gtk_exit(0);
	}
}

int
main (int argc, char**argv)
{
	CamelSession *session;
	CamelException *ex;
	CamelStore *store;
	gchar *store_url = "mbox:///tmp/evmail";
	CamelFolder *folder, *outbox;
	struct search_data *sd;

	gtk_init (&argc, &argv);
	camel_init ();		
	ex = camel_exception_new ();

	sd = g_malloc0(sizeof(*sd));
	sd->ex = ex;

	session = camel_session_new (auth_callback);
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

	sd->folder = folder;
	sd->outbox = outbox;

	printf("Search for messages\n");

	camel_folder_search_by_expression  (folder,
					    "(match-all (header-contains \"subject\" \"gnome\"))",
					    (CamelSearchFunc*)search_cb,
					    sd,
					    ex);

	gtk_main();

	return 0;
}




