/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-summary-url.c
 *
 * Authors: Iain Holmes <iain@helixcode.com>
 *
 * Copyright (C) 2000  Helix Code, Inc.
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
 */

#include <e-summary-util.h>

#include <gnome.h>

char *
e_pixmap_file (const char *filename)
{
	char *ret;
	char *edir;

	if (g_file_exists (filename)) {
		ret = g_strdup (filename);

		return ret;
	}

	/* Try the evolution images dir */
	edir = g_concat_dir_and_file (EVOLUTION_DATADIR "/images/evolution",
				      filename);

	if (g_file_exists (edir)) {
		ret = g_strdup (edir);
		g_free (edir);

		return ret;
	}
	g_free (edir);

	/* Try the evolution button images dir */
	edir = g_concat_dir_and_file (EVOLUTION_DATADIR "/images/evolution/buttons",
				      filename);

	if (g_file_exists (edir)) {
		ret = g_strdup (edir);
		g_free (edir);
		
		return ret;
	}
	g_free (edir);

	/* Fall back to the gnome_pixmap_file */
	return gnome_pixmap_file (filename);
}
	
