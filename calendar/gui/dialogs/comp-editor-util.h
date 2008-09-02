/*
 * Evolution calendar - Widget utilities
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
 *
 * Authors:
 *		JP Rosevear <jpr@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _COMP_EDITOR_UTIL_H_
#define _COMP_EDITOR_UTIL_H_

#include <gtk/gtk.h>
#include "comp-editor-page.h"

void comp_editor_dates (CompEditorPageDates *date, ECalComponent *comp);
void comp_editor_free_dates (CompEditorPageDates *dates);

void comp_editor_date_label (CompEditorPageDates *dates, GtkWidget *label);

GtkWidget *comp_editor_new_date_edit (gboolean show_date, gboolean show_time,
				      gboolean make_time_insensitive);

struct tm comp_editor_get_current_time (GtkObject *object, gpointer data);


char *comp_editor_strip_categories (const char *categories);

#endif
