
#include "ebook/e-book.h"
#include <libgnome/gnome-init.h>
#include <bonobo/bonobo-main.h>
#include <stdlib.h>

int
main (int argc, char **argv)
{
	EContact *contact;
	EContactDate date, *dp;

	gnome_program_init("test-string", "0.0", LIBGNOME_MODULE, argc, argv, NULL);

	if (bonobo_init (&argc, argv) == FALSE)
		g_error ("Could not initialize Bonobo");

	contact = e_contact_new ();

	date.year = 1999;
	date.month = 3;
	date.day = 3;

	e_contact_set (contact, E_CONTACT_BIRTH_DATE, &date);

	printf ("vcard = \n%s\n", e_vcard_to_string (E_VCARD (contact), EVC_FORMAT_VCARD_30));

	dp = e_contact_get (contact, E_CONTACT_BIRTH_DATE);

	if (dp->year != date.year
	    || dp->month != date.month
	    || dp->day != date.day)
	  printf ("failed\n");
	else
	  printf ("passed\n");
}
