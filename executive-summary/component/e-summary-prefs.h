/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-summary-prefs.h: Preference handling routines.
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

#ifndef __E_SUMMARY_PREFS_H__
#define __E_SUMMARY_PREFS_H__

typedef struct _ESummaryPrefs ESummaryPrefs;
struct _ESummaryPrefs {
	char *page; /* Background HTML page URL */
	int columns; /* Number of components per row (Default = 3) */

	/* If anything is added here, don't forget to add 
	   copy, compare, load and save routines to the appropriate
	   functions. */
};

ESummaryPrefs *e_summary_prefs_new (void);
void e_summary_prefs_free (ESummaryPrefs *prefs);
ESummaryPrefs *e_summary_prefs_copy (ESummaryPrefs *prefs);
gboolean e_summary_prefs_compare (ESummaryPrefs *p1,
				  ESummaryPrefs *p2);
ESummaryPrefs *e_summary_prefs_load (const char *path);
void e_summary_prefs_save (ESummaryPrefs *prefs,
			   const char *path);

#endif
