/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */


#include "camel-mbox-folder.h"
#include "camel-mbox-parser.h"
#include "camel-mbox-utils.h"
#include "camel.h"
#include "camel-log.h"
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

	camel_debug_level = CAMEL_LOG_LEVEL_FULL_DEBUG;

	gtk_init (&argc, &argv);
	camel_init ();	
	
	ex = camel_exception_new ();
	test_file_fd = open (argv[1], O_RDONLY);
	message_info_array = camel_mbox_parse_file (test_file_fd, 
						   "From ", 
						   0,
						   TRUE,
						   NULL,
						   0,
						   ex); 
	
	close (test_file_fd);
	camel_mbox_write_xev (argv[1], message_info_array, 0, ex);
	if (camel_exception_get_id (ex)) { 
		printf ("Exception caught in camel_mbox_write_xev : %s\n", camel_exception_get_description (ex));
	}
	

	
	
	
}



