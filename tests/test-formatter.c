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
	gchar* header_str;
	gchar* body_str;
	
	CamelStream* header_stream =
		camel_stream_mem_new (CAMEL_STREAM_FS_WRITE);	
	CamelStream* body_stream =
		camel_stream_mem_new (CAMEL_STREAM_FS_WRITE);
	camel_formatter_mime_message_to_html (
		cmf, msg, header_stream, body_stream);

	header_str = g_strndup (
		CAMEL_STREAM_MEM (header_stream)->buffer->data,
		CAMEL_STREAM_MEM (header_stream)->buffer->len);
	body_str = g_strndup (
		CAMEL_STREAM_MEM (body_stream)->buffer->data,
		CAMEL_STREAM_MEM (body_stream)->buffer->len);	
	g_print ("Header follows\n----------------------\n%s\n",
		 header_str);
	g_print ("Body follows\n----------------------\n%s\n",
		 body_str);

	g_free (header_str);
	g_free (body_str);
}

static void
print_usage_and_quit()
{
	g_print ("\nUsage: test-formatter [MIME-MESSAGE-FILE]\n\n");
	g_print ("Where MIME-MESSAGE-FILE is in the \"message/rfc822\"\n");
	g_print ("mime format.\n\n");
	
	exit(0);
}


int
main (int argc, char**argv)
{
	GHashTable *header_table;
	CamelMimeMessage *message;
	CamelStream *input_stream;
	
//	CamelStream *foo = CAMEL_STREAM(NULL);
	
	gtk_init (&argc, &argv);
	if (argc == 1 || argc > 2)
		print_usage_and_quit();
	
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

	camel_data_wrapper_construct_from_stream (
		CAMEL_DATA_WRAPPER (message), input_stream);

	camel_debug_level = CAMEL_LOG_LEVEL_FULL_DEBUG;
	convert_to_html_and_print (message);	


	camel_stream_close (input_stream);
	gtk_object_unref (GTK_OBJECT (input_stream));

	return 0;
}
