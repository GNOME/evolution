
#include "ebook/e-book.h"
#include <libgnome/gnome-init.h>
#include <bonobo/bonobo-main.h>
#include <stdlib.h>

const char *photo_data = "/9j/4AAQSkZJRgABAQEARwBHAAD//gAXQ3JlYXRlZCB3aXRo\
IFRoZSBHSU1Q/9sAQwAIBgYHBgUIBwcHCQkICgwUDQwLCwwZEhMPFB0aHx4dGhwcICQuJyAiLC\
McHCg3KSwwMTQ0NB8nOT04MjwuMzQy/9sAQwEJCQkMCwwYDQ0YMiEcITIyMjIyMjIyMjIyMjIy\
MjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIy/8AAEQgAMgAyAwEiAAIRAQMRAf\
/EABsAAQACAwEBAAAAAAAAAAAAAAAHCAQFBgID/8QAMBAAAgEDAQYEBQQDAAAAAAAAAQIDAAQR\
BQYSEyExQQdhcYEiI0JRkRQVMqFiguH/xAAaAQADAQEBAQAAAAAAAAAAAAAABAUCBgED/8QAIx\
EAAgICAQQCAwAAAAAAAAAAAAECAwQRQRITITEUYQUiUf/aAAwDAQACEQMRAD8An+sHUtWtNKjV\
rmQ7754cajLvjrgfbzPIdzWdVfds9pJb3XdQkMrcFZGj+HqY0bdVV9Tz/wBia+N9vbjvkaxMb5\
E9N6SJB1HxLEEjJaWsUjD6QzSMPXdGB7E1zV74t63HINy1s4F7CWCTn77wrA0TY86jY3N1qsUk\
6wxBxBDvYjLHkoUH4j3JP/a0V3s1CvF/QM9tKpw0THeU+TLkj8VLnmzT8y0n9FujBx5bioba/r\
ZLWx3iPZ7RzLp95GtnqRGVTezHNjruH7/4n+67iqpq7Qi3uYWMMsNynfnE6sM8/Lr6VamFi0KM\
epUE1Sx7XZHbI+fjxos1H0z3SlKYEjzISI2I64OKqsyu8sck2QYrmPjBvpIYg598Vauoh8VtlY\
7JW2isoBwpPl6hGByZTyD+o6E+h7UtlVOcPHA/+PyI1Wal6Zp7vaC/06wnTTLtEeUDiKwzu4H8\
vI9AM9Tiuctkng1Nnk1G5cOoYifB4nI/jB7VjWuoT21qPmwXUCHKlphHKvqG5N6g0/cLi/Rg88\
FhbkbxlaUSu3kqpnn6kDzqGqbNdPB0XyK4/svZr9RVntL50GePdcKEDqzhVBx7sKtPpayppNos\
xzKIlDHzxUFeG2zo2n2kivWhK6PpHwwoTnfk65J7kZyT9z5VYADAwKuYtfRA5zPv7tnjgUpSmR\
EV8bq1hvbWW1uY1khlUo6MMhgeor7UoAje18FtmLe9eeQT3EXPcglkJRPbv71EWu7Dajp2o3MG\
mlRCkjKQ30jPUe1WlrlNW0RptTleNB84DnjkD0P9VlxT4Nqck9pmn8JuFp2zo0cgCWFi2e7555\
/NSHXLadso2m3sU0NxlV65HM+VdTW3rgwvsUpSvAFKUoAUxSlAClKUAKUpQB//2Q==";


int
main (int argc, char **argv)
{
	EContact *contact;
	EContactPhoto *photo, *new_photo;

	gnome_program_init("test-photo", "0.0", LIBGNOME_MODULE, argc, argv, NULL);

	if (bonobo_init (&argc, argv) == FALSE)
		g_error ("Could not initialize Bonobo");

	contact = e_contact_new ();

	photo = g_new (EContactPhoto, 1);
	photo->data = g_strdup (photo_data);
	photo->length = _evc_base64_decode_simple (photo->data, strlen (photo_data));

	/* set the photo */
	e_contact_set (contact, E_CONTACT_PHOTO, photo);

	/* then get the photo */
	new_photo = e_contact_get (contact, E_CONTACT_PHOTO);
	
	/* and compare */
	if (new_photo->length != photo->length)
	  g_error ("photo lengths differ");
	 
	if (memcmp (new_photo->data, photo->data, photo->length))
	  g_error ("photo data differs");

	printf ("photo test passed\n");
}
