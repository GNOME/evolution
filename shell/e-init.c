/*
 * e-init.c: Initializes Evolution for first time users
 *
 */
#include <config.h>
#include <gnome.h>
#include "e-init.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include "e-util/e-gui-utils.h"

char *evolution_base_dir;

static void
e_init_local (void)
{
	evolution_base_dir = g_concat_dir_and_file (g_get_home_dir (), "Evolution");
	
	if (g_file_exists (evolution_base_dir))
		return;

	if (-1 == mkdir (evolution_base_dir, 0755)){
		e_notice (NULL, GNOME_MESSAGE_BOX_ERROR, _("Evolution can not create its local folders"));
		exit (0);
	}
}

void
e_init (void)
{
	e_init_local ();
}
