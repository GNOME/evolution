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
	GArray *message_info_array;
	gint test_file_fd;
	CamelException *ex;
	CamelMboxSummary *sum1, *sum2;
	GArray *mbox_summary_info;
	CamelMboxSummaryInformation *msg_info;
	int i;
	guint32 next_uid;
	guint32 mbox_file_size;

	//camel_debug_level = CAMEL_LOG_LEVEL_FULL_DEBUG;
	camel_debug_level = 0;
	
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
	camel_mbox_write_xev (argv[1], message_info_array, &mbox_file_size, 1, ex);
	if (camel_exception_get_id (ex)) { 
		printf ("Exception caught in camel_mbox_write_xev : %s\n", camel_exception_get_description (ex));
	}
	
	
	mbox_summary_info =
		parsed_information_to_mbox_summary (message_info_array);
	sum1 = g_new (CamelMboxSummary, 1);

	md5_get_digest_from_file (argv[1], sum1->md5_digest);
	sum1->nb_message = mbox_summary_info->len;

	sum1->message_info = mbox_summary_info;

	camel_mbox_save_summary (sum1, "ev-summary.mbox", ex);

	sum2 = camel_mbox_load_summary ("ev-summary.mbox", ex);

	for (i=0; i<sum1->nb_message; i++) {
		
		msg_info = (CamelMboxSummaryInformation *)(sum1->message_info->data) + i;
		printf ("Message %d :\n"
			"  From : %s\n", i, msg_info->sender);
	}

	printf ("Taille du fichier mbox : %ld\n", mbox_file_size);
	printf ("\t in the summary : %ld\n", sum1->mbox_file_size  );

	return 1;
	
}



