#include <ctype.h>
#include <stdio.h>
#include <glib.h>
#include <gtk/gtkmain.h>
#include <ename/e-name-western.h>


static void
do_name (char *n)
{
	ENameWestern *wname;

	wname = e_name_western_parse (n);

	printf ("Full Name: [%s]\n", n);

	printf ("Prefix: [%s]\n", wname->prefix ? wname->prefix : "");
	printf ("First:  [%s]\n", wname->first ? wname->first : "");
	printf ("Middle: [%s]\n", wname->middle ? wname->middle : "");
	printf ("Nick:   [%s]\n", wname->nick ? wname->nick : "");
	printf ("Last:   [%s]\n", wname->last ? wname->last : "");
	printf ("Suffix: [%s]\n", wname->suffix ? wname->suffix : "");

	printf ("\n");

	e_name_western_free (wname);
}

int
main (int argc, char **argv)
{
	GString *str;
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

	/* create a name of the form:

	   <Prefix> <First name> <Nickname> <Middle> <Last name> <Suffix>

	   composed almost entirely of multibyte utf8 sequences.
	*/
	str = g_string_new ("Dr. ");

	str = g_string_append_unichar (str, 0x5341);
	str = g_string_append_unichar (str, 0x57CE);
	str = g_string_append_unichar (str, 0x76EE);

	str = g_string_append (str, " \"");
	str = g_string_append_unichar (str, 0x5341);
	str = g_string_append_unichar (str, 0x5341);
	str = g_string_append (str, "\" ");

	str = g_string_append_unichar (str, 0x5341);
	str = g_string_append_unichar (str, 0x76EE);

	str = g_string_append (str, " ");

	str = g_string_append_unichar (str, 0x76EE);
	str = g_string_append_unichar (str, 0x76EE);
	str = g_string_append (str, ", Esquire");

	do_name (str->str);

	str = g_string_assign (str, "");

	/* Now try a utf8 sequence of the form:

	   Prefix Last, First Middle Suffix
	*/

	str = g_string_new ("Dr. ");

	/* last */
	str = g_string_append_unichar (str, 0x5341);
	str = g_string_append_unichar (str, 0x57CE);
	str = g_string_append_unichar (str, 0x76EE);

	str = g_string_append (str, ", ");

	/* first */
	str = g_string_append_unichar (str, 0x5341);
	str = g_string_append_unichar (str, 0x76EE);
	str = g_string_append_unichar (str, 0x57CE);

	str = g_string_append (str, " ");

	/* middle */
	str = g_string_append_unichar (str, 0x5341);
	str = g_string_append_unichar (str, 0x76EE);
	str = g_string_append_unichar (str, 0x76EE);
	str = g_string_append_unichar (str, 0x76EE);

	str = g_string_append (str, ", Esquire");

	do_name (str->str);
	
	return 0;
}
