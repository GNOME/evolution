/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* tests mime message file parsing */
#include "gmime-utils.h"
#include "stdio.h"
#include "camel-log.h"
#include "camel-mime-message.h"
#include "camel-mime-part.h"
#include "camel-stream.h"
#include "camel-stream-fs.h"

void print_header_pair (gpointer key, gpointer value, gpointer user_data)
{
	GString *header_name = (GString *)key;
	GString *header_value = (GString *)value;
	CamelMimeMessage *message = (CamelMimeMessage *) user_data;


	printf("\n--------- New Header ----------\n");
	if  ((header_name) && (header_name->str))
		printf("header name :%s\n", header_name->str);
	if  ((header_value) && (header_value->str))
		printf("header value :%s\n", header_value->str);

	camel_mime_part_add_header ( CAMEL_MIME_PART (message), header_name, header_value);

	printf("--------- End -----------------\n"); 
	
}

void
main (int argc, char**argv)
{
	GHashTable *header_table;
	CamelMimeMessage *message;
	CamelStream *input_stream, *output_stream;
	
	gtk_init (&argc, &argv);
	camel_debug_level = FULL_DEBUG;
	message = camel_mime_message_new_with_session( (CamelSession *)NULL);

	
	input_stream = camel_stream_fs_new_with_name (g_string_new ("mail1.test"), CAMEL_STREAM_FS_READ);
	if (!input_stream) {
		perror("could not open input file");
		exit(2);
	}
	
	header_table = get_header_table_from_stream (input_stream);

	if (header_table) g_hash_table_foreach (header_table, print_header_pair, (gpointer)message);
	else printf("header is empty, no header line present\n");

	camel_stream_close (input_stream);

	output_stream = camel_stream_fs_new_with_name (g_string_new ("mail2.test"), CAMEL_STREAM_FS_WRITE);
	camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (message), output_stream);
	camel_stream_close (output_stream);

	

}
