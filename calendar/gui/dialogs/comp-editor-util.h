/* Evolution calendar - Widget utilities
 *
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Author: JP Rosevear <jpr@ximian.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef _COMP_EDITOR_UTIL_H_
#define _COMP_EDITOR_UTIL_H_

#include <gtk/gtkwidget.h>
#include "comp-editor-page.h"

void comp_editor_dates (CompEditorPageDates *date, CalComponent *comp);
void comp_editor_date_label (CompEditorPageDates *dates, GtkWidget *label);

GtkWidget *comp_editor_new_date_edit (gboolean show_date, gboolean show_time);

#endif
