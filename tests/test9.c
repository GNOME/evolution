/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#include "camel.h"
#include "camel-mbox-folder.h"
#include "camel-mbox-parser.h"
#include "camel-mbox-utils.h"
#include "camel-mbox-summary.h"
#include "camel-exception.h"
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
	GArray *message_info_array;
	gint test_file_fd;
	CamelException *ex;
	CamelMboxSummary *sum1, *sum2;
	GArray *mbox_summary_info;
	CamelMboxSummaryInformation *msg_info;
	int i;
	guint32 next_uid;
	guint32 mbox_file_size;

	gtk_init (&argc, &argv);
	camel_init ();	
	
	ex = camel_exception_new ();
	test_file_fd = open (argv[1], O_RDONLY);
	message_info_array = camel_mbox_parse_file (test_file_fd, 
						    "From ", 
						    0,
						    &mbox_file_size,
						    &next_uid,
						    TRUE,
						    NULL,
						    0,
						    ex); 
	
	close (test_file_fd);
#warning This test is no longer valid.
#if 0
	/* needs a folder to work with (indexing) */
	camel_mbox_write_xev (argv[1], message_info_array, &mbox_file_size, 1, ex);
#endif
	if (camel_exception_get_id (ex)) { 
		printf ("Exception caught in camel_mbox_write_xev : %s\n", camel_exception_get_description (ex));
	}
	
	
	mbox_summary_info =
		parsed_information_to_mbox_summary (message_info_array);
	sum1 = CAMEL_MBOX_SUMMARY (gtk_object_new (camel_mbox_summary_get_type (), NULL));

	sum1->nb_message = mbox_summary_info->len;

	sum1->message_info = mbox_summary_info;

	camel_mbox_summary_save (sum1, "ev-summary.mbox", ex);

	sum2 = camel_mbox_summary_load ("ev-summary.mbox", ex);

	for (i=0; i<sum1->nb_message; i++) {
		
		msg_info = (CamelMboxSummaryInformation *)(sum1->message_info->data) + i;
		printf ("Message %d :\n"
			"  From : %s\n", i, msg_info->headers.sender);
	}

	return 1;
	
}



