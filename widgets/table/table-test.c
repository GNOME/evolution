/*
 * Test code for the ETable package
 *
 * Author:
 *   Miguel de Icaza (miguel@gnu.org)
 */
#include <config.h>
#include <stdio.h>
#include <string.h>
#include <gnome.h>

int
main (int argc, char *argv [])
{

	if (isatty (0)){
		printf ("you have to provide data on standard input\n");
		exit (1);
	}
	
	gnome_init ("TableTest", "TableTest", argc, argv);
	e_cursors_init ();

	table_browser_test ();
	multi_cols_test ();
	check_test ();
	
	gtk_main ();

	e_cursors_shutdown ();
	return 0;
}
