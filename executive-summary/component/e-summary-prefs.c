/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-summary-prefs.c: Preference handling routines.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-config.h>
#include "e-summary-prefs.h"
#include "e-summary.h"

void
e_summary_prefs_free (ESummaryPrefs *prefs)
{
	g_return_if_fail (prefs != NULL);

	g_free (prefs->page);
	g_free (prefs);
}

ESummaryPrefs *
e_summary_prefs_new (void)
{
	ESummaryPrefs *prefs;

	prefs = g_new0 (ESummaryPrefs, 1);
	return prefs;
}

ESummaryPrefs *
e_summary_prefs_copy (ESummaryPrefs *prefs)
{
	ESummaryPrefs *copy;

	g_return_val_if_fail (prefs != NULL, NULL);

	copy = e_summary_prefs_new ();
	copy->page = g_strdup (prefs->page);
	copy->columns = prefs->columns;

	return copy;
}

gboolean
e_summary_prefs_compare (ESummaryPrefs *p1,
			 ESummaryPrefs *p2)
{
	if (p1 == p2)
		return TRUE;

	if (strcmp (p1->page, p2->page) == 0)
		return TRUE;

	if (p1->columns == p2->columns)
		return TRUE;

	return FALSE;
}

ESummaryPrefs *
e_summary_prefs_load (const char *path)
{
	ESummaryPrefs *prefs;
	char *item;
	
	g_return_val_if_fail (path != NULL, NULL);
	g_return_val_if_fail (*path != '\0', NULL);

	prefs = e_summary_prefs_new ();
	
	item = g_strdup_printf ("=%s/e-summary=/executive-summary/page", path);
	prefs->page = gnome_config_get_string (item);
	g_free (item);

	item = g_strdup_printf ("=%s/e-summary=/executive-summary/columns=3", path);
	prefs->columns = gnome_config_get_int (item);
	g_free (item);
	return prefs;
}

void
e_summary_prefs_save (ESummaryPrefs *prefs,
		      const char *path)
{
	char *item;

	g_return_if_fail (prefs != NULL);
	g_return_if_fail (path != NULL);
	g_return_if_fail (*path != '\0');

	item = g_strdup_printf ("=%s/e-summary=/executive-summary/page", path);
	gnome_config_set_string (item, prefs->page);
	g_free (item);

	item = g_strdup_printf ("=%s/e-summary=/executive-summary/columns", path);
	gnome_config_set_int (item, prefs->columns);
	g_free (item);

	gnome_config_sync ();
	gnome_config_drop_all ();
}
