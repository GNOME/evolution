/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-shell-component-utils.c
 *
 * Copyright (C) 2000, 2001 Ximian, Inc.
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "evolution-shell-component-utils.h"

#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-util.h>
#include <bonobo/bonobo-ui-util.h>

static void free_pixmaps (void);
static GSList *inited_arrays = NULL;

void e_pixmaps_update (BonoboUIComponent *uic, EPixmap *pixcache)
{
	static int done_init = 0;
	int i;

	if (!done_init) {
		g_atexit (free_pixmaps);
		done_init = 1;
	}

	if (g_slist_find (inited_arrays, pixcache) == NULL)
		inited_arrays = g_slist_prepend (inited_arrays, pixcache);

	for (i = 0; pixcache [i].path; i++) {
		if (!pixcache [i].pixbuf) {
			char *path;
			GdkPixbuf *pixbuf;

			path = g_concat_dir_and_file (EVOLUTION_IMAGES,
						      pixcache [i].fname);

			pixbuf = gdk_pixbuf_new_from_file (path);
			if (pixbuf == NULL) {
				g_warning ("Cannot load image -- %s", path);
			} else {
				pixcache [i].pixbuf = bonobo_ui_util_pixbuf_to_xml (pixbuf);
				gdk_pixbuf_unref (pixbuf);
			}

			g_free (path);
		}
		bonobo_ui_component_set_prop (uic, pixcache [i].path,
					      "pixname", pixcache [i].pixbuf,
					      NULL);
	}
}

static void
free_pixmaps (void)
{
	int i;
	GSList *li;

	for (li = inited_arrays; li != NULL; li = li->next) {
		EPixmap *pixcache = li->data;
		for (i = 0; pixcache [i].path; i++)
			g_free (pixcache [i].pixbuf);
	}

	g_slist_free (inited_arrays);
}

