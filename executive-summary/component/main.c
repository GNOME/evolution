/* 
 * main.c: The core of the executive summary component.
 *
 * Copyright (C) 2000 Helix Code, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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
 * Author: Iain Holmes  <iain@helixcode.com>
 */

#include <config.h>

#include <signal.h>

#include <gnome.h>
#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-object-directory.h>
#include <liboaf/liboaf.h>

#ifdef GTKHTML_HAVE_GCONF
#include <gconf/gconf.h>
#endif

#include "gal/widgets/e-gui-utils.h"
#include "gal/widgets/e-cursors.h"
#include "gal/widgets/e-unicode.h"

#include "component-factory.h"

int
main (int argc,
      char **argv)
{
  CORBA_ORB orb;

  bindtextdomain (PACKAGE, EVOLUTION_LOCALEDIR);
  textdomain (PACKAGE);

  gnome_init_with_popt_table ("evolution-executive-summary", VERSION,
			      argc, argv, oaf_popt_options, 0, NULL);
  orb = oaf_init (argc, argv);

  gdk_rgb_init ();
  if (bonobo_init (orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL) == FALSE) {
    g_error (_("Executive summary component could not initialize Bonobo.\n"
	       "If there was a warning message about the "
	       "RootPOA, it probably means\nyou compiled "
	       "Bonobo against GOAD instead of OAF."));
  }

#ifdef GTKHTML_HAVE_GCONF
  gconf_init (argc, argv, NULL);
#endif

  e_unicode_init ();

  e_cursors_init ();

  component_factory_init ();

  signal (SIGSEGV, SIG_DFL);
  signal (SIGBUS, SIG_DFL);

  bonobo_main ();

  return 0;
}
