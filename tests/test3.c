/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#include "camel.h"

int
main (int argc, char**argv)
{
	GtkType type;
	
	gtk_init (&argc, &argv);
	camel_init ();
	
	printf ("Test 3 : data wrapper repository\n");

	printf ("\nMime type : \"multipart\"\n");
	type = data_wrapper_get_data_wrapper_type ("multipart");
	printf ("Type found %s\n", gtk_type_name (type) );
	
	printf ("\nMime type : \"multipart/alternative\"\n");
	type = data_wrapper_get_data_wrapper_type ("multipart/alternative");
	printf ("Type found %s\n", gtk_type_name (type) );
	
	printf ("\nMime type : \"toto/titi\"\n");
	type = data_wrapper_get_data_wrapper_type ("toto/titi");
	printf ("Type found %s\n", gtk_type_name (type) );
	
	printf ("Test3 finished\n");
}
 
