#include <gnome.h>
#include "e-charset-picker.h"

int
main (int argc, char **argv)
{
	char *charset;

	gnome_init ("test-charset-picker", "1.0", argc, argv);

	charset = e_charset_picker_dialog ("test-charset-picker",
					   "Pick a charset, any charset",
					   NULL, NULL);
	if (charset)
		printf ("You picked: %s\n", charset);

	return 0;
}
