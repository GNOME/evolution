/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Based on 'test2.c'; tests the camel-formatter class */
#include "gmime-utils.h"
#include "stdio.h"
#include "camel-log.h"
#include "camel-mime-message.h"
#include "camel-mime-part.h"
#include "camel-stream.h"
#include "camel-stream-fs.h"
#include "camel.h"
#include "camel-formatter.h"

static void
convert_to_html_and_print (CamelMimeMessage *msg)
{
	CamelFormatter* cmf = camel_formatter_new();
	CamelStream* stream = camel_stream_mem_new (CAMEL_STREAM_FS_WRITE);
	camel_formatter_mime_message_to_html (
		cmf, msg, stream);
	g_print ("Parsed message follows\n----------------------\n%s",
		 (CAMEL_STREAM_MEM(stream))->buffer->data);
}

int
main (int argc, char**argv)
{
	GHashTable *header_table;
	CamelMimeMessage *message;
	CamelStream *input_stream;
	
	
	gtk_init (&argc, &argv);
	camel_init ();
	camel_debug_level = CAMEL_LOG_LEVEL_FULL_DEBUG;
		
	message = camel_mime_message_new_with_session( (CamelSession *)NULL);
	
	input_stream = camel_stream_fs_new_with_name (
		argv[1], CAMEL_STREAM_FS_READ);

	if (!input_stream) {
		perror ("could not open input file");
		printf ("You must create the file mail.test before running this test");
		exit(2);
	}
	
	camel_data_wrapper_construct_from_stream ( CAMEL_DATA_WRAPPER (message), input_stream);

	camel_debug_level = CAMEL_LOG_LEVEL_FULL_DEBUG;
	convert_to_html_and_print (message);	

	camel_stream_close (input_stream);
	gtk_object_unref (GTK_OBJECT (input_stream));

	return 0;
}
