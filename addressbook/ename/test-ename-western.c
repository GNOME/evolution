#include <gnome.h>
#include <ctype.h>

#include <ename/e-name-western.h>

static void
do_name (char *n)
{
	ENameWestern *wname;

	wname = e_name_western_parse (n);

	printf ("Full Name: [%s]\n", n);

	printf ("Prefix: [%s]\n", wname->prefix);
	printf ("First:  [%s]\n", wname->first);
	printf ("Middle: [%s]\n", wname->middle);
	printf ("Nick:   [%s]\n", wname->nick);
	printf ("Last:   [%s]\n", wname->last);
	printf ("Suffix: [%s]\n", wname->suffix);

	printf ("\n");

	e_name_western_free (wname);
}

int
main (int argc, char **argv)
{
	if (argc == 2) {
		while (! feof (stdin)) {
			char s[256];

			if (fgets (s, sizeof (s), stdin) == NULL)
				return 0;

			g_strstrip (s);

			do_name (s);
		}

		return 0;
	}

	do_name ("Nat");
	do_name ("Karl Anders Carlsson");
	do_name ("Miguel de Icaza Amozorrutia");
	do_name ("The Honorable Doctor de Icaza, Miguel \"Sparky\" Junior, PhD, MD");
	do_name ("Nat Friedman MD, Phd");
	do_name ("Nat Friedman PhD");
	do_name ("Friedman, Nat");
	do_name ("Miguel de Icaza Esquire");
	do_name ("Dr Miguel \"Sparky\" de Icaza");
	do_name ("Robert H.B. Netzer");
	do_name ("W. Richard Stevens");
	do_name ("Nat Friedman");
	do_name ("N. Friedman");
	do_name ("Miguel de Icaza");
	do_name ("Drew Johnson");
	do_name ("President Bill \"Slick Willy\" Clinton");
	do_name ("The Honorable Mark J. Einstein Jr");
	do_name ("Friedman, Nat");
	do_name ("de Icaza, Miguel");
	do_name ("Mr de Icaza, Miguel");
	do_name ("Smith, John Jr");
	do_name ("Nick Glennie-Smith");
	do_name ("Dr von Johnson, Albert Roderick Jr");

	return 0;
}
