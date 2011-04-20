/*
 * e-shell-meego.c
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * Inspired by mx's mx-application.c by
 *	Thomas Wood <thomas.wood@intel.com>,
 *	Chris Lord  <chris@linux.intel.com>
 */

#include <glib.h>
#include <e-shell-meego.h>

#ifndef G_OS_WIN32
#include <gdk/gdkx.h>
#include <X11/Xatom.h>
#endif

#ifdef G_OS_WIN32
void
e_shell_detect_meego (gboolean *is_meego,
                      gboolean *small_screen)
{
	*is_meego = *small_screen = FALSE;
}
#else
void
e_shell_detect_meego (gboolean *is_meego,
                      gboolean *small_screen)
{
	Window *wm_window_v = NULL;
	guchar *moblin_string = NULL;
	GModule *module = NULL;
	/*
	 * Wow - this is unpleasant, but it is hard to link directly
	 * to the X libraries, and we have to use XGetWindowProperty
	 * to get to the (mind-mashed) 'supporting' window.
	 */
	struct {
	  gint (*XFree) (gpointer);
	  gint (*XGetWindowProperty) (Display*, XID, Atom, long, long, Bool,
				     Atom, Atom *, gint *, unsigned long*,
				     unsigned long*, guchar **);
	} fns = { 0, 0 };

	*is_meego = *small_screen = FALSE;

	moblin_string = (guchar *)g_getenv ("EVO_MEEGO");
	if (!moblin_string) {
		GdkScreen *screen;
		GdkDisplay *display;
		GdkAtom wm_win, mob_atom;
		Atom dummy_t;
		unsigned long dummy_l;
		gint dummy_i;

		if (!gdk_display_get_default ())
			return;

		wm_win = gdk_atom_intern ("_NET_SUPPORTING_WM_CHECK", TRUE);
		mob_atom = gdk_atom_intern ("_MEEGO", TRUE);
		if (!wm_win || !mob_atom)
			return;

		module = g_module_open (NULL, 0);
		if (!module)
			return;
		g_module_symbol (module, "XFree", (gpointer) &fns.XFree);
		g_module_symbol (module, "XGetWindowProperty",
				 (gpointer) &fns.XGetWindowProperty);
		if (!fns.XFree || !fns.XGetWindowProperty) {
			fprintf (stderr, "defective X server\n");
			goto exit;
		}

		display = gdk_display_get_default ();
		screen = gdk_display_get_default_screen (gdk_display_get_default ());

		gdk_error_trap_push ();

		/* get the window manager's supporting window */
		fns.XGetWindowProperty (gdk_x11_display_get_xdisplay (display),
					GDK_WINDOW_XID (gdk_screen_get_root_window (screen)),
					gdk_x11_atom_to_xatom_for_display (display, wm_win),
					0, 1, False, XA_WINDOW, &dummy_t, &dummy_i,
					&dummy_l, &dummy_l, (guchar **)(&wm_window_v));

		/* get the '_Moblin' setting */
		if (wm_window_v && (*wm_window_v != None))
			fns.XGetWindowProperty (gdk_x11_display_get_xdisplay (display), *wm_window_v,
						gdk_x11_atom_to_xatom_for_display (display, mob_atom),
						0, 8192, False, XA_STRING,
						&dummy_t, &dummy_i, &dummy_l, &dummy_l,
						&moblin_string);

		gdk_error_trap_pop ();
	}

	if (moblin_string) {
		gint i;
		gchar **props;

		g_warning ("prop '%s'", moblin_string);

		/* use meego theming tweaks */
		*is_meego = TRUE;

		props = g_strsplit ((gchar *)moblin_string, ":", -1);
		for (i = 0; props && props[i]; i++) {
			gchar **pair = g_strsplit (props[i], "=", 2);

			g_warning ("pair '%s'='%s'", pair ? pair[0] : "<null>",
				   pair && pair[0] ? pair[1] : "<null>");

			/* Hunt for session-type=small-screen */
			if (pair && pair[0] && !g_ascii_strcasecmp (pair[0], "session-type"))
				*small_screen = !g_ascii_strcasecmp (pair[1], "small-screen");
			g_strfreev (pair);
		}
		g_strfreev (props);
		if (fns.XFree)
			fns.XFree (moblin_string);
	}

 exit:
	if (wm_window_v)
		fns.XFree (wm_window_v);
	if (module)
		g_module_close (module);
}
#endif

#ifdef TEST_APP
/* gcc -g -O0 -Wall -I. -DTEST_APP `pkg-config --cflags --libs gtk+-2.0`
 *     e-shell-meego.c && ./a.out */
#include <gtk/gtk.h>

gint main (gint argc, gchar **argv)
{
	gboolean is_meego, small_screen;

	gtk_init (&argc, &argv);

	e_shell_detect_meego (&is_meego, &small_screen);
	fprintf (stderr, "Meego ? %d small ? %d\n", is_meego, small_screen);

	return 0;
}
#endif
