/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* test provider stuff */


#include "camel.h"


int
main (int argc, char**argv)
{
	const CamelProvider *new_provider;

	camel_debug_level = CAMEL_LOG_LEVEL_FULL_DEBUG;

	gtk_init (&argc, &argv);
	camel_init ();


	new_provider = camel_provider_register_as_module ("../camel/providers/MH/.libs/libcamelmh.so");

	
	return 1;
}
