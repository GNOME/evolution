/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
  Test vfolder.
  */


#include <camel/camel.h>
#include <camel/camel-exception.h>
#include <camel/camel-folder.h>
#include <camel/providers/vee/camel-vee-folder.h>
#include <camel/md5-utils.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <glib.h>

static void
dump_message_content(CamelDataWrapper *object)
{
	CamelDataWrapper *containee;
	CamelStream *stream;
	int parts, i;
	int len;
	int left;
	char buffer[128];

	printf("Dumping message ...");

	containee = camel_medium_get_content_object(CAMEL_MEDIUM(object));

	if (containee) {
		char *type = gmime_content_field_get_mime_type(containee->mime_type);

		printf("type = %s\n", type);
		
		if (CAMEL_IS_MULTIPART(containee)) {
			parts = camel_multipart_get_number (CAMEL_MULTIPART(containee));
			printf("multipart message, scanning contents  %d parts ...\n", parts);
			for (i=0;i<parts;i++) {
				dump_message_content(CAMEL_DATA_WRAPPER (camel_multipart_get_part(CAMEL_MULTIPART(containee), i)));
			}
		} else if (CAMEL_IS_MIME_MESSAGE(containee)) {
			dump_message_content((CamelDataWrapper *)containee);
		} else {
			stream = camel_data_wrapper_get_output_stream(containee);
			left = 0;

			if (stream) {
				while ( (len = camel_stream_read(stream, buffer+left, sizeof(buffer)-left, NULL)) > 0) {
					fwrite(buffer, len, 1, stdout);
				}
				printf("\n");
			} else {
				g_warning("cannot get stream for message?");
			}
		}

		g_free(type);
	} else {
		printf("no containee?\n");
	}
}


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
	gchar *store_url = "vfolder:";
	CamelFolder *folder;
	GList *n, *matches;

	gtk_init (&argc, &argv);
	camel_init ();		
	ex = camel_exception_new ();

	session = camel_session_new (auth_callback);

	camel_provider_load (session, "../camel/providers/vee/.libs/libcamelvee.so.0", ex);
	if (camel_exception_get_id (ex)) {
		printf ("Exceptions suck: %s\n", camel_exception_get_description (ex));
		return 1;
	}

	store = camel_session_get_store (session, store_url, ex);
	if (camel_exception_get_id (ex)) {
		printf ("Exception caught in camel_session_get_store\n"
			"Full description : %s\n", camel_exception_get_description (ex));
		return -1;
	}

	printf("get folder\n");	

	folder = camel_store_get_folder (store, "gnome_email?(match-all (header-contains \"subject\" \"gnome\"))", ex);
	if (camel_exception_get_id (ex)) {
		printf ("Exception caught in camel_store_get_folder\n"
			"Full description : %s\n", camel_exception_get_description (ex));
		return -1;
	}

	/* setup searched folders */
	{
		CamelFolder *subfolder;
		CamelStore *substore;

		substore = camel_session_get_store (session, "mbox:///home/notzed/evolution/local/Inbox", ex);
		subfolder = camel_store_get_folder(substore, "mbox", ex);
		camel_folder_open (subfolder, FOLDER_OPEN_READ, ex);
		camel_vee_folder_add_folder(folder, subfolder);

		if (camel_exception_get_id (ex)) {
			printf ("Exception caught in camel_store_get_folder\n"
				"Full description : %s\n", camel_exception_get_description (ex));
			return -1;
		}

		substore = camel_session_get_store (session, "mbox:///home/notzed/evolution/local/Outbox", ex);
		subfolder = camel_store_get_folder(substore, "mbox", ex);
		camel_folder_open (subfolder, FOLDER_OPEN_READ, ex);
		camel_vee_folder_add_folder(folder, subfolder);

		if (camel_exception_get_id (ex)) {
			printf ("Exception caught in camel_store_get_folder\n"
				"Full description : %s\n", camel_exception_get_description (ex));
			return -1;
		}
	}
	
	printf("open folder\n");

	camel_folder_open (folder, FOLDER_OPEN_READ, ex);
	if (camel_exception_get_id (ex)) {
		printf ("Exception caught when trying to open the folder\n"
			"Full description : %s\n", camel_exception_get_description (ex));
		return -1;
	}

	printf("vfolder's uid's:\n");
	n = camel_folder_get_uid_list(folder, ex);
	while (n) {
		CamelMimeMessage *m;
		
		printf("uid: %s\n", (char *) n->data);

		m = camel_folder_get_message_by_uid(folder, n->data, ex);
		if (m) {
			dump_message_content(m);
			gtk_object_unref(m);
		}
		n = g_list_next(n);
	}

	camel_folder_close (folder, TRUE, ex);

	gtk_object_unref((GtkObject *)folder);

	return 0;
}




