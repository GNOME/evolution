/*
 * Test code for the ETable package
 *
 * Author:
 *   Miguel de Icaza (miguel@gnu.org)
 */
#include <config.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <gnome.h>
#include "e-util/e-cursors.h"
#include "table-test.h"

int
main (int argc, char *argv [])
{

	if (isatty (0)){
		int fd;
		
		close (0);
		fd = open ("sample.table", O_RDONLY);
		if (fd == -1){
			fprintf (stderr, "Could not find sample.table, try feeding a table on stdin");
			exit (1);
		}
		dup2 (fd, 0);
	}
	
	gnome_init ("TableTest", "TableTest", argc, argv);
	e_cursors_init ();

	table_browser_test ();
	multi_cols_test ();
	check_test ();
	e_table_test ();
	
	gtk_main ();

	e_cursors_shutdown ();
	return 0;
}
