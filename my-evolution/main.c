/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* main.c
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Iain Holmes
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <glib.h>
#include <gdk/gdk.h>
#include <gdk/gdkrgb.h>

#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-ui-init.h>

#include <bonobo/bonobo-main.h>

#include <e-util/e-proxy.h>

#include <glade/glade.h>

#include <gconf/gconf.h>

#include <gal/widgets/e-cursors.h>

#include "component-factory.h"


int
main (int argc,
      char **argv)
{
	/* Make ElectricFence work.  */
	free (malloc (10));

	bindtextdomain (PACKAGE, EVOLUTION_LOCALEDIR);
	textdomain (PACKAGE);

	gnome_program_init (PACKAGE, VERSION, LIBGNOMEUI_MODULE, argc, argv, 
			    GNOME_PROGRAM_STANDARD_PROPERTIES,
			    GNOME_PARAM_HUMAN_READABLE_NAME, _("Evolution Summary"),
			    NULL);

	e_cursors_init ();

	/* Start our component */
	component_factory_init ();

	bonobo_main ();

	return 0;
}
